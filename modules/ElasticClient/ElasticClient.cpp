/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ElasticClient.h"

#include <json_spirit.h>

#include "elastic_handler.hpp"

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

#include <str/utils.hpp>
#include <str/format.hpp>

#include <boost/algorithm/string/replace.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

/**
 * Default c-tor
 * @return
 */
ElasticClient::ElasticClient() 
{}

/**
 * Default d-tor
 * @return
 */
ElasticClient::~ElasticClient() {}

#include <Client.hpp>


bool ElasticClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
	try {
// 		sh::settings_registry settings(get_settings_proxy());
// 		settings.set_alias("graphite", alias, "client");
// 		client_.set_path(settings.alias().get_settings_path("targets"));
// 
// 		settings.alias().add_path_to_settings()
// 			("GRAPHITE CLIENT SECTION", "Section for graphite passive check module.")
// 
// 			("handlers", sh::fun_values_path(boost::bind(&ElasticClient::add_command, this, _1, _2)),
// 				"CLIENT HANDLER SECTION", "",
// 				"CLIENT HANDLER", "For more configuration options add a dedicated section")
// 
// 			("targets", sh::fun_values_path(boost::bind(&ElasticClient::add_target, this, _1, _2)),
// 				"REMOTE TARGET DEFINITIONS", "",
// 				"TARGET", "For more configuration options add a dedicated section")
// 			;
// 
// 		settings.alias().add_key_to_settings()
// 			("hostname", sh::string_key(&hostname_, "auto"),
// 				"HOSTNAME", "The host name of the monitored computer.\nSet this to auto (default) to use the windows name of the computer.\n\n"
// 				"auto\tHostname\n"
// 				"${host}\tHostname\n"
// 				"${host_lc}\nHostname in lowercase\n"
// 				"${host_uc}\tHostname in uppercase\n"
// 				"${domain}\tDomainname\n"
// 				"${domain_lc}\tDomainname in lowercase\n"
// 				"${domain_uc}\tDomainname in uppercase\n"
// 				)
// 
// 			("channel", sh::string_key(&channel_, "GRAPHITE"),
// 				"CHANNEL", "The channel to listen to.")
// 			;
// 
// 		settings.register_all();
// 		settings.notify();
// 
// 		client_.finalize(get_settings_proxy());
// 
// 		nscapi::core_helper core(get_core(), get_id());
// 		core.register_channel(channel_);
// 
// 		if (hostname_ == "auto") {
// 			hostname_ = boost::asio::ip::host_name();
// 		} else if (hostname_ == "auto-lc") {
// 			hostname_ = boost::asio::ip::host_name();
// 			std::transform(hostname_.begin(), hostname_.end(), hostname_.begin(), ::tolower);
// 		} else if (hostname_ == "auto-uc") {
// 			hostname_ = boost::asio::ip::host_name();
// 			std::transform(hostname_.begin(), hostname_.end(), hostname_.begin(), ::toupper);
// 		} else {
// 			str::utils::token dn = str::utils::getToken(boost::asio::ip::host_name(), '.');
// 
// 			try {
// 				boost::asio::io_service svc;
// 				boost::asio::ip::tcp::resolver resolver(svc);
// 				boost::asio::ip::tcp::resolver::query query(boost::asio::ip::host_name(), "");
// 				boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve(query), end;
// 
// 				std::string s;
// 				while (iter != end) {
// 					s += iter->host_name();
// 					s += " - ";
// 					s += iter->endpoint().address().to_string();
// 					iter++;
// 				}
// 			} catch (const std::exception& e) {
// 				NSC_LOG_ERROR_EXR("Failed to resolve: ", e);
// 			}
// 
// 			str::utils::replace(hostname_, "${host}", dn.first);
// 			str::utils::replace(hostname_, "${domain}", dn.second);
// 			std::transform(dn.first.begin(), dn.first.end(), dn.first.begin(), ::toupper);
// 			std::transform(dn.second.begin(), dn.second.end(), dn.second.begin(), ::toupper);
// 			str::utils::replace(hostname_, "${host_uc}", dn.first);
// 			str::utils::replace(hostname_, "${domain_uc}", dn.second);
// 			std::transform(dn.first.begin(), dn.first.end(), dn.first.begin(), ::tolower);
// 			std::transform(dn.second.begin(), dn.second.end(), dn.second.begin(), ::tolower);
// 			str::utils::replace(hostname_, "${host_lc}", dn.first);
// 			str::utils::replace(hostname_, "${domain_lc}", dn.second);
// 		}
// 		client_.set_sender(hostname_);

		nscapi::core_helper ch(get_core(), get_id());
		ch.register_event("eventlog:login");

	} catch (nsclient::nsclient_exception &e) {
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

void ElasticClient::add_target(std::string key, std::string arg) {
	try {
// 		client_.add_target(get_settings_proxy(), key, arg);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add target: " + key);
	}
}

void ElasticClient::add_command(std::string key, std::string arg) {
	try {
// 		nscapi::core_helper core(get_core(), get_id());
// 		std::string k = client_.add_command(key, arg);
// 		if (!k.empty())
// 			core.register_command(k.c_str(), "Graphite relay for: " + key);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add command: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add command: " + key);
	}
}

/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool ElasticClient::unloadModule() {
// 	client_.clear();
	return true;
}

void ElasticClient::query_fallback(const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
// 	client_.do_query(request_message, response_message);
}

bool ElasticClient::commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage &request, Plugin::ExecuteResponseMessage &response) {

	//Mongoose::Client c("http://127.0.0.1:9200/_ingest/pipeline/my-pipeline-id?pretty");
	Mongoose::Client c("http://127.0.0.1:9200/_bulk");
	// 	if (target_mode == NSCAPI::target_module)
	std::map<std::string, std::string> hdr;
	hdr["Content-Type"] = "application/json";
	//std::string payload = "{\"description\" : \"describe pipeline\",\"processors\" : [{\"set\" : {\"field\": \"foo\",\"value\": \"bar\"}}]}";
	std::string payload = "{ \"index\" : { \"_index\" : \"test\", \"_type\" : \"type1\", \"_id\" : \"1\" } }\n{ \"field1\" : \"value1\" }\n";
	boost::shared_ptr<Mongoose::Response> r = c.fetch("POST", hdr, payload);
	BOOST_FOREACH(const Mongoose::Response::header_type::value_type &v, r->get_headers()) {
		NSC_LOG_ERROR(v.first + " = " + v.second);
	}
	NSC_LOG_ERROR(r->getBody());
	return true;
}

void ElasticClient::handleNotification(const std::string &, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message) {
// 	client_.do_submit(request_message, *response_message);
}

void ElasticClient::onEvent(const Plugin::EventMessage & request, const std::string & buffer) {
	NSC_LOG_ERROR("God evnts...");
	BOOST_FOREACH(const ::Plugin::EventMessage::Request &line, request.payload()) {
		std::string event = line.event();
		json_spirit::Object node;
		BOOST_FOREACH(const ::Plugin::Common::KeyValue e, line.data()) {
			if (e.key() != "message" && e.key() != "xml" && e.key() != "guid") {
				node[e.key()] = e.value();
				NSC_LOG_ERROR("  " + e.key() + ": " + e.value());
			} else if (e.key() == "message") {
				std::string v = e.value(); // .substr(0, 100);
				NSC_LOG_ERROR("  " + e.key() + ": " + str::format::format_buffer("på"));
				NSC_LOG_ERROR("  " + e.key() + ": " + str::format::format_buffer(v));
				node[e.key()] = v;
			}
		}


		using namespace boost::posix_time;
		ptime t = microsec_clock::universal_time();
		node["@timestamp"] = to_iso_extended_string(t) + "Z";

		//Mongoose::Client c("http://127.0.0.1:9200/_ingest/pipeline/my-pipeline-id?pretty");
		Mongoose::Client c("http://127.0.0.1:9200/_bulk");
		// 	if (target_mode == NSCAPI::target_module)
		std::map<std::string, std::string> hdr;
		hdr["Content-Type"] = "application/json";
		//std::string payload = "{\"description\" : \"describe pipeline\",\"processors\" : [{\"set\" : {\"field\": \"foo\",\"value\": \"bar\"}}]}";
		std::string payload = "{ \"index\" : { \"_index\" : \"test\", \"_type\" : \"type1\", \"_id\" : \"" + to_iso_extended_string(t) + "T\" } }\n"
			+ json_spirit::write(node, json_spirit::Output_options::raw_utf8) + "\n";
		NSC_LOG_ERROR(str::format::format_buffer(payload));
		boost::shared_ptr<Mongoose::Response> r = c.fetch("POST", hdr, payload);
		BOOST_FOREACH(const Mongoose::Response::header_type::value_type &v, r->get_headers()) {
			NSC_LOG_ERROR(v.first + " = " + v.second);
		}
		NSC_LOG_ERROR(r->getBody());

	}

}


void build_metrics(json_spirit::Object &metrics, const std::string trail, const Plugin::Common::MetricsBundle & b) {
	json_spirit::Object node;
	BOOST_FOREACH(const Plugin::Common::MetricsBundle &b2, b.children()) {
		build_metrics(node, trail + boost::replace_all_copy(b.alias(), ".", "_"), b2);
	}
	BOOST_FOREACH(const Plugin::Common::Metric &v, b.value()) {
		const ::Plugin::Common_AnyDataType &value = v.value();
		std::string key = trail.empty() ? boost::replace_all_copy(v.key(), ".", "_") 
			: trail + "_" + boost::replace_all_copy(v.key(), ".", "_");
		if (value.has_int_data())
			node.insert(json_spirit::Object::value_type(key, v.value().int_data()));
		else if (value.has_string_data())
			node.insert(json_spirit::Object::value_type(key, v.value().string_data()));
		else if (value.has_float_data())
			node.insert(json_spirit::Object::value_type(key, v.value().float_data()));
	}
	metrics.insert(json_spirit::Object::value_type(b.key(), node));
}
void ElasticClient::submitMetrics(const Plugin::MetricsMessage &response) {

	json_spirit::Object metrics;
	BOOST_FOREACH(const Plugin::MetricsMessage::Response &p, response.payload()) {
		BOOST_FOREACH(const Plugin::Common::MetricsBundle &b, p.bundles()) {
			build_metrics(metrics, "", b);
		}
	}

	using namespace boost::posix_time;
	ptime t = microsec_clock::universal_time();
	metrics["@timestamp"] = to_iso_extended_string(t) + "Z";

	//Mongoose::Client c("http://127.0.0.1:9200/_ingest/pipeline/my-pipeline-id?pretty");
	Mongoose::Client c("http://127.0.0.1:9200/_bulk");
	// 	if (target_mode == NSCAPI::target_module)
	std::map<std::string, std::string> hdr;
	hdr["Content-Type"] = "application/json";
	//std::string payload = "{\"description\" : \"describe pipeline\",\"processors\" : [{\"set\" : {\"field\": \"foo\",\"value\": \"bar\"}}]}";
/*	std::string payload = "{ \"index\" : { \"_index\" : \"test\", \"_type\" : \"type1\", \"_id\" : \"" + to_iso_extended_string(t) + "T\" } }\n"
		+ json_spirit::write(metrics) + "\n";
	boost::shared_ptr<Mongoose::Response> r = c.fetch("POST", hdr, payload);
	BOOST_FOREACH(const Mongoose::Response::header_type::value_type &v, r->get_headers()) {
		NSC_LOG_ERROR(v.first + " = " + v.second);
	}
	NSC_LOG_ERROR(r->getBody());
	*/
}