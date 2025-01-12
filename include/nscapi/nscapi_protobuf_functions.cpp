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

#include <parsers/perfdata.hpp>

#include <nscapi/nscapi_protobuf_functions.hpp>
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

		inline PB::Common::ResultCode gbp_status_to_gbp_nagios(PB::Common::Result::StatusCodeType ret) {
			if (ret == PB::Common::Result_StatusCodeType_STATUS_OK)
				return PB::Common::ResultCode::OK;
			return PB::Common::ResultCode::UNKNOWN;
		}
		inline PB::Common::Result::StatusCodeType gbp_to_nagios_gbp_status(PB::Common::ResultCode ret) {
			if (ret == PB::Common::ResultCode::UNKNOWN || ret == PB::Common::ResultCode::WARNING || ret == PB::Common::ResultCode::CRITICAL)
				return PB::Common::Result_StatusCodeType_STATUS_ERROR;
			return PB::Common::Result_StatusCodeType_STATUS_OK;
		}

		std::string functions::query_data_to_nagios_string(const PB::Commands::QueryResponseMessage &message, std::size_t max_length) {
			std::stringstream ss;
			for (int i = 0; i < message.payload_size(); ++i) {
				PB::Commands::QueryResponseMessage::Response p = message.payload(i);
				for (int j = 0; j < p.lines_size(); ++j) {
					PB::Commands::QueryResponseMessage::Response::Line l = p.lines(j);
					if (l.perf_size() > 0)
						ss << l.message() << "|" << build_performance_data(l, max_length);
					else
						ss << l.message();
				}
			}
			return ss.str();
		}

		void functions::set_response_good(::PB::Commands::QueryResponseMessage_Response &response, std::string message) {
			response.set_result(PB::Common::ResultCode::OK);
			response.add_lines()->set_message(message);
		}

		void functions::set_response_good_wdata(::PB::Commands::QueryResponseMessage_Response &response, std::string message) {
			response.set_result(PB::Common::ResultCode::OK);
			response.set_data(message);
			response.add_lines()->set_message("see data segment");
		}

		void functions::set_response_good_wdata(::PB::Commands::SubmitResponseMessage_Response &response, std::string message) {
			response.mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
			response.mutable_result()->set_data(message);
			response.mutable_result()->set_message("see data segment");
		}

		void functions::set_response_good_wdata(::PB::Commands::ExecuteResponseMessage_Response &response, std::string message) {
			response.set_result(PB::Common::ResultCode::OK);
			response.set_data(message);
			response.set_message("see data segment");
			if (response.command().empty())
				response.set_command("unknown");
		}

		void functions::set_response_good(::PB::Commands::SubmitResponseMessage_Response &response, std::string message) {
			response.mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
			response.mutable_result()->set_message(message);
			if (response.command().empty())
				response.set_command("unknown");
		}

		void functions::set_response_good(::PB::Commands::ExecuteResponseMessage_Response &response, std::string message) {
			response.set_result(PB::Common::ResultCode::OK);
			response.set_message(message);
			if (response.command().empty())
				response.set_command("unknown");
		}

		std::string functions::query_data_to_nagios_string(const PB::Commands::QueryResponseMessage::Response &p, std::size_t max_length) {
			std::stringstream ss;
			for (int j = 0; j < p.lines_size(); ++j) {
				PB::Commands::QueryResponseMessage::Response::Line l = p.lines(j);
				if (l.perf_size() > 0)
					ss << l.message() << "|" << build_performance_data(l, max_length);
				else
					ss << l.message();
			}
			return ss.str();
		}

		//////////////////////////////////////////////////////////////////////////

		void functions::make_submit_from_query(std::string &message, const std::string channel, const std::string alias, const std::string target, const std::string source) {
			PB::Commands::QueryResponseMessage response;
			response.ParseFromString(message);
			PB::Commands::SubmitRequestMessage request;
			request.mutable_header()->CopyFrom(response.header());
			request.mutable_header()->set_source_id(request.mutable_header()->recipient_id());
			for (int i = 0; i < request.mutable_header()->hosts_size(); i++) {
				PB::Common::Host *host = request.mutable_header()->mutable_hosts(i);
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
					PB::Common::Host *host = request.mutable_header()->mutable_hosts(i);
					if (host->id() == source) {
						host->set_address(source);
						found = true;
					}
				}
				if (!found) {
					PB::Common::Host *host = request.mutable_header()->add_hosts();
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
			PB::Commands::ExecuteResponseMessage exec_response_message;
			exec_response_message.ParseFromString(data);
			PB::Commands::QueryResponseMessage query_response_message;
			query_response_message.mutable_header()->CopyFrom(exec_response_message.header());
			for (int i = 0; i < exec_response_message.payload_size(); ++i) {
				PB::Commands::ExecuteResponseMessage::Response p = exec_response_message.payload(i);
				append_simple_query_response_payload(query_response_message.add_payload(), p.command(), p.result(), p.message());
			}
			data = query_response_message.SerializeAsString();
		}
		void functions::make_query_from_submit(std::string &data) {
			PB::Commands::SubmitResponseMessage submit_response_message;
			submit_response_message.ParseFromString(data);
			PB::Commands::QueryResponseMessage query_response_message;
			query_response_message.mutable_header()->CopyFrom(submit_response_message.header());
			for (int i = 0; i < submit_response_message.payload_size(); ++i) {
				PB::Commands::SubmitResponseMessage::Response p = submit_response_message.payload(i);
				append_simple_query_response_payload(query_response_message.add_payload(), p.command(), gbp_status_to_gbp_nagios(p.result().code()), p.result().message(), "");
			}
			data = query_response_message.SerializeAsString();
		}

		void functions::make_exec_from_submit(std::string &data) {
			PB::Commands::SubmitResponseMessage submit_response_message;
			submit_response_message.ParseFromString(data);
			PB::Commands::ExecuteResponseMessage exec_response_message;
			exec_response_message.mutable_header()->CopyFrom(submit_response_message.header());
			for (int i = 0; i < submit_response_message.payload_size(); ++i) {
				PB::Commands::SubmitResponseMessage::Response p = submit_response_message.payload(i);
				append_simple_exec_response_payload(exec_response_message.add_payload(), p.command(), gbp_status_to_gbp_nagios(p.result().code()), p.result().message());
			}
			data = exec_response_message.SerializeAsString();
		}

		void functions::make_return_header(PB::Common::Header *target, const PB::Common::Header &source) {
			target->CopyFrom(source);
			target->set_source_id(target->recipient_id());
		}

		void functions::create_simple_submit_request(std::string channel, std::string command, NSCAPI::nagiosReturn ret, std::string msg, std::string perf, std::string &buffer) {
			PB::Commands::SubmitRequestMessage message;
			message.set_channel(channel);

			PB::Commands::QueryResponseMessage::Response *payload = message.add_payload();
			payload->set_command(command);
			payload->set_result(nagios_status_to_gpb(ret));
			::PB::Commands::QueryResponseMessage_Response_Line* l = payload->add_lines();
			l->set_message(msg);
			if (!perf.empty())
				parse_performance_data(l, perf);

			message.SerializeToString(&buffer);
		}

		void functions::create_simple_submit_response_ok(const std::string channel, const std::string command, const std::string msg, std::string &buffer) {
			PB::Commands::SubmitResponseMessage message;

			PB::Commands::SubmitResponseMessage::Response *payload = message.add_payload();
			payload->set_command(command);
			payload->mutable_result()->set_message(msg);
			payload->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
			message.SerializeToString(&buffer);
		}
		bool functions::parse_simple_submit_response(const std::string &request, std::string &response) {
			PB::Commands::SubmitResponseMessage message;
			message.ParseFromString(request);

			if (message.payload_size() != 1) {
				THROW_INVALID_SIZE(message.payload_size());
			}
			::PB::Commands::SubmitResponseMessage::Response payload = message.payload().Get(0);
			response = payload.mutable_result()->message();
			return payload.mutable_result()->code() == PB::Common::Result_StatusCodeType_STATUS_OK;
		}
		void functions::create_simple_query_request(std::string command, std::list<std::string> arguments, std::string &buffer) {
			PB::Commands::QueryRequestMessage message;

			PB::Commands::QueryRequestMessage::Request *payload = message.add_payload();
			payload->set_command(command);

			for(std::string s: arguments) {
				payload->add_arguments(s);
			}

			message.SerializeToString(&buffer);
		}
		void functions::create_simple_query_request(std::string command, std::vector<std::string> arguments, std::string &buffer) {
			PB::Commands::QueryRequestMessage message;

			PB::Commands::QueryRequestMessage::Request *payload = message.add_payload();
			payload->set_command(command);

			for(std::string s: arguments) {
				payload->add_arguments(s);
			}

			message.SerializeToString(&buffer);
		}

		void functions::append_simple_query_response_payload(PB::Commands::QueryResponseMessage::Response *payload, std::string command, NSCAPI::nagiosReturn ret, std::string msg, std::string perf) {
			payload->set_command(command);
			payload->set_result(nagios_status_to_gpb(ret));
			PB::Commands::QueryResponseMessage::Response::Line *l = payload->add_lines();
			l->set_message(msg);
			if (!perf.empty())
				parse_performance_data(l, perf);
		}
		void functions::append_simple_exec_response_payload(PB::Commands::ExecuteResponseMessage::Response *payload, std::string command, int ret, std::string msg) {
			payload->set_command(command);
			payload->set_message(msg);
			payload->set_result(nagios_status_to_gpb(ret));
		}
		void functions::append_simple_submit_response_payload(PB::Commands::SubmitResponseMessage::Response *payload, std::string command, bool result, std::string msg) {
			payload->set_command(command);
			payload->mutable_result()->set_code(result? PB::Common::Result_StatusCodeType_STATUS_OK:PB::Common::Result_StatusCodeType_STATUS_ERROR);
			payload->mutable_result()->set_message(msg);
		}

		void functions::append_simple_query_request_payload(PB::Commands::QueryRequestMessage::Request *payload, std::string command, std::vector<std::string> arguments) {
			payload->set_command(command);
			for(const std::string &s: arguments) {
				payload->add_arguments(s);
			}
		}

		void functions::append_simple_exec_request_payload(PB::Commands::ExecuteRequestMessage::Request *payload, std::string command, std::vector<std::string> arguments) {
			payload->set_command(command);
			for(const std::string &s: arguments) {
				payload->add_arguments(s);
			}
		}

		void functions::parse_simple_query_request(std::list<std::string> &args, const std::string &request) {

			PB::Commands::QueryRequestMessage message;
			message.ParseFromString(request);

			if (message.payload_size() != 1) {
				THROW_INVALID_SIZE(message.payload_size());
			}
			::PB::Commands::QueryRequestMessage::Request payload = message.payload().Get(0);
			for (int i = 0; i < payload.arguments_size(); i++) {
				args.push_back(payload.arguments(i));
			}
		}
		int functions::parse_simple_query_response(const std::string &response, std::string &msg, std::string &perf, std::size_t max_length) {
			PB::Commands::QueryResponseMessage message;
			message.ParseFromString(response);

			if (message.payload_size() == 0 || message.payload(0).lines_size() == 0) {
				return NSCAPI::query_return_codes::returnUNKNOWN;
			} else if (message.payload_size() > 1 && message.payload(0).lines_size() > 1) {
				THROW_INVALID_SIZE(message.payload_size());
			}

			PB::Commands::QueryResponseMessage::Response payload = message.payload().Get(0);
			for(const PB::Commands::QueryResponseMessage::Response::Line &l: payload.lines()) {
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
			PB::Commands::ExecuteRequestMessage message;
			if (!module.empty()) {
				PB::Common::KeyValue* kvp = message.mutable_header()->add_metadata();
				kvp->set_key("target");
				kvp->set_value(module);
			}

			PB::Commands::ExecuteRequestMessage::Request *payload = message.add_payload();
			payload->set_command(command);

			for(const std::string &s: args)
				payload->add_arguments(s);

			message.SerializeToString(&request);
		}

		void functions::create_simple_exec_request(const std::string &module, const std::string &command, const std::vector<std::string> & args, std::string &request) {
			PB::Commands::ExecuteRequestMessage message;
			if (!module.empty()) {
				PB::Common::KeyValue* kvp = message.mutable_header()->add_metadata();
				kvp->set_key("target");
				kvp->set_value(module);
			}

			PB::Commands::ExecuteRequestMessage::Request *payload = message.add_payload();
			payload->set_command(command);

			for(std::string s: args)
				payload->add_arguments(s);

			message.SerializeToString(&request);
		}
		int functions::parse_simple_exec_response(const std::string &response, std::list<std::string> &result) {
			int ret = 0;
			PB::Commands::ExecuteResponseMessage message;
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
			PB::Commands::ExecuteResponseMessage message;

			PB::Commands::ExecuteResponseMessage::Response *payload = message.add_payload();
			payload->set_command(command);
			payload->set_message(result);

			payload->set_result(nagios_status_to_gpb(ret));
			message.SerializeToString(&response);
			return NSCAPI::cmd_return_codes::isSuccess;
		}
		int functions::create_simple_exec_response_unknown(std::string command, std::string result, std::string &response) {
			PB::Commands::ExecuteResponseMessage message;

			PB::Commands::ExecuteResponseMessage::Response *payload = message.add_payload();
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
		std::string functions::extract_perf_value_as_string(const PB::Common::PerformanceData &perf) {
			if (perf.has_float_value()) {
				const PB::Common::PerformanceData::FloatValue &val = perf.float_value();
				return str::xtos_non_sci(val.value()*get_multiplier(val.unit()));
			} else if (perf.has_string_value()) {
				const PB::Common::PerformanceData::StringValue& val = perf.string_value();
				return val.value();
			}
			return "unknown";
		}
		long long functions::extract_perf_value_as_int(const PB::Common::PerformanceData &perf) {
			if (perf.has_float_value()) {
				const PB::Common::PerformanceData::FloatValue &val = perf.float_value();
				if (!val.unit().empty())
					return static_cast<long long>(val.value()*get_multiplier(val.unit()));
				return static_cast<long long>(val.value());
			} else if (perf.has_string_value()) {
				return 0;
			}
			return 0;
		}
		std::string functions::extract_perf_maximum_as_string(const PB::Common::PerformanceData &perf) {
			if (perf.has_float_value()) {
				const PB::Common::PerformanceData::FloatValue &val = perf.float_value();
				if (!val.unit().empty())
					return str::xtos_non_sci(val.maximum().value()*get_multiplier(val.unit()));
				return str::xtos_non_sci(val.maximum().value());
			}
			return "unknown";
		}

		struct builder {


			virtual void add(std::string alias) = 0;
			virtual void set_value(float value) = 0;
			virtual void set_warning(float value) = 0;
			virtual void set_critical(float value) = 0;
			virtual void set_minimum(float value) = 0;
			virtual void set_maximum(float value) = 0;
			virtual void set_unit(const std::string &value) = 0;

			virtual void add_string(std::string alias, std::string value) = 0;


		};

		struct perf_builder : public parsers::perfdata::builder {

			PB::Commands::QueryResponseMessage::Response::Line *payload;
			PB::Common::PerformanceData* lastPerf = NULL;

			perf_builder(PB::Commands::QueryResponseMessage::Response::Line *payload) : payload(payload) {}


			void add_string(std::string alias, std::string value) {
				PB::Common::PerformanceData* perfData = payload->add_perf();
				perfData->set_alias(alias);
				perfData->mutable_string_value()->set_value(value);
			}

			void add(std::string alias) {
				lastPerf = payload->add_perf();
				lastPerf->set_alias(alias);
			}
			void set_value(double value) {
				lastPerf->mutable_float_value()->set_value(value);
			}
			void set_warning(double value) {
				lastPerf->mutable_float_value()->mutable_warning()->set_value(value);
			}
			void set_critical(double value) {
				lastPerf->mutable_float_value()->mutable_critical()->set_value(value);
			}
			void set_minimum(double value) {
				lastPerf->mutable_float_value()->mutable_minimum()->set_value(value);
			}
			void set_maximum(double value) {
				lastPerf->mutable_float_value()->mutable_maximum()->set_value(value);
			}
			void set_unit(const std::string &value) {
				lastPerf->mutable_float_value()->set_unit(value);
			}
			void next() {}
		};

		void functions::parse_performance_data(PB::Commands::QueryResponseMessage::Response::Line *payload, const std::string &perf) {
			parsers::perfdata::parse(boost::shared_ptr<parsers::perfdata::builder>(new perf_builder(payload)), perf);
		}

		void parse_float_perf_value(std::stringstream &ss, const PB::Common::PerformanceData_FloatValue &val) {
			ss << str::xtos_non_sci(val.value());
			if (!val.unit().empty())
				ss << val.unit();
			if (!val.has_warning() && !val.has_critical() && !val.has_minimum() && !val.has_maximum()) {
				return;
			}
			ss << ";";
			if (val.has_warning())
				ss << str::xtos_non_sci(val.warning().value());
			if (!val.has_critical() && !val.has_minimum() && !val.has_maximum()) {
				return;
			}
			ss << ";";
			if (val.has_critical())
				ss << str::xtos_non_sci(val.critical().value());
			if (!val.has_minimum() && !val.has_maximum()) {
				return;
			}
			ss << ";";
			if (val.has_minimum())
				ss << str::xtos_non_sci(val.minimum().value());
			if (!val.has_maximum()) {
				return;
			}
			ss << ";";
			if (val.has_maximum())
				ss << str::xtos_non_sci(val.maximum().value());
			return;
		}
		/*
		void parse_int_perf_value(std::stringstream &ss, const PB::Common::PerformanceData_IntValue &val) {
			ss << val.value();
			if (!val.unit().empty())
				ss << val.unit();
			if (!val.has_warning() && !val.has_critical() && !val.has_minimum() && !val.has_maximum()) {
				return;
			}
			ss << ";";
			if (val.has_warning())
				ss << val.warning().value();
			if (!val.has_critical() && !val.has_minimum() && !val.has_maximum()) {
				return;
			}
			ss << ";";
			if (val.has_critical())
				ss << val.critical().value();
			if (!val.has_minimum() && !val.has_maximum()) {
				return;
			}
			ss << ";";
			if (val.has_minimum())
				ss << val.minimum().value();
			if (!val.has_maximum()) {
				return;
			}
			ss << ";";
			if (val.has_maximum())
				ss << val.maximum().value();
			return;
		}
		*/
		std::string functions::build_performance_data(PB::Commands::QueryResponseMessage::Response::Line const &payload, std::size_t len) {
			std::string ret;

			bool first = true;
			for (int i = 0; i < payload.perf_size(); i++) {
				std::stringstream ss;
				ss.precision(5);
				PB::Common::PerformanceData perfData = payload.perf(i);
				if (!first)
					ss << " ";
				first = false;
				ss << '\'' << perfData.alias() << "'=";
				if (perfData.has_float_value()) {
					parse_float_perf_value(ss, perfData.float_value());
				//} else if (perfData.has_int_value()) {
				//	parse_int_perf_value(ss, perfData.int_value());
				}
				std::string tmp = ss.str();
				if (len == no_truncation || ret.length() + tmp.length() <= len) {
					ret += tmp;
				}
			}
			return ret;
		}

		PB::Common::ResultCode functions::nagios_status_to_gpb(int ret) {
			if (ret == NSCAPI::query_return_codes::returnOK)
				return PB::Common::ResultCode::OK;
			if (ret == NSCAPI::query_return_codes::returnWARN)
				return PB::Common::ResultCode::WARNING;
			if (ret == NSCAPI::query_return_codes::returnCRIT)
				return PB::Common::ResultCode::CRITICAL;
			return PB::Common::ResultCode::UNKNOWN;
		}

		int functions::gbp_to_nagios_status(PB::Common::ResultCode ret) {
			if (ret == PB::Common::ResultCode::OK)
				return NSCAPI::query_return_codes::returnOK;
			if (ret == PB::Common::ResultCode::WARNING)
				return NSCAPI::query_return_codes::returnWARN;
			if (ret == PB::Common::ResultCode::CRITICAL)
				return NSCAPI::query_return_codes::returnCRIT;
			return NSCAPI::query_return_codes::returnUNKNOWN;
		}

		void functions::set_response_bad(::PB::Commands::QueryResponseMessage_Response &response, std::string message) {
			response.set_result(PB::Common::ResultCode::UNKNOWN);
			response.add_lines()->set_message(message);
			if (response.command().empty())
				response.set_command("unknown");
		}

		void functions::set_response_bad(::PB::Commands::SubmitResponseMessage::Response &response, std::string message) {
			response.mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_ERROR);
			response.mutable_result()->set_message(message);
			if (response.command().empty())
				response.set_command("unknown");
		}

		void functions::set_response_bad(::PB::Commands::ExecuteResponseMessage::Response &response, std::string message) {
			response.set_result(PB::Common::ResultCode::UNKNOWN);
			response.set_message(message);
			if (response.command().empty())
				response.set_command("unknown");
		}

		PB::Common::ResultCode functions::parse_nagios(const std::string &status) {
			std::string lcstat = boost::to_lower_copy(status);
			if (lcstat == "o" || lcstat == "ok" || lcstat == "0")
				return PB::Common::ResultCode::OK;
			if (lcstat == "w" || lcstat == "warn" || lcstat == "warning" || lcstat == "1")
				return PB::Common::ResultCode::WARNING;
			if (lcstat == "c" || lcstat == "crit" || lcstat == "critical" || lcstat == "2")
				return PB::Common::ResultCode::CRITICAL;
			return PB::Common::ResultCode::UNKNOWN;
		}


		void functions::copy_response(const std::string command, ::PB::Commands::QueryResponseMessage::Response* target, const ::PB::Commands::ExecuteResponseMessage::Response source) {
			::PB::Commands::QueryResponseMessage::Response::Line* line = target->add_lines();
			line->set_message(source.message());
			target->set_command(command);
		}
		void functions::copy_response(const std::string command, ::PB::Commands::QueryResponseMessage::Response* target, const ::PB::Commands::SubmitResponseMessage::Response source) {
			::PB::Commands::QueryResponseMessage::Response::Line* line = target->add_lines();
			line->set_message(source.result().message());
			target->set_command(command);
			target->set_result(gbp_status_to_gbp_nagios(source.result().code()));
		}
		void functions::copy_response(const std::string command, ::PB::Commands::QueryResponseMessage::Response* target, const ::PB::Commands::QueryResponseMessage::Response source) {
			target->CopyFrom(source);
		}
		void functions::copy_response(const std::string command, ::PB::Commands::ExecuteResponseMessage::Response* target, const ::PB::Commands::ExecuteResponseMessage::Response source) {
			target->CopyFrom(source);
		}
		void functions::copy_response(const std::string command, ::PB::Commands::ExecuteResponseMessage::Response* target, const ::PB::Commands::SubmitResponseMessage::Response source) {
			target->set_message(source.result().message());
			target->set_command(source.command());
			target->set_result(gbp_status_to_gbp_nagios(source.result().code()));
		}
		void functions::copy_response(const std::string command, ::PB::Commands::ExecuteResponseMessage::Response* target, const ::PB::Commands::QueryResponseMessage::Response source) {
			target->set_message(query_data_to_nagios_string(source, no_truncation));
			target->set_command(source.command());
			target->set_result(source.result());
		}
		void functions::copy_response(const std::string command, ::PB::Commands::SubmitResponseMessage::Response* target, const ::PB::Commands::ExecuteResponseMessage::Response source) {
			target->mutable_result()->set_message(source.message());
		}
		void functions::copy_response(const std::string command, ::PB::Commands::SubmitResponseMessage::Response* target, const ::PB::Commands::SubmitResponseMessage::Response source) {
			target->CopyFrom(source);
		}
		void functions::copy_response(const std::string command, ::PB::Commands::SubmitResponseMessage::Response* target, const ::PB::Commands::QueryResponseMessage::Response source) {
			target->mutable_result()->set_message(query_data_to_nagios_string(source, no_truncation));
			target->mutable_result()->set_code(gbp_to_nagios_gbp_status(source.result()));
		}
	}
}