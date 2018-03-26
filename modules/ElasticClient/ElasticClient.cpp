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

#include <nsclient/logger/logger_helper.hpp>

#include <str/utils.hpp>
#include <str/format.hpp>

#include <Client.hpp>

#include <boost/algorithm/string/replace.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/formatters.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

/**
 * Default c-tor
 * @return
 */
ElasticClient::ElasticClient() : started(false)
{}

/**
 * Default d-tor
 * @return
 */
ElasticClient::~ElasticClient() {}



bool ElasticClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	try {

		std::string events;

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias("elastic", alias, "client");
 
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

			("events", sh::string_key(&events, "eventlog:*,logfile:*"),
				"Event", "The events to subscribe to such as eventlog:* or logfile:mylog.")

			("address", sh::string_key(&address),
				"Elastic address", "The address to send data to (http://127.0.0.1:9200/_bulk).")

			("event index", sh::string_key(&event_index, "nsclient_event-%(date)"),
			"Elastic index used for events", "The elastic index to use for events (log messages).")
			("event type", sh::string_key(&event_type, "eventlog"),
			"Elastic type used for events", "The elastic type to use for events (log messages).")

			("metrics index", sh::string_key(&metrics_index, "nsclient_metrics-%(date)"),
			"Elastic index used for metrics", "The elastic index to use for metrics.")
			("metrics type", sh::string_key(&metrics_type, "metrics"),
			"Elastic type used for metrics", "The elastic type to use for metrics.")

			("nsclient log index", sh::string_key(&nsclient_index, "nsclient_log-%(date)"),
			"Elastic index used for metrics", "The elastic index to use for metrics.")
			("nsclient log type", sh::string_key(&nsclient_type, "nsclient log"),
			"Elastic type used for metrics", "The elastic type to use for metrics.")

			;

		settings.register_all();
		settings.notify();
// 
// 		client_.finalize(get_settings_proxy());
// 
// 		nscapi::core_helper core(get_core(), get_id());
// 		core.register_channel(channel_);
// 
		if (hostname_ == "auto") {
			hostname_ = boost::asio::ip::host_name();
		} else if (hostname_ == "auto-lc") {
			hostname_ = boost::asio::ip::host_name();
			std::transform(hostname_.begin(), hostname_.end(), hostname_.begin(), ::tolower);
		} else if (hostname_ == "auto-uc") {
			hostname_ = boost::asio::ip::host_name();
			std::transform(hostname_.begin(), hostname_.end(), hostname_.begin(), ::toupper);
		} else {
			str::utils::token dn = str::utils::getToken(boost::asio::ip::host_name(), '.');

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

			str::utils::replace(hostname_, "${host}", dn.first);
			str::utils::replace(hostname_, "${domain}", dn.second);
			std::transform(dn.first.begin(), dn.first.end(), dn.first.begin(), ::toupper);
			std::transform(dn.second.begin(), dn.second.end(), dn.second.begin(), ::toupper);
			str::utils::replace(hostname_, "${host_uc}", dn.first);
			str::utils::replace(hostname_, "${domain_uc}", dn.second);
			std::transform(dn.first.begin(), dn.first.end(), dn.first.begin(), ::tolower);
			std::transform(dn.second.begin(), dn.second.end(), dn.second.begin(), ::tolower);
			str::utils::replace(hostname_, "${host_lc}", dn.first);
			str::utils::replace(hostname_, "${domain_lc}", dn.second);
		}

		nscapi::core_helper ch(get_core(), get_id());
		ch.register_event(events);

		if (mode == NSCAPI::normalStart || mode == NSCAPI::reloadStart) {
			started = true;
		}

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

/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool ElasticClient::unloadModule() {
	return true;
}

void ElasticClient::handleNotification(const std::string &, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message) {
// 	client_.do_submit(request_message, *response_message);
}
std::string parse_index(std::string index) {
	std::string date = boost::gregorian::to_iso_extended_string(boost::gregorian::day_clock::universal_day());
	return boost::algorithm::replace_all_copy(index, "%(date)", date);
}
void send_to_elastic(const std::string address, const std::string index, std::string type, const std::vector<std::string> payloads, bool log_errors) {
	boost::uuids::uuid uuid = boost::uuids::random_generator()();

	std::string payload;
	BOOST_FOREACH(const std::string &data, payloads) {
		json_spirit::Object tgtidx;
		tgtidx["_index"] = parse_index(index);
		tgtidx["_type"] = type;
		tgtidx["_id"] = boost::uuids::to_string(uuid);

		json_spirit::Object header;
		header["index"] = tgtidx;

		payload += json_spirit::write(header, json_spirit::raw_utf8) + "\n";
		payload += data + "\n";
	}
	Mongoose::Client c(address);
	std::map<std::string, std::string> http_hdr;
	http_hdr["Content-Type"] = "application/x-ndjson";

	if (log_errors) {
		NSC_TRACE_ENABLED() {
			NSC_TRACE_MSG(payload);
		}
	}
	boost::shared_ptr<Mongoose::Response> r = c.fetch("POST", http_hdr, payload);
	if (!r) {
		if (log_errors) {
			NSC_LOG_ERROR("Failed to send log record to elastic (no response from server)");
		}
		return;
	}
	payload = r->getBody();
	if (log_errors) {
		NSC_TRACE_ENABLED() {
			NSC_TRACE_MSG("code: " + str::xtos(r->get_response_code()));
			BOOST_FOREACH(const Mongoose::Response::header_type::value_type &v, r->get_headers()) {
				NSC_TRACE_MSG(v.first + " = " + v.second);
			}
			NSC_TRACE_MSG(r->getBody());
		}
	}
	try {
		json_spirit::Value root;
		json_spirit::read_or_throw(payload, root);
		if (root.contains("errors")) {
			if (root.getBool("errors")) {
				std::string errors;
				BOOST_FOREACH(const json_spirit::Value &item, root.getArray("items")) {
					str::format::append_list(errors, item.get("index").get("error").getString("reason"));
				}
				if (log_errors) {
					NSC_LOG_ERROR("Failed to send log record to elastic: " + errors);
				}
			}
		} else if (log_errors && root.contains("error")) {
			NSC_LOG_ERROR("Failed to send log record to elastic: " + root.get("error").getString("reason"));
		}
	} catch (const json_spirit::PathError &e) {
		if (log_errors) {
			NSC_LOG_ERROR("Failed to parse elastic response " + e.reason + ": " + e.path);
		}
	} catch (const json_spirit::ParseError &e) {
		if (log_errors) {
			NSC_LOG_ERROR("Failed to parse elastic response: " + e.reason_);
		}
	} catch (const std::exception &e) {
		if (log_errors) {
			NSC_LOG_ERROR_EXR("Failed to parse elastic response: ", e);
		}
	} catch (...) {
		if (log_errors) {
			NSC_LOG_ERROR_EX("Failed to parse elastic response: UNKNOWN EXCEPTION");
		}
	}
}

void ElasticClient::onEvent(const Plugin::EventMessage & request, const std::string & buffer) {
	if (!started || address.empty()) {
		return;
	}
	std::string time = boost::posix_time::to_iso_extended_string(boost::posix_time::microsec_clock::universal_time());
	boost::uuids::uuid uuid = boost::uuids::random_generator()();

	std::vector<std::string> payloads;
	BOOST_FOREACH(const ::Plugin::EventMessage::Request &line, request.payload()) {
		json_spirit::Object node;
		BOOST_FOREACH(const ::Plugin::Common::KeyValue e, line.data()) {
			if (e.key() == "written_str") {
				time = e.value();
			} else if (e.key() != "xml" && e.key() != "written") {
				node[e.key()] = e.value();
			}
		}
		node["@timestamp"] = time;
		node["hostname"] = hostname_;
		payloads.push_back(json_spirit::write(node, json_spirit::raw_utf8));
	}
	send_to_elastic(address, event_index, event_type, payloads, true);
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
	if (!started || address.empty()) {
		return;
	}
	json_spirit::Object metrics;
	metrics["@timestamp"] = boost::posix_time::to_iso_extended_string(boost::posix_time::microsec_clock::universal_time());
	metrics["hostname"] = hostname_;
	BOOST_FOREACH(const Plugin::MetricsMessage::Response &p, response.payload()) {
		BOOST_FOREACH(const Plugin::Common::MetricsBundle &b, p.bundles()) {
			build_metrics(metrics, "", b);
		}
	}

	std::vector<std::string> payloads;
	payloads.push_back(json_spirit::write(metrics, json_spirit::raw_utf8));
	send_to_elastic(address, metrics_index, metrics_type, payloads, true);

}

void ElasticClient::handleLogMessage(const Plugin::LogEntry::Entry &message) {
	if (!started || address.empty()) {
		return;
	}

	json_spirit::Object node;
	node["sender"] = message.sender();
	node["message"] = message.message();
	node["file"] = message.file();
	node["line"] = message.line();
	node["level"] = nsclient::logging::logger_helper::render_log_level_long(message.level());
	node["@timestamp"] = boost::posix_time::to_iso_extended_string(boost::posix_time::microsec_clock::universal_time());
	node["hostname"] = hostname_;

	bool log = message.sender() != "elastic";
	std::vector<std::string> payloads;
	payloads.push_back(json_spirit::write(node, json_spirit::raw_utf8));
	send_to_elastic(address, nsclient_index, nsclient_type, payloads, log);


}
