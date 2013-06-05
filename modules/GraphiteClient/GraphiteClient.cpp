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
#include "stdafx.h"
#include "GraphiteClient.h"

#include <utils.h>
#include <strEx.h>

#include <settings/client/settings_client.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>

namespace sh = nscapi::settings_helper;

const std::string command_prefix("graphite");
const std::string default_command("submit");
/**
 * Default c-tor
 * @return 
 */
GraphiteClient::GraphiteClient() {}

/**
 * Default d-tor
 * @return 
 */
GraphiteClient::~GraphiteClient() {}

bool GraphiteClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {

	try {

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias("graphite", alias, "client");
		target_path = settings.alias().get_settings_path("targets");

		settings.alias().add_path_to_settings()
			("GRAPHITE CLIENT SECTION", "Section for graphite passive check module.")

			("handlers", sh::fun_values_path(boost::bind(&GraphiteClient::add_command, this, _1, _2)), 
			"CLIENT HANDLER SECTION", "")

			("targets", sh::fun_values_path(boost::bind(&GraphiteClient::add_target, this, _1, _2)), 
			"REMOTE TARGET DEFINITIONS", "")
			;

		settings.alias().add_key_to_settings()
			("hostname", sh::string_key(&hostname_, "auto"),
			"HOSTNAME", "The host name of this host if set to blank (default) the windows name of the computer will be used.")

			("channel", sh::string_key(&channel_, "GRAPHITE"),
			"CHANNEL", "The channel to listen to.")
			;

		settings.register_all();
		settings.notify();

		targets.add_samples(get_settings_proxy(), target_path);
		targets.add_missing(get_settings_proxy(), target_path, "default", "", true);
		nscapi::core_helper::core_proxy core(get_core(), get_id());
		core.register_channel(channel_);

		if (hostname_ == "auto") {
			hostname_ = boost::asio::ip::host_name();
		} else {
			std::pair<std::string,std::string> dn = strEx::split<std::string>(boost::asio::ip::host_name(), ".");

			try {
				boost::asio::io_service svc;
				boost::asio::ip::tcp::resolver resolver (svc);
				boost::asio::ip::tcp::resolver::query query (boost::asio::ip::host_name(), "");
				boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve (query), end;

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
		}


	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_EXR("NSClient API exception: ", e);
		return false;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_EXR("NSClient API exception: ", e);
		return false;
	} catch (...) {
		NSC_LOG_ERROR_EX("NSClient API exception: ");
		return false;
	}
	return true;
}

std::string get_command(std::string alias, std::string command = "") {
	if (!alias.empty())
		return alias; 
	if (!command.empty())
		return command; 
	return "host_check";
}

//////////////////////////////////////////////////////////////////////////
// Settings helpers
//

void GraphiteClient::add_target(std::string key, std::string arg) {
	try {
		targets.add(get_settings_proxy(), target_path , key, arg);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add: " + key);
	}
}

void GraphiteClient::add_command(std::string name, std::string args) {
	try {
		nscapi::core_helper::core_proxy core(get_core(), get_id());
		std::string key = commands.add_command(name, args);
		if (!key.empty())
			core.register_command(key.c_str(), "Graphite relay for: " + name);
	} catch (boost::program_options::validation_error &e) {
		NSC_LOG_ERROR_EXR("Failed to add: " + name, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add: " + name);
	}
}

/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool GraphiteClient::unloadModule() {
	return true;
}

void GraphiteClient::query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message) {
	client::configuration config(command_prefix);
	setup(config, request_message.header());
	commands.parse_query(command_prefix, default_command, request.command(), config, request, *response, request_message);
}

bool GraphiteClient::commandLineExec(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message) {
	client::configuration config(command_prefix);
	setup(config, request_message.header());
	return commands.parse_exec(command_prefix, default_command, request.command(), config, request, *response, request_message);
}

void GraphiteClient::handleNotification(const std::string &channel, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message) {
	client::configuration config(command_prefix);
	setup(config, request_message.header());
	commands.forward_submit(config, request_message, *response_message);
}

//////////////////////////////////////////////////////////////////////////
// Parser setup/Helpers
//

void GraphiteClient::add_local_options(po::options_description &desc, client::configuration::data_type data) {
	desc.add_options()
		("path", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "path", _1)), 
		"")

		("timeout", po::value<unsigned int>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_int_data, &data->recipient, "timeout", _1)), 
		"")

		;
}

void GraphiteClient::setup(client::configuration &config, const ::Plugin::Common_Header& header) {
	boost::shared_ptr<clp_handler_impl> handler(new clp_handler_impl(this));
	add_local_options(config.local, config.data);

	config.data->recipient.id = header.recipient_id();
	config.default_command = default_command;
	std::string recipient = config.data->recipient.id;
	if (!targets.has_object(recipient)) {
		recipient = "default";
	}
	nscapi::targets::optional_target_object opt = targets.find_object(recipient);

	if (opt) {
		nscapi::targets::target_object t = *opt;
		nscapi::protobuf::functions::destination_container def = t.to_destination_container();
		config.data->recipient.apply(def);
		}
	config.data->host_self.id = "self";
	config.data->host_self.address.host = hostname_;

	config.target_lookup = handler;
	config.handler = handler;
}

GraphiteClient::connection_data GraphiteClient::parse_header(const ::Plugin::Common_Header &header, client::configuration::data_type data) {
	nscapi::protobuf::functions::destination_container recipient, sender;
	nscapi::protobuf::functions::parse_destination(header, header.recipient_id(), recipient, true);
	nscapi::protobuf::functions::parse_destination(header, header.sender_id(), sender, true);
	return connection_data(recipient, data->recipient, sender);
}

//////////////////////////////////////////////////////////////////////////
// Parser implementations
//

int GraphiteClient::clp_handler_impl::query(client::configuration::data_type data, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
	NSC_LOG_ERROR_STD("GRAPHITE does not support query patterns");
	nscapi::protobuf::functions::set_response_bad(*response_message.add_payload(), "GRAPHITE does not support query patterns");
	return NSCAPI::hasFailed;
}

int GraphiteClient::clp_handler_impl::submit(client::configuration::data_type data, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage &response_message) {
	const ::Plugin::Common_Header& request_header = request_message.header();
	connection_data con = parse_header(request_header, data);
	std::string path = con.path;

	strEx::replace(path, "${hostname}", con.sender_hostname);

	nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

	std::list<g_data> list;
	for (int i=0;i < request_message.payload_size(); ++i) {
		Plugin::QueryResponseMessage::Response r =request_message.payload(i);
		std::string tmp_path = path;
		strEx::replace(tmp_path, "${check_alias}", r.alias());

		for (int j=0;j<r.perf_size();j++) {
			g_data d;
			::Plugin::Common::PerformanceData perf = r.perf(j);
			double value = 0.0;
			d.path = tmp_path;
			strEx::replace(d.path, "${perf_alias}", perf.alias());
			if (perf.has_float_value()) {
				if (perf.float_value().has_value())
					value = perf.float_value().value();
				else
					NSC_LOG_ERROR("Unsopported performance data (no value)");
			} else if (perf.has_int_value()) {
				if (perf.int_value().has_value())
					value = perf.int_value().value();
				else
					NSC_LOG_ERROR("Unsopported performance data (no value)");
			} else {
				NSC_LOG_ERROR("Unsopported performance data type: " + perf.alias());
				continue;
			}
			strEx::replace(d.path, " ", "_");
			d.value = strEx::s::xtos(value);
			list.push_back(d);
		}
	}

	boost::tuple<int,std::wstring> ret = instance->send(con, list);
	nscapi::protobuf::functions::append_simple_submit_response_payload(response_message.add_payload(), "TODO", ret.get<0>(), utf8::cvt<std::string>(ret.get<1>()));
	return NSCAPI::isSuccess;
}

int GraphiteClient::clp_handler_impl::exec(client::configuration::data_type data, const Plugin::ExecuteRequestMessage &request_message, Plugin::ExecuteResponseMessage &response_message) {
	NSC_LOG_ERROR_STD("GRAPHITE does not support exec patterns");
	nscapi::protobuf::functions::set_response_bad(*response_message.add_payload(), "GRAPHITE does not support query patterns");
	return NSCAPI::hasFailed;
}

//////////////////////////////////////////////////////////////////////////
// Protocol implementations
//

boost::tuple<int,std::wstring> GraphiteClient::send(connection_data data, const std::list<g_data> payload) {
	try {
		boost::asio::io_service io_service;
		boost::asio::ip::tcp::resolver resolver(io_service);
		boost::asio::ip::tcp::resolver::query query(data.host, data.port);
		boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
		boost::asio::ip::tcp::resolver::iterator end;

		boost::asio::ip::tcp::socket socket(io_service);
		boost::system::error_code error = boost::asio::error::host_not_found;
		while(error && endpoint_iterator != end) {
			socket.close();
			socket.connect(*endpoint_iterator++, error);
		}
		if(error)
			throw boost::system::system_error(error);

		boost::posix_time::ptime time_t_epoch(boost::gregorian::date(1970,1,1)); 
		boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
		boost::posix_time::time_duration diff = now - time_t_epoch;
		int x = diff.total_seconds();

		BOOST_FOREACH(const g_data &d, payload) {
			std::string msg = d.path + " " +d.value + " " + boost::lexical_cast<std::string>(x) + "\n";
			socket.send(boost::asio::buffer(msg));
		}
		//socket.shutdown();
		return boost::make_tuple(NSCAPI::returnUNKNOWN, _T(""));
	} catch (const std::runtime_error &e) {
		NSC_LOG_ERROR_EXR("Socket error", e);
		return boost::make_tuple(NSCAPI::returnUNKNOWN, _T("Socket error: ") + utf8::to_unicode(e.what()));
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("sending data", e);
		return boost::make_tuple(NSCAPI::returnUNKNOWN, _T("Error: ") + utf8::to_unicode(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_EX("sending data");
		return boost::make_tuple(NSCAPI::returnUNKNOWN, _T("Unknown error -- REPORT THIS!"));
	}
}
