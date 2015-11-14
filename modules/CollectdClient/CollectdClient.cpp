/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#include "CollectdClient.h"

#include <utils.h>
#include <strEx.h>

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

#include <boost/make_shared.hpp>

#include "collectd_client.hpp"
#include "collectd_handler.hpp"

/**
 * Default c-tor
 * @return
 */
CollectdClient::CollectdClient() : client_("nsca", boost::make_shared<collectd_client::collectd_client_handler>(), boost::make_shared<collectd_handler::options_reader_impl>()) {}

/**
 * Default d-tor
 * @return
 */
CollectdClient::~CollectdClient() {}

bool CollectdClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
	try {
		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias("NSCA", alias, "client");
		std::string target_path = settings.alias().get_settings_path("targets");

		client_.set_path(target_path);

		settings.alias().add_path_to_settings()
			("COLLECTD CLIENT SECTION", "Section for NSCA passive check module.")

			("targets", sh::fun_values_path(boost::bind(&CollectdClient::add_target, this, _1, _2)),
				"REMOTE TARGET DEFINITIONS", "",
				"TARGET", "For more configuration options add a dedicated section")
			;

		settings.alias().add_key_to_settings()
			("hostname", sh::string_key(&hostname_, "auto"),
				"HOSTNAME", "The host name of the monitored computer.\nSet this to auto (default) to use the windows name of the computer.\n\n"
				"auto\tHostname\n"
				"${host}\tHostname\n"
				"${host_lc}\nHostname in lowercase\n"
				"${host_uc}\tHostname in uppercase\n"
				"${domain}\tDomainname\n"
				"${domain_lc}\tDomainname in lowercase\n"
				"${domain_uc}\tDomainname in uppercase\n"
				)
			;

		settings.register_all();
		settings.notify();

		client_.finalize(get_settings_proxy());

		nscapi::core_helper core(get_core(), get_id());

		if (hostname_ == "auto") {
			hostname_ = boost::asio::ip::host_name();
		} else if (hostname_ == "auto-lc") {
			hostname_ = boost::asio::ip::host_name();
			std::transform(hostname_.begin(), hostname_.end(), hostname_.begin(), ::tolower);
		} else if (hostname_ == "auto-uc") {
			hostname_ = boost::asio::ip::host_name();
			std::transform(hostname_.begin(), hostname_.end(), hostname_.begin(), ::toupper);
		} else {
			strEx::s::token dn = strEx::s::getToken(boost::asio::ip::host_name(), '.');

			try {
				boost::asio::io_service svc;
				boost::asio::ip::tcp::resolver resolver(svc);
				boost::asio::ip::tcp::resolver::query query(boost::asio::ip::host_name(), "");
				boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve(query), end;

				std::string s;
				while (iter != end) {
					s += iter->host_name();
					s += " - ";
					s += iter->endpoint().address().to_string();
					iter++;
				}
			} catch (const std::exception& e) {
				NSC_LOG_ERROR_EXR("Failed to resolve: ", e);
			}
			strEx::replace(hostname_, "${host}", dn.first);
			strEx::replace(hostname_, "${domain}", dn.second);
			std::transform(dn.first.begin(), dn.first.end(), dn.first.begin(), ::toupper);
			std::transform(dn.second.begin(), dn.second.end(), dn.second.begin(), ::toupper);
			strEx::replace(hostname_, "${host_uc}", dn.first);
			strEx::replace(hostname_, "${domain_uc}", dn.second);
			std::transform(dn.first.begin(), dn.first.end(), dn.first.begin(), ::tolower);
			std::transform(dn.second.begin(), dn.second.end(), dn.second.begin(), ::tolower);
			strEx::replace(hostname_, "${host_lc}", dn.first);
			strEx::replace(hostname_, "${domain_lc}", dn.second);
		}
	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_EXR("NSClient API exception: ", e);
		return false;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_EXR("loading", e);
		return false;
	} catch (...) {
		NSC_LOG_ERROR_EX("loading");
		return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
// Settings helpers
//

void CollectdClient::add_target(std::string key, std::string arg) {
	try {
		client_.add_target(get_settings_proxy(), key, arg);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add target: " + key);
	}
}

/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool CollectdClient::unloadModule() {
	client_.clear();
	return true;
}

const short multicast_port = 25826;
const int max_message_count = 10;

class sender {
	std::string payload;
public:
	sender(boost::asio::io_service& io_service,
		const boost::asio::ip::address& multicast_address, const std::string data)
		: endpoint_(multicast_address, multicast_port),
		socket_(io_service, boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string("192.168.0.201"), 25826)),
		timer_(io_service)
		, payload(data) {
		socket_.async_send_to(
			boost::asio::buffer(payload), endpoint_,
			boost::bind(&sender::handle_send_to, this,
				boost::asio::placeholders::error));
	}

	void handle_send_to(const boost::system::error_code& error) {}

	void handle_timeout(const boost::system::error_code& error) {
		if (!error) {
			socket_.async_send_to(
				boost::asio::buffer(message_), endpoint_,
				boost::bind(&sender::handle_send_to, this,
					boost::asio::placeholders::error));
		}
	}

private:
	boost::asio::ip::udp::endpoint endpoint_;
	boost::asio::ip::udp::socket socket_;
	boost::asio::deadline_timer timer_;
	int message_count_;
	std::string message_;
};

void CollectdClient::submitMetrics(const Plugin::MetricsMessage &response) {
	collectd::packet p;
	p.add_host("my-machine");

	boost::posix_time::ptime const time_epoch(boost::gregorian::date(1970, 1, 1));

	unsigned long long ms = (boost::posix_time::microsec_clock::local_time() - time_epoch).total_seconds();
	std::cout << "microseconds: " << ms << "\n";
	std::cout << "microseconds: " << (ms << 30) << "\n";

	p.add_time_hr(ms << 30);
	p.add_interval_hr(300 << 30);
	p.add_plugin("memory");
	//p.add_plugin_instance("pool_nonpaged");
	p.add_type("memory");
	p.add_type_instance("pool_nonpaged");

	collectd::collectd_value_list values;
	values.push_back(collectd::collectd_value::mk_gague(18137714688));

	p.add_value(values);

	try {
		boost::asio::io_service io_service;

		boost::asio::ip::udp::resolver resolver(io_service);
		boost::asio::ip::udp::resolver::query query("239.192.74.66", "25826", boost::asio::ip::resolver_query_base::numeric_service);
		boost::asio::ip::udp::resolver::iterator endpoint_iterator = resolver.resolve(query);
		boost::asio::ip::udp::resolver::iterator end;

		boost::system::error_code error = boost::asio::error::host_not_found;
		while (endpoint_iterator != end) {
			std::cout << endpoint_iterator->host_name() << std::endl;

			endpoint_iterator++;
		}

		sender s(io_service, boost::asio::ip::address::from_string("239.192.74.66"), p.get_buffer());
		io_service.run();
	} catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << "\n";
	}

	BOOST_FOREACH(const Plugin::MetricsMessage::Response &p, response.payload()) {
		BOOST_FOREACH(const Plugin::Common::MetricsBundle &b, p.bundles()) {
			//build_metrics(metrics, b);
		}
	}
}