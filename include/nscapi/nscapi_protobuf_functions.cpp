/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_protobuf_nagios.hpp>

#include <str/utils.hpp>
#include <str/xtos.hpp>
#include <nsclient/nsclient_exception.hpp>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/optional.hpp>

#include <iostream>

#define THROW_INVALID_SIZE(size) \
	throw nsclient::nsclient_exception(std::string("Whoops, invalid payload size: ") + str::xtos(size) + " != 1 at line " + str::xtos(__LINE__));

namespace nscapi {
	namespace protobuf {

		inline Plugin::Common::ResultCode gbp_status_to_gbp_nagios(Plugin::Common::Result::StatusCodeType ret) {
			if (ret == Plugin::Common_Result_StatusCodeType_STATUS_OK)
				return Plugin::Common_ResultCode_OK;
			return Plugin::Common_ResultCode_UNKNOWN;
		}
		inline Plugin::Common::Result::StatusCodeType gbp_to_nagios_gbp_status(Plugin::Common::ResultCode ret) {
			if (ret == Plugin::Common_ResultCode_UNKNOWN || ret == Plugin::Common_ResultCode_WARNING || ret == Plugin::Common_ResultCode_CRITICAL)
				return Plugin::Common_Result_StatusCodeType_STATUS_ERROR;
			return Plugin::Common_Result_StatusCodeType_STATUS_OK;
		}

		double trim_to_double(std::string s) {
			std::string::size_type pend = s.find_first_not_of("0123456789,.-");
			if (pend != std::string::npos)
				s = s.substr(0, pend);
			str::utils::replace(s, ",", ".");
			if (s.empty()) {
				return 0.0;
			}
			try {
				return str::stox<double>(s);
			} catch (...) {
				return 0.0;
			}
		}

		std::string functions::query_data_to_nagios_string(const Plugin::QueryResponseMessage &message, std::size_t max_length) {
			std::stringstream ss;
			for (int i = 0; i < message.payload_size(); ++i) {
				Plugin::QueryResponseMessage::Response p = message.payload(i);
				for (int j = 0; j < p.lines_size(); ++j) {
					Plugin::QueryResponseMessage::Response::Line l = p.lines(j);
					if (l.perf_size() > 0)
						ss << l.message() << "|" << build_performance_data(l, max_length);
					else
						ss << l.message();
				}
			}
			return ss.str();
		}

		void functions::set_response_good(::Plugin::QueryResponseMessage_Response &response, std::string message) {
			response.set_result(::Plugin::Common_ResultCode_OK);
			response.add_lines()->set_message(message);
		}

		void functions::set_response_good_wdata(::Plugin::QueryResponseMessage_Response &response, std::string message) {
			response.set_result(::Plugin::Common_ResultCode_OK);
			response.set_data(message);
			response.add_lines()->set_message("see data segment");
		}

		void functions::set_response_good_wdata(::Plugin::SubmitResponseMessage_Response &response, std::string message) {
			response.mutable_result()->set_code(::Plugin::Common_Result_StatusCodeType_STATUS_OK);
			response.mutable_result()->set_data(message);
			response.mutable_result()->set_message("see data segment");
		}

		void functions::set_response_good_wdata(::Plugin::ExecuteResponseMessage_Response &response, std::string message) {
			response.set_result(::Plugin::Common_ResultCode_OK);
			response.set_data(message);
			response.set_message("see data segment");
			if (!response.has_command())
				response.set_command("unknown");
		}

		void functions::set_response_good(::Plugin::SubmitResponseMessage_Response &response, std::string message) {
			response.mutable_result()->set_code(::Plugin::Common_Result_StatusCodeType_STATUS_OK);
			response.mutable_result()->set_message(message);
			if (!response.has_command())
				response.set_command("unknown");
		}

		void functions::set_response_good(::Plugin::ExecuteResponseMessage_Response &response, std::string message) {
			response.set_result(::Plugin::Common_ResultCode_OK);
			response.set_message(message);
			if (!response.has_command())
				response.set_command("unknown");
		}

		std::string functions::query_data_to_nagios_string(const Plugin::QueryResponseMessage::Response &p, std::size_t max_length) {
			std::stringstream ss;
			for (int j = 0; j < p.lines_size(); ++j) {
				Plugin::QueryResponseMessage::Response::Line l = p.lines(j);
				if (l.perf_size() > 0)
					ss << l.message() << "|" << build_performance_data(l, max_length);
				else
					ss << l.message();
			}
			return ss.str();
		}

		//////////////////////////////////////////////////////////////////////////

		void functions::make_submit_from_query(std::string &message, const std::string channel, const std::string alias, const std::string target, const std::string source) {
			Plugin::QueryResponseMessage response;
			response.ParseFromString(message);
			Plugin::SubmitRequestMessage request;
			request.mutable_header()->CopyFrom(response.header());
			request.mutable_header()->set_source_id(request.mutable_header()->recipient_id());
			for (int i = 0; i < request.mutable_header()->hosts_size(); i++) {
				Plugin::Common_Host *host = request.mutable_header()->mutable_hosts(i);
				if (host->id() == request.mutable_header()->recipient_id()) {
					host->clear_address();
					host->clear_metadata();
				}
			}
			request.set_channel(channel);
			if (!target.empty())
				request.mutable_header()->set_recipient_id(target);
			if (!source.empty()) {
				request.mutable_header()->set_sender_id(source);
				bool found = false;
				for (int i = 0; i < request.mutable_header()->hosts_size(); i++) {
					Plugin::Common_Host *host = request.mutable_header()->mutable_hosts(i);
					if (host->id() == source) {
						host->set_address(source);
						found = true;
					}
				}
				if (!found) {
					Plugin::Common_Host *host = request.mutable_header()->add_hosts();
					host->set_id(source);
					host->set_address(source);
				}
			}
			for (int i = 0; i < response.payload_size(); ++i) {
				request.add_payload()->CopyFrom(response.payload(i));
				if (!alias.empty())
					request.mutable_payload(i)->set_alias(alias);
			}
			message = request.SerializeAsString();
		}

		void functions::make_query_from_exec(std::string &data) {
			Plugin::ExecuteResponseMessage exec_response_message;
			exec_response_message.ParseFromString(data);
			Plugin::QueryResponseMessage query_response_message;
			query_response_message.mutable_header()->CopyFrom(exec_response_message.header());
			for (int i = 0; i < exec_response_message.payload_size(); ++i) {
				Plugin::ExecuteResponseMessage::Response p = exec_response_message.payload(i);
				append_simple_query_response_payload(query_response_message.add_payload(), p.command(), p.result(), p.message());
			}
			data = query_response_message.SerializeAsString();
		}
		void functions::make_query_from_submit(std::string &data) {
			Plugin::SubmitResponseMessage submit_response_message;
			submit_response_message.ParseFromString(data);
			Plugin::QueryResponseMessage query_response_message;
			query_response_message.mutable_header()->CopyFrom(submit_response_message.header());
			for (int i = 0; i < submit_response_message.payload_size(); ++i) {
				Plugin::SubmitResponseMessage::Response p = submit_response_message.payload(i);
				append_simple_query_response_payload(query_response_message.add_payload(), p.command(), gbp_status_to_gbp_nagios(p.result().code()), p.result().message(), "");
			}
			data = query_response_message.SerializeAsString();
		}

		void functions::make_exec_from_submit(std::string &data) {
			Plugin::SubmitResponseMessage submit_response_message;
			submit_response_message.ParseFromString(data);
			Plugin::ExecuteResponseMessage exec_response_message;
			exec_response_message.mutable_header()->CopyFrom(submit_response_message.header());
			for (int i = 0; i < submit_response_message.payload_size(); ++i) {
				Plugin::SubmitResponseMessage::Response p = submit_response_message.payload(i);
				append_simple_exec_response_payload(exec_response_message.add_payload(), p.command(), gbp_status_to_gbp_nagios(p.result().code()), p.result().message());
			}
			data = exec_response_message.SerializeAsString();
		}

		void functions::make_return_header(::Plugin::Common_Header *target, const ::Plugin::Common_Header &source) {
			target->CopyFrom(source);
			target->set_source_id(target->recipient_id());
		}

		void functions::create_simple_submit_request(std::string channel, std::string command, NSCAPI::nagiosReturn ret, std::string msg, std::string perf, std::string &buffer) {
			Plugin::SubmitRequestMessage message;
			message.set_channel(channel);

			Plugin::QueryResponseMessage::Response *payload = message.add_payload();
			payload->set_command(command);
			payload->set_result(nagios_status_to_gpb(ret));
			::Plugin::QueryResponseMessage_Response_Line* l = payload->add_lines();
			l->set_message(msg);
			if (!perf.empty())
				parse_performance_data(l, perf);

			message.SerializeToString(&buffer);
		}

		void functions::create_simple_submit_response_ok(const std::string channel, const std::string command, const std::string msg, std::string &buffer) {
			Plugin::SubmitResponseMessage message;

			Plugin::SubmitResponseMessage::Response *payload = message.add_payload();
			payload->set_command(command);
			payload->mutable_result()->set_message(msg);
			payload->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
			message.SerializeToString(&buffer);
		}
		bool functions::parse_simple_submit_response(const std::string &request, std::string &response) {
			Plugin::SubmitResponseMessage message;
			message.ParseFromString(request);

			if (message.payload_size() != 1) {
				THROW_INVALID_SIZE(message.payload_size());
			}
			::Plugin::SubmitResponseMessage::Response payload = message.payload().Get(0);
			response = payload.mutable_result()->message();
			return payload.mutable_result()->code() == Plugin::Common_Result_StatusCodeType_STATUS_OK;
		}
		void functions::create_simple_query_request(std::string command, std::list<std::string> arguments, std::string &buffer) {
			Plugin::QueryRequestMessage message;

			Plugin::QueryRequestMessage::Request *payload = message.add_payload();
			payload->set_command(command);

			BOOST_FOREACH(std::string s, arguments) {
				payload->add_arguments(s);
			}

			message.SerializeToString(&buffer);
		}
		void functions::create_simple_query_request(std::string command, std::vector<std::string> arguments, std::string &buffer) {
			Plugin::QueryRequestMessage message;

			Plugin::QueryRequestMessage::Request *payload = message.add_payload();
			payload->set_command(command);

			BOOST_FOREACH(std::string s, arguments) {
				payload->add_arguments(s);
			}

			message.SerializeToString(&buffer);
		}

		void functions::append_simple_query_response_payload(Plugin::QueryResponseMessage::Response *payload, std::string command, NSCAPI::nagiosReturn ret, std::string msg, std::string perf) {
			payload->set_command(command);
			payload->set_result(nagios_status_to_gpb(ret));
			Plugin::QueryResponseMessage::Response::Line *l = payload->add_lines();
			l->set_message(msg);
			if (!perf.empty())
				parse_performance_data(l, perf);
		}
		void functions::append_simple_exec_response_payload(Plugin::ExecuteResponseMessage::Response *payload, std::string command, int ret, std::string msg) {
			payload->set_command(command);
			payload->set_message(msg);
			payload->set_result(nagios_status_to_gpb(ret));
		}
		void functions::append_simple_submit_response_payload(Plugin::SubmitResponseMessage::Response *payload, std::string command, bool result, std::string msg) {
			payload->set_command(command);
			payload->mutable_result()->set_code(result? Plugin::Common_Result_StatusCodeType_STATUS_OK:Plugin::Common_Result_StatusCodeType_STATUS_ERROR);
			payload->mutable_result()->set_message(msg);
		}

		void functions::append_simple_query_request_payload(Plugin::QueryRequestMessage::Request *payload, std::string command, std::vector<std::string> arguments) {
			payload->set_command(command);
			BOOST_FOREACH(const std::string &s, arguments) {
				payload->add_arguments(s);
			}
		}

		void functions::append_simple_exec_request_payload(Plugin::ExecuteRequestMessage::Request *payload, std::string command, std::vector<std::string> arguments) {
			payload->set_command(command);
			BOOST_FOREACH(const std::string &s, arguments) {
				payload->add_arguments(s);
			}
		}

		void functions::parse_simple_query_request(std::list<std::string> &args, const std::string &request) {

			Plugin::QueryRequestMessage message;
			message.ParseFromString(request);

			if (message.payload_size() != 1) {
				THROW_INVALID_SIZE(message.payload_size());
			}
			::Plugin::QueryRequestMessage::Request payload = message.payload().Get(0);
			for (int i = 0; i < payload.arguments_size(); i++) {
				args.push_back(payload.arguments(i));
			}
		}
		int functions::parse_simple_query_response(const std::string &response, std::string &msg, std::string &perf, std::size_t max_length) {
			Plugin::QueryResponseMessage message;
			message.ParseFromString(response);

			if (message.payload_size() == 0 || message.payload(0).lines_size() == 0) {
				return NSCAPI::query_return_codes::returnUNKNOWN;
			} else if (message.payload_size() > 1 && message.payload(0).lines_size() > 1) {
				THROW_INVALID_SIZE(message.payload_size());
			}

			Plugin::QueryResponseMessage::Response payload = message.payload().Get(0);
			BOOST_FOREACH(const Plugin::QueryResponseMessage::Response::Line &l, payload.lines()) {
				msg += l.message();
				std::string tmpPerf = build_performance_data(l, max_length);
				if (!tmpPerf.empty()) {
					if (perf.empty()) {
						perf = tmpPerf;
					} else {
						perf += " " + tmpPerf;
					}
				}
			}
			return gbp_to_nagios_status(payload.result());
		}

		//////////////////////////////////////////////////////////////////////////

		void functions::create_simple_exec_request(const std::string &module, const std::string &command, const std::list<std::string> & args, std::string &request) {
			Plugin::ExecuteRequestMessage message;
			if (!module.empty()) {
				::Plugin::Common_KeyValue* kvp = message.mutable_header()->add_metadata();
				kvp->set_key("target");
				kvp->set_value(module);
			}

			Plugin::ExecuteRequestMessage::Request *payload = message.add_payload();
			payload->set_command(command);

			BOOST_FOREACH(const std::string &s, args)
				payload->add_arguments(s);

			message.SerializeToString(&request);
		}

		void functions::create_simple_exec_request(const std::string &module, const std::string &command, const std::vector<std::string> & args, std::string &request) {
			Plugin::ExecuteRequestMessage message;
			if (!module.empty()) {
				::Plugin::Common_KeyValue* kvp = message.mutable_header()->add_metadata();
				kvp->set_key("target");
				kvp->set_value(module);
			}

			Plugin::ExecuteRequestMessage::Request *payload = message.add_payload();
			payload->set_command(command);

			BOOST_FOREACH(std::string s, args)
				payload->add_arguments(s);

			message.SerializeToString(&request);
		}
		int functions::parse_simple_exec_response(const std::string &response, std::list<std::string> &result) {
			int ret = 0;
			Plugin::ExecuteResponseMessage message;
			message.ParseFromString(response);

			for (int i = 0; i < message.payload_size(); i++) {
				result.push_back(message.payload(i).message());
				int r = gbp_to_nagios_status(message.payload(i).result());
				if (r > ret)
					ret = r;
			}
			return ret;
		}

		int functions::create_simple_exec_response(const std::string &command, NSCAPI::nagiosReturn ret, const std::string result, std::string &response) {
			Plugin::ExecuteResponseMessage message;

			Plugin::ExecuteResponseMessage::Response *payload = message.add_payload();
			payload->set_command(command);
			payload->set_message(result);

			payload->set_result(nagios_status_to_gpb(ret));
			message.SerializeToString(&response);
			return NSCAPI::cmd_return_codes::isSuccess;
		}
		int functions::create_simple_exec_response_unknown(std::string command, std::string result, std::string &response) {
			Plugin::ExecuteResponseMessage message;

			Plugin::ExecuteResponseMessage::Response *payload = message.add_payload();
			payload->set_command(command);
			payload->set_message(result);

			payload->set_result(nagios_status_to_gpb(NSCAPI::exec_return_codes::returnERROR));
			message.SerializeToString(&response);
			return NSCAPI::cmd_return_codes::isSuccess;
		}

		//////////////////////////////////////////////////////////////////////////

		long long get_multiplier(const std::string &unit) {
			if (unit.empty())
				return 1;
			if (unit[0] == 'K')
				return 1024ll;
			if (unit[0] == 'M')
				return 1024ll * 1024ll;
			if (unit[0] == 'G')
				return 1024ll * 1024ll * 1024ll;
			if (unit[0] == 'T')
				return 1024ll * 1024ll * 1024ll * 1024ll;
			return 1;
		}
		std::string functions::extract_perf_value_as_string(const ::Plugin::Common_PerformanceData &perf) {
			if (perf.has_int_value()) {
				const Plugin::Common::PerformanceData::IntValue &val = perf.int_value();
				if (val.has_unit())
					return str::xtos_non_sci(val.value()*get_multiplier(val.unit()));
				return str::xtos_non_sci(val.value());
			} else if (perf.has_bool_value()) {
				const Plugin::Common::PerformanceData::BoolValue &val = perf.bool_value();
				return val.value() ? "true" : "false";
			} else if (perf.has_float_value()) {
				const Plugin::Common::PerformanceData::FloatValue &val = perf.float_value();
				return str::xtos_non_sci(val.value()*get_multiplier(val.unit()));
			} else if (perf.has_string_value()) {
				const Plugin::Common::PerformanceData::StringValue& val = perf.string_value();
				return val.value();
			}
			return "unknown";
		}
		long long functions::extract_perf_value_as_int(const ::Plugin::Common_PerformanceData &perf) {
			if (perf.has_int_value()) {
				const Plugin::Common::PerformanceData::IntValue &val = perf.int_value();
				if (val.has_unit())
					return val.value()*get_multiplier(val.unit());
				return val.value();
			} else if (perf.has_bool_value()) {
				const Plugin::Common::PerformanceData::BoolValue &val = perf.bool_value();
				return val.value() ? 1 : 0;
			} else if (perf.has_float_value()) {
				const Plugin::Common::PerformanceData::FloatValue &val = perf.float_value();
				if (val.has_unit())
					return static_cast<long long>(val.value()*get_multiplier(val.unit()));
				return static_cast<long long>(val.value());
			} else if (perf.has_string_value()) {
				return 0;
			}
			return 0;
		}
		std::string functions::extract_perf_maximum_as_string(const ::Plugin::Common_PerformanceData &perf) {
			if (perf.has_int_value()) {
				const Plugin::Common::PerformanceData::IntValue &val = perf.int_value();
				if (val.has_unit())
					return str::xtos_non_sci(val.maximum()*get_multiplier(val.unit()));
				return str::xtos_non_sci(val.maximum());
			} else if (perf.has_bool_value() || perf.has_string_value()) {
				return "";
			} else if (perf.has_float_value()) {
				const Plugin::Common::PerformanceData::FloatValue &val = perf.float_value();
				if (val.has_unit())
					return str::xtos_non_sci(val.maximum()*get_multiplier(val.unit()));
				return str::xtos_non_sci(val.maximum());
			}
			return "unknown";
		}

		void functions::parse_performance_data(Plugin::QueryResponseMessage::Response::Line *payload, const std::string &perff) {
			std::string perf = perff;
			// TODO: make this work with const!

			const std::string perf_separator = " ";
			const std::string perf_lable_enclosure = "'";
			const std::string perf_equal_sign = "=";
			const std::string perf_item_splitter = ";";
			const std::string perf_valid_number = "0123456789,.-";

			while (true) {
				if (perf.size() == 0)
					return;
				std::string::size_type p = 0;
				p = perf.find_first_not_of(perf_separator, p);
				if (p != 0)
					perf = perf.substr(p);
				if (perf[0] == perf_lable_enclosure[0]) {
					p = perf.find(perf_lable_enclosure[0], 1) + 1;
					if (p == std::string::npos)
						return;
				}
				p = perf.find(perf_separator, p);
				if (p == 0)
					return;
				std::string chunk;
				if (p == std::string::npos) {
					chunk = perf;
					perf = std::string();
				} else {
					chunk = perf.substr(0, p);
					p = perf.find_first_not_of(perf_separator, p);
					if (p == std::string::npos)
						perf = std::string();
					else
						perf = perf.substr(p);
				}
				std::vector<std::string> items;
				str::utils::split(items, chunk, perf_item_splitter);
				if (items.size() < 1) {
					Plugin::Common::PerformanceData* perfData = payload->add_perf();
					std::pair<std::string, std::string> fitem = str::utils::split2("", perf_equal_sign);
					perfData->set_alias("invalid");
					Plugin::Common_PerformanceData_StringValue* stringPerfData = perfData->mutable_string_value();
					stringPerfData->set_value("invalid performance data");
					break;
				}

				std::pair<std::string, std::string> fitem = str::utils::split2(items[0], perf_equal_sign);
				std::string alias = fitem.first;
				if (alias.size() > 0 && alias[0] == perf_lable_enclosure[0] && alias[alias.size() - 1] == perf_lable_enclosure[0])
					alias = alias.substr(1, alias.size() - 2);

				if (alias.empty())
					continue;
				Plugin::Common::PerformanceData* perfData = payload->add_perf();
				perfData->set_alias(alias);
				Plugin::Common_PerformanceData_FloatValue* floatPerfData = perfData->mutable_float_value();

				std::string::size_type pstart = fitem.second.find_first_of(perf_valid_number);
				if (pstart == std::string::npos) {
					floatPerfData->set_value(0);
					continue;
				}
				if (pstart != 0)
					fitem.second = fitem.second.substr(pstart);
				std::string::size_type pend = fitem.second.find_first_not_of(perf_valid_number);
				if (pend == std::string::npos) {
					floatPerfData->set_value(trim_to_double(fitem.second));
				} else {
					floatPerfData->set_value(trim_to_double(fitem.second.substr(0, pend)));
					floatPerfData->set_unit(fitem.second.substr(pend));
				}
				if (items.size() >= 2 && items[1].size() > 0)
					floatPerfData->set_warning(trim_to_double(items[1]));
				if (items.size() >= 3 && items[2].size() > 0)
					floatPerfData->set_critical(trim_to_double(items[2]));
				if (items.size() >= 4 && items[3].size() > 0)
					floatPerfData->set_minimum(trim_to_double(items[3]));
				if (items.size() >= 5 && items[4].size() > 0)
					floatPerfData->set_maximum(trim_to_double(items[4]));
			}
		}

		void parse_float_perf_value(std::stringstream &ss, const Plugin::Common_PerformanceData_FloatValue &val) {
			ss << str::xtos_non_sci(val.value());
			if (val.has_unit())
				ss << val.unit();
			if (!val.has_warning() && !val.has_critical() && !val.has_minimum() && !val.has_maximum()) {
				return;
			}
			ss << ";";
			if (val.has_warning())
				ss << str::xtos_non_sci(val.warning());
			if (!val.has_critical() && !val.has_minimum() && !val.has_maximum()) {
				return;
			}
			ss << ";";
			if (val.has_critical())
				ss << str::xtos_non_sci(val.critical());
			if (!val.has_minimum() && !val.has_maximum()) {
				return;
			}
			ss << ";";
			if (val.has_minimum())
				ss << str::xtos_non_sci(val.minimum());
			if (!val.has_maximum()) {
				return;
			}
			ss << ";";
			if (val.has_maximum())
				ss << str::xtos_non_sci(val.maximum());
			return;
		}

		void parse_int_perf_value(std::stringstream &ss, const Plugin::Common_PerformanceData_IntValue &val) {
			ss << val.value();
			if (val.has_unit())
				ss << val.unit();
			if (!val.has_warning() && !val.has_critical() && !val.has_minimum() && !val.has_maximum()) {
				return;
			}
			ss << ";";
			if (val.has_warning())
				ss << val.warning();
			if (!val.has_critical() && !val.has_minimum() && !val.has_maximum()) {
				return;
			}
			ss << ";";
			if (val.has_critical())
				ss << val.critical();
			if (!val.has_minimum() && !val.has_maximum()) {
				return;
			}
			ss << ";";
			if (val.has_minimum())
				ss << val.minimum();
			if (!val.has_maximum()) {
				return;
			}
			ss << ";";
			if (val.has_maximum())
				ss << val.maximum();
			return;
		}

		std::string functions::build_performance_data(Plugin::QueryResponseMessage::Response::Line const &payload, std::size_t len) {
			std::string ret;

			bool first = true;
			for (int i = 0; i < payload.perf_size(); i++) {
				std::stringstream ss;
				ss.precision(5);
				Plugin::Common::PerformanceData perfData = payload.perf(i);
				if (!first)
					ss << " ";
				first = false;
				ss << '\'' << perfData.alias() << "'=";
				if (perfData.has_float_value()) {
					parse_float_perf_value(ss, perfData.float_value());
				} else if (perfData.has_int_value()) {
					parse_int_perf_value(ss, perfData.int_value());
				}
				std::string tmp = ss.str();
				if (len == no_truncation || ret.length() + tmp.length() <= len) {
					ret += tmp;
				}
			}
			return ret;
		}

		Plugin::Common::ResultCode functions::nagios_status_to_gpb(int ret) {
			if (ret == NSCAPI::query_return_codes::returnOK)
				return Plugin::Common_ResultCode_OK;
			if (ret == NSCAPI::query_return_codes::returnWARN)
				return Plugin::Common_ResultCode_WARNING;
			if (ret == NSCAPI::query_return_codes::returnCRIT)
				return Plugin::Common_ResultCode_CRITICAL;
			return Plugin::Common_ResultCode_UNKNOWN;
		}

		int functions::gbp_to_nagios_status(Plugin::Common::ResultCode ret) {
			if (ret == Plugin::Common_ResultCode_OK)
				return NSCAPI::query_return_codes::returnOK;
			if (ret == Plugin::Common_ResultCode_WARNING)
				return NSCAPI::query_return_codes::returnWARN;
			if (ret == Plugin::Common_ResultCode_CRITICAL)
				return NSCAPI::query_return_codes::returnCRIT;
			return NSCAPI::query_return_codes::returnUNKNOWN;
		}

		void functions::set_response_bad(::Plugin::QueryResponseMessage_Response &response, std::string message) {
			response.set_result(Plugin::Common_ResultCode_UNKNOWN);
			response.add_lines()->set_message(message);
			if (!response.has_command())
				response.set_command("unknown");
		}

		void functions::set_response_bad(::Plugin::SubmitResponseMessage::Response &response, std::string message) {
			response.mutable_result()->set_code(::Plugin::Common_Result_StatusCodeType_STATUS_ERROR);
			response.mutable_result()->set_message(message);
			if (!response.has_command())
				response.set_command("unknown");
		}

		void functions::set_response_bad(::Plugin::ExecuteResponseMessage::Response &response, std::string message) {
			response.set_result(Plugin::Common_ResultCode_UNKNOWN);
			response.set_message(message);
			if (!response.has_command())
				response.set_command("unknown");
		}

		Plugin::Common::ResultCode functions::parse_nagios(const std::string &status) {
			std::string lcstat = boost::to_lower_copy(status);
			if (lcstat == "o" || lcstat == "ok" || lcstat == "0")
				return Plugin::Common_ResultCode_OK;
			if (lcstat == "w" || lcstat == "warn" || lcstat == "warning" || lcstat == "1")
				return Plugin::Common_ResultCode_WARNING;
			if (lcstat == "c" || lcstat == "crit" || lcstat == "critical" || lcstat == "2")
				return Plugin::Common_ResultCode_CRITICAL;
			return Plugin::Common_ResultCode_UNKNOWN;
		}

		namespace functions {

			struct settings_query_data {
				::Plugin::SettingsRequestMessage request_message;
				::Plugin::SettingsResponseMessage response_message;
				std::string response_buffer;
				int plugin_id;
				
				settings_query_data(int plugin_id) : plugin_id(plugin_id) {}
			};

			struct  settings_query_key_values_data {
				std::string path;
				boost::optional<std::string> key;
				boost::optional<std::string> str_value;
				boost::optional<long long> int_value;
				boost::optional<bool> bool_value;

				settings_query_key_values_data(std::string path) : path(path) {}
				settings_query_key_values_data(std::string path, std::string key, std::string str_value) : path(path), key(key), str_value(str_value) {}
				settings_query_key_values_data(std::string path, std::string key, long long int_value) : path(path), key(key), int_value(int_value) {}
				settings_query_key_values_data(std::string path, std::string key, bool bool_value) : path(path), key(key), bool_value(bool_value) {}
				settings_query_key_values_data(const settings_query_key_values_data& other) : path(other.path), key(other.key), str_value(other.str_value), int_value(other.int_value), bool_value(other.bool_value) {}

				settings_query_key_values_data& operator=(const settings_query_key_values_data& other) {
					path = other.path;
					key = other.key;
					str_value = other.str_value;
					int_value = other.int_value;
					bool_value = other.bool_value;
					return *this;
				}


			};

			settings_query::key_values::key_values(std::string path) : pimpl(new settings_query_key_values_data(path)) {}
			settings_query::key_values::key_values(std::string path, std::string key, std::string str_value) : pimpl(new settings_query_key_values_data(path, key, str_value)) {}
			settings_query::key_values::key_values(std::string path, std::string key, long long int_value) : pimpl(new settings_query_key_values_data(path, key, int_value)) {}
			settings_query::key_values::key_values(std::string path, std::string key, bool bool_value) : pimpl(new settings_query_key_values_data(path, key, bool_value)) {}

			settings_query::key_values::key_values(const key_values &other)
				: pimpl(new settings_query_key_values_data(*other.pimpl)) {}
			settings_query::key_values& settings_query::key_values::operator= (const key_values &other) {
				pimpl->operator=(*other.pimpl);
				return *this;
			}

			settings_query::key_values::~key_values() {
				delete pimpl;
			}


			bool settings_query::key_values::matches(const char* path, const char* key) const {
				if (!pimpl || !pimpl->key)
					return false;
				return pimpl->path == path && *pimpl->key == key;
			}
			bool settings_query::key_values::matches(const char* path) const {
				if (!pimpl->key)
					return false;
				return pimpl->path == path;
			}
			bool settings_query::key_values::matches(const std::string &path, const std::string &key) const {
				if (!pimpl || !pimpl->key)
					return false;
				return pimpl->path == path && *pimpl->key == key;
			}
			bool settings_query::key_values::matches(const std::string &path) const {
				if (!pimpl)
					return false;
				return pimpl->path == path;
			}

			std::string settings_query::key_values::path() const {
				if (!pimpl)
					return "";
				return pimpl->path;
			}
			std::string settings_query::key_values::key() const {
				if (!pimpl || !pimpl->key)
					return "";
				return *pimpl->key;
			}

			std::string settings_query::key_values::get_string() const {
				if (pimpl->str_value)
					return *pimpl->str_value;
				if (pimpl->int_value)
					return str::xtos(*pimpl->int_value);
				if (pimpl->bool_value)
					return *pimpl->bool_value ? "true" : "false";
				return "";
			}

			long long settings_query::key_values::get_int() const {
				if (pimpl->str_value)
					return str::stox<long long>(*pimpl->str_value);
				if (pimpl->int_value)
					return *pimpl->int_value;
				if (pimpl->bool_value)
					return *pimpl->bool_value ? 1 : 0;
				return 0;
			}

			bool settings_query::key_values::get_bool() const {
				if (pimpl->str_value) {
					std::string s = *pimpl->str_value;
					std::transform(s.begin(), s.end(), s.begin(), ::tolower);
					return s == "true" || s == "1";
				}
				if (pimpl->int_value)
					return *pimpl->int_value == 1;
				if (pimpl->bool_value)
					return *pimpl->bool_value;
				return "";
			}

			settings_query::settings_query(int plugin_id) : pimpl(new settings_query_data(plugin_id)) {
			}
			settings_query::~settings_query() {
				delete pimpl;
			}

			void settings_query::set(const std::string path, const std::string key, const std::string value) {
				::Plugin::SettingsRequestMessage::Request *r = pimpl->request_message.add_payload();
				r->set_plugin_id(pimpl->plugin_id);
				r->mutable_update()->mutable_node()->set_path(path);
				r->mutable_update()->mutable_node()->set_key(key);
				r->mutable_update()->mutable_value()->set_string_data(value);
			}

			void settings_query::erase(const std::string path, const std::string key) {
				::Plugin::SettingsRequestMessage::Request *r = pimpl->request_message.add_payload();
				r->set_plugin_id(pimpl->plugin_id);
				r->mutable_update()->mutable_node()->set_path(path);
				r->mutable_update()->mutable_node()->set_key(key);
			}

			void settings_query::get(const std::string path, const std::string key, const std::string def) {
				::Plugin::SettingsRequestMessage::Request *r = pimpl->request_message.add_payload();
				r->set_plugin_id(pimpl->plugin_id);
				r->mutable_query()->mutable_node()->set_path(path);
				r->mutable_query()->mutable_node()->set_key(key);
				r->mutable_query()->set_type(::Plugin::Common_DataType_STRING);
				r->mutable_query()->mutable_default_value()->set_string_data(def);
				r->mutable_query()->set_recursive(false);
			}
			void settings_query::get(const std::string path, const std::string key, const char* def) {
				::Plugin::SettingsRequestMessage::Request *r = pimpl->request_message.add_payload();
				r->set_plugin_id(pimpl->plugin_id);
				r->mutable_query()->mutable_node()->set_path(path);
				r->mutable_query()->mutable_node()->set_key(key);
				r->mutable_query()->set_type(::Plugin::Common_DataType_STRING);
				r->mutable_query()->mutable_default_value()->set_string_data(def);
				r->mutable_query()->set_recursive(false);
			}
			void settings_query::get(const std::string path, const std::string key, const long long def) {
				::Plugin::SettingsRequestMessage::Request *r = pimpl->request_message.add_payload();
				r->set_plugin_id(pimpl->plugin_id);
				r->mutable_query()->mutable_node()->set_path(path);
				r->mutable_query()->mutable_node()->set_key(key);
				r->mutable_query()->set_type(::Plugin::Common_DataType_INT);
				r->mutable_query()->mutable_default_value()->set_int_data(def);
				r->mutable_query()->set_recursive(false);
			}
			void settings_query::get(const std::string path, const std::string key, const bool def) {
				::Plugin::SettingsRequestMessage::Request *r = pimpl->request_message.add_payload();
				r->set_plugin_id(pimpl->plugin_id);
				r->mutable_query()->mutable_node()->set_path(path);
				r->mutable_query()->mutable_node()->set_key(key);
				r->mutable_query()->set_type(::Plugin::Common_DataType_BOOL);
				r->mutable_query()->mutable_default_value()->set_bool_data(def);
				r->mutable_query()->set_recursive(false);
			}

			void settings_query::list(const std::string path, const bool recursive) {
				::Plugin::SettingsRequestMessage::Request *r = pimpl->request_message.add_payload();
				r->set_plugin_id(pimpl->plugin_id);
				r->mutable_inventory()->mutable_node()->set_path(path);
				r->mutable_inventory()->set_fetch_keys(true);
				r->mutable_inventory()->set_recursive_fetch(recursive);
			}


			void settings_query::save() {
				::Plugin::SettingsRequestMessage::Request *r = pimpl->request_message.add_payload();
				r->set_plugin_id(pimpl->plugin_id);
				r->mutable_control()->set_command(Plugin::Settings_Command_SAVE);
			}
			void settings_query::load() {
				::Plugin::SettingsRequestMessage::Request *r = pimpl->request_message.add_payload();
				r->set_plugin_id(pimpl->plugin_id);
				r->mutable_control()->set_command(Plugin::Settings_Command_LOAD);
			}
			void settings_query::reload() {
				::Plugin::SettingsRequestMessage::Request *r = pimpl->request_message.add_payload();
				r->set_plugin_id(pimpl->plugin_id);
				r->mutable_control()->set_command(Plugin::Settings_Command_RELOAD);
			}
			const std::string settings_query::request() const {
				return pimpl->request_message.SerializeAsString();
			}

			bool settings_query::validate_response() const {
				pimpl->response_message.ParsePartialFromString(pimpl->response_buffer);
				bool ret = true;
				for (int i = 0; i < pimpl->response_message.payload_size(); ++i) {
					if (pimpl->response_message.payload(i).result().code() != Plugin::Common_Result_StatusCodeType_STATUS_OK)
						ret = false;
				}
				return ret;
			}
			std::string settings_query::get_response_error() const {
				std::string ret;
				for (int i = 0; i < pimpl->response_message.payload_size(); ++i) {
					ret += pimpl->response_message.payload(i).result().message();
				}
				return ret;
			}
			std::list<settings_query::key_values> settings_query::get_query_key_response() const {
				std::list<key_values> ret;
				for (int i = 0; i < pimpl->response_message.payload_size(); ++i) {
					::Plugin::SettingsResponseMessage::Response pl = pimpl->response_message.payload(i);
					if (pl.has_query()) {
						::Plugin::SettingsResponseMessage::Response::Query q = pl.query();
						if (q.node().has_key() && q.has_value()) {
							if (q.value().has_string_data())
								ret.push_back(key_values(q.node().path(), q.node().key(), q.value().string_data()));
							else if (q.value().has_int_data())
								ret.push_back(key_values(q.node().path(), q.node().key(), static_cast<long long>(q.value().int_data())));
							else if (q.value().has_bool_data())
								ret.push_back(key_values(q.node().path(), q.node().key(), q.value().bool_data()));
						} else {
							ret.push_back(key_values(q.node().path()));
						}
					} else if (pl.inventory_size() > 0) {
						BOOST_FOREACH(const ::Plugin::SettingsResponseMessage::Response::Inventory &q, pl.inventory()) {
							if (q.node().has_key() && q.has_value()) {
								if (q.value().has_string_data())
									ret.push_back(key_values(q.node().path(), q.node().key(), q.value().string_data()));
								else if (q.value().has_int_data())
									ret.push_back(key_values(q.node().path(), q.node().key(), static_cast<long long>(q.value().int_data())));
								else if (q.value().has_bool_data())
									ret.push_back(key_values(q.node().path(), q.node().key(), q.value().bool_data()));
							} else if (q.node().has_key() && q.has_info() && q.info().has_default_value()) {
								if (q.info().default_value().has_string_data())
									ret.push_back(key_values(q.node().path(), q.node().key(), q.info().default_value().string_data()));
								else if (q.info().default_value().has_int_data())
									ret.push_back(key_values(q.node().path(), q.node().key(), static_cast<long long>(q.info().default_value().int_data())));
								else if (q.info().default_value().has_bool_data())
									ret.push_back(key_values(q.node().path(), q.node().key(), q.info().default_value().bool_data()));
							} else {
								ret.push_back(key_values(q.node().path()));
							}
						}
					}
				}
				return ret;
			}

			std::string& settings_query::response() const { 
				return pimpl->response_buffer; 
			}




			void copy_response(const std::string command, ::Plugin::QueryResponseMessage::Response* target, const ::Plugin::ExecuteResponseMessage::Response source) {
				::Plugin::QueryResponseMessage::Response::Line* line = target->add_lines();
				line->set_message(source.message());
				target->set_command(command);
			}
			void copy_response(const std::string command, ::Plugin::QueryResponseMessage::Response* target, const ::Plugin::SubmitResponseMessage::Response source) {
				::Plugin::QueryResponseMessage::Response::Line* line = target->add_lines();
				line->set_message(source.result().message());
				target->set_command(command);
				target->set_result(gbp_status_to_gbp_nagios(source.result().code()));
			}
			void copy_response(const std::string command, ::Plugin::QueryResponseMessage::Response* target, const ::Plugin::QueryResponseMessage::Response source) {
				target->CopyFrom(source);
			}
			void copy_response(const std::string command, ::Plugin::ExecuteResponseMessage::Response* target, const ::Plugin::ExecuteResponseMessage::Response source) {
				target->CopyFrom(source);
			}
			void copy_response(const std::string command, ::Plugin::ExecuteResponseMessage::Response* target, const ::Plugin::SubmitResponseMessage::Response source) {
				target->set_message(source.result().message());
				target->set_command(source.command());
				target->set_result(gbp_status_to_gbp_nagios(source.result().code()));
			}
			void copy_response(const std::string command, ::Plugin::ExecuteResponseMessage::Response* target, const ::Plugin::QueryResponseMessage::Response source) {
				target->set_message(query_data_to_nagios_string(source, no_truncation));
				target->set_command(source.command());
				target->set_result(source.result());
			}
			void copy_response(const std::string command, ::Plugin::SubmitResponseMessage::Response* target, const ::Plugin::ExecuteResponseMessage::Response source) {
				target->mutable_result()->set_message(source.message());
			}
			void copy_response(const std::string command, ::Plugin::SubmitResponseMessage::Response* target, const ::Plugin::SubmitResponseMessage::Response source) {
				target->CopyFrom(source);
			}
			void copy_response(const std::string command, ::Plugin::SubmitResponseMessage::Response* target, const ::Plugin::QueryResponseMessage::Response source) {
				target->mutable_result()->set_message(query_data_to_nagios_string(source, -1));
				target->mutable_result()->set_code(gbp_to_nagios_gbp_status(source.result()));
			}
		}
	}
}