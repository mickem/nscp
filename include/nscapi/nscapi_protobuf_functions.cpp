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
#include <boost/noncopyable.hpp>

#include <utf8.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>

#define THROW_INVALID_SIZE(size) \
	throw nscapi_exception(std::string("Whoops, invalid payload size: ") + strEx::s::xtos(size) + " != 1 at line " + strEx::s::xtos(__LINE__));

namespace nscapi {
	namespace protobuf {
		namespace traits {
			template<class T>
			struct perf_data_consts {
				static const T get_valid_perf_numbers();
				static const T get_replace_perf_coma_src();
				static const T get_replace_perf_coma_tgt();
			};

// 			template<>
// 			struct perf_data_consts<std::wstring> {
// 				static const std::wstring get_valid_perf_numbers() {
// 					return _T("0123456789,.-");
// 				}
// 				static const std::wstring get_replace_perf_coma_src() {
// 					return _T(",");
// 				}
// 				static const std::wstring get_replace_perf_coma_tgt() {
// 					return _T(".");
// 				}
// 			};
			template<>
			struct perf_data_consts<std::string> {
				static const std::string get_valid_perf_numbers() {
					return "0123456789,.-";
				}
				static const std::string get_replace_perf_coma_src() {
					return ",";
				}
				static const std::string get_replace_perf_coma_tgt() {
					return ".";
				}
			};
		}

		template<class T>
		double trim_to_double(T s) {
			typename T::size_type pend = s.find_first_not_of(traits::perf_data_consts<T>::get_valid_perf_numbers());
			if (pend != T::npos)
				s = s.substr(0,pend);
			strEx::replace(s, traits::perf_data_consts<T>::get_replace_perf_coma_src(), traits::perf_data_consts<T>::get_replace_perf_coma_tgt());
			if (s.empty()) {
				return 0.0;
			}
			try {
				return strEx::stod(s);
			} catch (...) {
				return 0.0;
			}
		}

		void functions::create_simple_header(Plugin::Common::Header* hdr)  {
			hdr->set_version(Plugin::Common_Version_VERSION_1);
			hdr->set_max_supported_version(Plugin::Common_Version_VERSION_1);
			// @todo add additional fields here!
		}

		//////////////////////////////////////////////////////////////////////////

		void functions::add_host(Plugin::Common::Header* hdr, const destination_container &dst)  {
			::Plugin::Common::Host *host = hdr->add_hosts();
			if (!dst.id.empty())
				host->set_id(dst.id);
			if (!dst.address.host.empty())
				host->set_address(dst.address.to_string());
			if (!dst.has_protocol())
				host->set_protocol(dst.get_protocol());
			if (!dst.comment.empty())
				host->set_comment(dst.comment);
			BOOST_FOREACH(const std::string &t, dst.tags) {
				host->add_tags(t);
			}
			BOOST_FOREACH(const destination_container::data_map::value_type &kvp, dst.data) {
				::Plugin::Common_KeyValue* x = host->add_metadata();
				x->set_key(kvp.first);
				x->set_value(kvp.second);
			}
		}

		bool functions::parse_destination(const ::Plugin::Common_Header &header, const std::string tag, destination_container &data, const bool expand_meta) {
			for (int i=0;i<header.hosts_size();++i) {
				const ::Plugin::Common::Host &host = header.hosts(i);
				if (host.id() == tag) {
					data.id = tag;
					if (!host.address().empty())
						data.address.import(net::parse(host.address()));
					if (!host.comment().empty())
						data.comment = host.comment();
					if (expand_meta) {
						for(int j=0;j<host.tags_size(); ++j) {
							data.tags.insert(host.tags(j));
						}
						for(int j=0;j<host.metadata_size(); ++j) {
							data.data[host.metadata(j).key()] = host.metadata(j).value();
						}
					}
					return true;
				}
			}
			return false;
		}

		//////////////////////////////////////////////////////////////////////////

		void functions::make_submit_from_query(std::string &message, const std::string channel, const std::string alias, const std::string target, const std::string source) {
			Plugin::QueryResponseMessage response;
			response.ParseFromString(message);
			Plugin::SubmitRequestMessage request;
			request.mutable_header()->CopyFrom(response.header());
			request.mutable_header()->set_source_id(request.mutable_header()->recipient_id());
			for (int i=0;i<request.mutable_header()->hosts_size();i++) {
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
				for (int i=0;i<request.mutable_header()->hosts_size();i++) {
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
			for (int i=0;i<response.payload_size();++i) {
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
			for (int i=0;i<exec_response_message.payload_size();++i) {
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
			for (int i=0;i<submit_response_message.payload_size();++i) {
				Plugin::SubmitResponseMessage::Response p = submit_response_message.payload(i);
				append_simple_query_response_payload(query_response_message.add_payload(), p.command(), gbp_status_to_gbp_nagios(p.status().status()), p.status().message(), "");
			}
			data = query_response_message.SerializeAsString();
		}

		void functions::make_exec_from_submit(std::string &data) {
			Plugin::SubmitResponseMessage submit_response_message;
			submit_response_message.ParseFromString(data);
			Plugin::ExecuteResponseMessage exec_response_message;
			exec_response_message.mutable_header()->CopyFrom(submit_response_message.header());
			for (int i=0;i<submit_response_message.payload_size();++i) {
				Plugin::SubmitResponseMessage::Response p = submit_response_message.payload(i);
				append_simple_exec_response_payload(exec_response_message.add_payload(), p.command(), gbp_status_to_gbp_nagios(p.status().status()), p.status().message());
			}
			data = exec_response_message.SerializeAsString();
		}
		void functions::make_exec_from_query(std::string &data) {
			Plugin::QueryResponseMessage query_response_message;
			query_response_message.ParseFromString(data);
			Plugin::ExecuteResponseMessage exec_response_message;
			exec_response_message.mutable_header()->CopyFrom(query_response_message.header());
			for (int i=0;i<query_response_message.payload_size();++i) {
				Plugin::QueryResponseMessage::Response p = query_response_message.payload(i);
				std::string s = build_performance_data(p);
				if (!s.empty())
					s = p.message() + "|" + s;
				else
					s = p.message();
				append_simple_exec_response_payload(exec_response_message.add_payload(), p.command(), p.result(), s);
			}
			data = exec_response_message.SerializeAsString();
		}

		void functions::make_return_header(::Plugin::Common_Header *target, const ::Plugin::Common_Header &source) {
			target->CopyFrom(source);
			target->set_source_id(target->recipient_id());
		}

		void functions::create_simple_submit_request(std::string channel, std::string command, NSCAPI::nagiosReturn ret, std::string msg, std::string perf, std::string &buffer) {
			Plugin::SubmitRequestMessage message;
			create_simple_header(message.mutable_header());
			message.set_channel(channel);

			Plugin::QueryResponseMessage::Response *payload = message.add_payload();
			payload->set_command(command);
			payload->set_message(msg);
			payload->set_result(nagios_status_to_gpb(ret));
			if (!perf.empty())
				parse_performance_data(payload, perf);

			message.SerializeToString(&buffer);
		}

		void functions::create_simple_submit_response(const std::string channel, const std::string command, const NSCAPI::nagiosReturn ret, const std::string msg, std::string &buffer) {
			Plugin::SubmitResponseMessage message;
			create_simple_header(message.mutable_header());
			//message.set_channel(to_string(channel));

			Plugin::SubmitResponseMessage::Response *payload = message.add_payload();
			payload->set_command(command);
			payload->mutable_status()->set_message(msg);
			payload->mutable_status()->set_status(status_to_gpb(ret));
			message.SerializeToString(&buffer);
		}
		NSCAPI::errorReturn functions::parse_simple_submit_response(const std::string &request, std::string &response) {
			Plugin::SubmitResponseMessage message;
			message.ParseFromString(request);

			if (message.payload_size() != 1) {
				THROW_INVALID_SIZE(message.payload_size());
			}
			::Plugin::SubmitResponseMessage::Response payload = message.payload().Get(0);
			response = payload.mutable_status()->message();
			return gbp_to_status(payload.mutable_status()->status());
		}
		void functions::create_simple_query_request(std::string command, std::list<std::string> arguments, std::string &buffer) {
			Plugin::QueryRequestMessage message;
			create_simple_header(message.mutable_header());

			Plugin::QueryRequestMessage::Request *payload = message.add_payload();
			payload->set_command(utf8::cvt<std::string>(command));

			BOOST_FOREACH(std::string s, arguments) {
				payload->add_arguments(utf8::cvt<std::string>(s));
			}

			message.SerializeToString(&buffer);
		}
		void functions::create_simple_query_request(std::string command, std::vector<std::string> arguments, std::string &buffer) {
			Plugin::QueryRequestMessage message;
			create_simple_header(message.mutable_header());

			Plugin::QueryRequestMessage::Request *payload = message.add_payload();
			payload->set_command(utf8::cvt<std::string>(command));

			BOOST_FOREACH(std::string s, arguments) {
				payload->add_arguments(utf8::cvt<std::string>(s));
			}

			message.SerializeToString(&buffer);
		}

		NSCAPI::nagiosReturn functions::create_simple_query_response_unknown(std::string command, std::string msg, std::string &buffer) {
			Plugin::QueryResponseMessage message;
			create_simple_header(message.mutable_header());

			::Plugin::QueryResponseMessage::Response *payload = message.add_payload();
			payload->set_command(command);
			payload->set_message(msg);
			payload->set_result(Plugin::Common_ResultCode_UNKNOWN);

			message.SerializeToString(&buffer);
			return NSCAPI::returnUNKNOWN;
		}
		NSCAPI::nagiosReturn functions::create_simple_query_response(std::string command, NSCAPI::nagiosReturn ret, std::string msg, std::string perf, std::string &buffer) {
			Plugin::QueryResponseMessage message;
			create_simple_header(message.mutable_header());

			append_simple_query_response_payload(message.add_payload(), command, ret, msg, perf);

			message.SerializeToString(&buffer);
			return ret;
		}
		NSCAPI::nagiosReturn functions::create_simple_query_response(std::string command, NSCAPI::nagiosReturn ret, std::string msg, std::string &buffer) {
			Plugin::QueryResponseMessage message;
			create_simple_header(message.mutable_header());

			::Plugin::QueryResponseMessage::Response *payload = message.add_payload();
			payload->set_command(utf8::cvt<std::string>(command));
			payload->set_message(utf8::cvt<std::string>(msg));
			payload->set_result(nagios_status_to_gpb(ret));

			message.SerializeToString(&buffer);
			return ret;
		}

		void functions::append_simple_submit_request_payload(Plugin::QueryResponseMessage::Response *payload, std::string command, NSCAPI::nagiosReturn ret, std::string msg, std::string perf) {
			payload->set_command(command);
			payload->set_message(msg);
			payload->set_result(nagios_status_to_gpb(ret));
			if (!perf.empty())
				parse_performance_data(payload, perf);
		}

		void functions::append_simple_query_response_payload(Plugin::QueryResponseMessage::Response *payload, std::string command, NSCAPI::nagiosReturn ret, std::string msg, std::string perf) {
			payload->set_command(command);
			payload->set_message(msg);
			payload->set_result(nagios_status_to_gpb(ret));
			if (!perf.empty())
				parse_performance_data(payload, perf);
		}
		void functions::append_simple_exec_response_payload(Plugin::ExecuteResponseMessage::Response *payload, std::string command, int ret, std::string msg) {
			payload->set_command(command);
			payload->set_message(msg);
			payload->set_result(nagios_status_to_gpb(ret));
		}
		void functions::append_simple_submit_response_payload(Plugin::SubmitResponseMessage::Response *payload, std::string command, int ret, std::string msg) {
			payload->set_command(command);
			payload->mutable_status()->set_status(status_to_gpb(ret));
			payload->mutable_status()->set_message(msg);
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
			decoded_simple_command_data data;

			Plugin::QueryRequestMessage message;
			message.ParseFromString(request);

			if (message.payload_size() != 1) {
				THROW_INVALID_SIZE(message.payload_size());
			}
			::Plugin::QueryRequestMessage::Request payload = message.payload().Get(0);
			for (int i=0;i<payload.arguments_size();i++) {
				args.push_back(payload.arguments(i));
			}
		}
		functions::decoded_simple_command_data functions::parse_simple_query_request(const std::string command, const std::string &request) {
			decoded_simple_command_data data;

			data.command = command;
			Plugin::QueryRequestMessage message;
			message.ParseFromString(request);

			if (message.payload_size() != 1) {
				THROW_INVALID_SIZE(message.payload_size());
			}
			::Plugin::QueryRequestMessage::Request payload = message.payload().Get(0);
			for (int i=0;i<payload.arguments_size();i++) {
				data.args.push_back(payload.arguments(i));
			}
			return data;
		}
		functions::decoded_simple_command_data functions::parse_simple_query_request(const char* char_command, const std::string &request) {
			decoded_simple_command_data data;

			data.command = char_command;
			Plugin::QueryRequestMessage message;
			message.ParseFromString(request);

			if (message.payload_size() != 1) {
				THROW_INVALID_SIZE(message.payload_size());
			}
			::Plugin::QueryRequestMessage::Request payload = message.payload().Get(0);
			for (int i=0;i<payload.arguments_size();i++) {
				data.args.push_back(payload.arguments(i));
			}
			return data;
		}
		functions::decoded_simple_command_data functions::parse_simple_query_request(const ::Plugin::QueryRequestMessage::Request &payload) {
			decoded_simple_command_data data;
			data.command = payload.command();
			for (int i=0;i<payload.arguments_size();i++) {
				data.args.push_back(payload.arguments(i));
			}
			return data;
		}
		int functions::parse_simple_query_response(const std::string &response, std::string &msg, std::string &perf) {
			Plugin::QueryResponseMessage message;
			message.ParseFromString(response);

			if (message.payload_size() == 0) {
				return NSCAPI::returnUNKNOWN;
			} else if (message.payload_size() > 1) {
				THROW_INVALID_SIZE(message.payload_size());
			}

			Plugin::QueryResponseMessage::Response payload = message.payload().Get(0);
			msg = utf8::cvt<std::string>(payload.message());
			perf = utf8::cvt<std::string>(build_performance_data(payload));
			return gbp_to_nagios_status(payload.result());
		}

		//////////////////////////////////////////////////////////////////////////

		void functions::create_simple_exec_request(const std::string &command, const std::list<std::string> & args, std::string &request) {
			Plugin::ExecuteRequestMessage message;
			create_simple_header(message.mutable_header());

			Plugin::ExecuteRequestMessage::Request *payload = message.add_payload();
			payload->set_command(command);

			BOOST_FOREACH(const std::string &s, args)
				payload->add_arguments(s);

			message.SerializeToString(&request);
		}

		void functions::create_simple_exec_request(const std::string &command, const std::vector<std::string> & args, std::string &request) {
			Plugin::ExecuteRequestMessage message;
			create_simple_header(message.mutable_header());

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

			for (int i=0;i<message.payload_size(); i++) {
				result.push_back(utf8::cvt<std::string>(message.payload(i).message()));
				int r=gbp_to_nagios_status(message.payload(i).result());
				if (r > ret)
					ret = r;
			}
			return ret;
		}
		functions::decoded_simple_command_data functions::parse_simple_exec_request(const std::string &request) {
			Plugin::ExecuteRequestMessage message;
			message.ParseFromString(request);

			return parse_simple_exec_request(message);
		}
		functions::decoded_simple_command_data functions::parse_simple_exec_request(const Plugin::ExecuteRequestMessage &message) {
			decoded_simple_command_data data;
			if (message.has_header())
				data.target = message.header().recipient_id();

			if (message.payload_size() != 1) {
				THROW_INVALID_SIZE(message.payload_size());
			}
			Plugin::ExecuteRequestMessage::Request payload = message.payload().Get(0);
			data.command = payload.command();
			for (int i=0;i<payload.arguments_size();i++) {
				data.args.push_back(payload.arguments(i));
			}
			return data;
		}
		functions::decoded_simple_command_data functions::parse_simple_exec_request_payload(const Plugin::ExecuteRequestMessage::Request &payload) {
			decoded_simple_command_data data;
			data.command = payload.command();
			for (int i=0;i<payload.arguments_size();i++) {
				data.args.push_back(payload.arguments(i));
			}
			return data;
		}
		int functions::create_simple_exec_response(const std::string &command, NSCAPI::nagiosReturn ret, const std::string result, std::string &response) {
			Plugin::ExecuteResponseMessage message;
			create_simple_header(message.mutable_header());

			Plugin::ExecuteResponseMessage::Response *payload = message.add_payload();
			payload->set_command(command);
			payload->set_message(result);

			payload->set_result(nagios_status_to_gpb(ret));
			message.SerializeToString(&response);
			return ret;
		}
		int functions::create_simple_exec_response_unknown(std::string command, std::string result, std::string &response) {
			Plugin::ExecuteResponseMessage message;
			create_simple_header(message.mutable_header());

			Plugin::ExecuteResponseMessage::Response *payload = message.add_payload();
			payload->set_command(command);
			payload->set_message(result);

			payload->set_result(nagios_status_to_gpb(NSCAPI::returnUNKNOWN));
			message.SerializeToString(&response);
			return NSCAPI::returnUNKNOWN;
		}

		//////////////////////////////////////////////////////////////////////////

		long long get_multiplier(const std::string &unit) {
			if (unit.empty())
				return 1;
			if (unit[0] == 'K')
				return 1024ll;
			if (unit[0] == 'M')
				return 1024ll*1024ll;
			if (unit[0] == 'G')
				return 1024ll*1024ll*1024ll;
			if (unit[0] == 'T')
				return 1024ll*1024ll*1024ll*1024ll;
			return 1;
		}
		std::string functions::extract_perf_value_as_string(const ::Plugin::Common_PerformanceData &perf) {
			if (perf.has_int_value()) {
				const Plugin::Common::PerformanceData::IntValue &val = perf.int_value();
				if (val.has_unit())
					return strEx::s::xtos_non_sci(val.value()*get_multiplier(val.unit()));
				return strEx::s::xtos_non_sci(val.value());
			} else if (perf.has_bool_value()) {
				const Plugin::Common::PerformanceData::BoolValue &val = perf.bool_value();
				return val.value()?"true":"false";
			} else if (perf.has_float_value()) {
				const Plugin::Common::PerformanceData::FloatValue &val = perf.float_value();
				return strEx::s::xtos_non_sci(val.value()*get_multiplier(val.unit()));
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
				return val.value()?1:0;
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
					return strEx::s::xtos_non_sci(val.maximum()*get_multiplier(val.unit()));
				return strEx::s::xtos_non_sci(val.maximum());
			} else if (perf.has_bool_value() || perf.has_string_value()) {
				return "";
			} else if (perf.has_float_value()) {
				const Plugin::Common::PerformanceData::FloatValue &val = perf.float_value();
				if (val.has_unit())
					return strEx::s::xtos_non_sci(val.maximum()*get_multiplier(val.unit()));
				return strEx::s::xtos_non_sci(val.maximum());
			}
			return "unknown";
		}

		void functions::parse_performance_data(Plugin::QueryResponseMessage::Response *payload, const std::string &perff) {
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
					p = perf.find(perf_lable_enclosure[0], 1)+1;
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
				strEx::split(items, chunk, perf_item_splitter);
				if (items.size() < 1) {
					Plugin::Common::PerformanceData* perfData = payload->add_perf();
					perfData->set_type(Plugin::Common_DataType_STRING);
					std::pair<std::string,std::string> fitem = strEx::split("", perf_equal_sign);
					perfData->set_alias("invalid");
					Plugin::Common_PerformanceData_StringValue* stringPerfData = perfData->mutable_string_value();
					stringPerfData->set_value("invalid performance data");
					break;
				}

				std::pair<std::string,std::string> fitem = strEx::split(items[0], perf_equal_sign);
				std::string alias = fitem.first;
				if (alias.size() > 0 && alias[0] == perf_lable_enclosure[0] && alias[alias.size()-1] == perf_lable_enclosure[0])
					alias = alias.substr(1, alias.size()-2);

				if (alias.empty())
					continue;
				Plugin::Common::PerformanceData* perfData = payload->add_perf();
				perfData->set_type(Plugin::Common_DataType_FLOAT);
				perfData->set_alias(utf8::cvt<std::string>(alias));
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
					floatPerfData->set_value(trim_to_double(fitem.second.substr(0,pend)));
					floatPerfData->set_unit(utf8::cvt<std::string>(fitem.second.substr(pend)));
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

		std::string functions::build_performance_data(Plugin::QueryResponseMessage::Response const &payload) {
			std::stringstream ss;
			ss.precision(5);

			bool first = true;
			for (int i=0;i<payload.perf_size();i++) {
				Plugin::Common::PerformanceData perfData = payload.perf(i);
				if (!first)
					ss << " ";
				first = false;
				ss << '\'' << perfData.alias() << "'=";
				if (perfData.has_float_value()) {
					Plugin::Common_PerformanceData_FloatValue fval = perfData.float_value();

					ss << strEx::s::xtos_non_sci(fval.value());
					if (fval.has_unit())
						ss << fval.unit();
					if (!fval.has_warning() && !fval.has_critical() && !fval.has_minimum() && !fval.has_maximum())
						continue;
					ss << ";";
					if (fval.has_warning())
						ss << strEx::s::xtos_non_sci(fval.warning());
					if (!fval.has_critical() && !fval.has_minimum() && !fval.has_maximum())
						continue;
					ss << ";";
					if (fval.has_critical())
						ss << strEx::s::xtos_non_sci(fval.critical());
					if (!fval.has_minimum() && !fval.has_maximum())
						continue;
					ss << ";";
					if (fval.has_minimum())
						ss << strEx::s::xtos_non_sci(fval.minimum());
					if (!fval.has_maximum())
						continue;
					ss << ";";
					if (fval.has_maximum())
						ss << strEx::s::xtos_non_sci(fval.maximum());
				} else if (perfData.has_int_value()) {
					Plugin::Common_PerformanceData_IntValue fval = perfData.int_value();
					ss << fval.value();
					if (fval.has_unit())
						ss << fval.unit();
					if (!fval.has_warning() && !fval.has_critical() && !fval.has_minimum() && !fval.has_maximum())
						continue;
					ss << ";";
					if (fval.has_warning())
						ss << fval.warning();
					if (!fval.has_critical() && !fval.has_minimum() && !fval.has_maximum())
						continue;
					ss << ";";
					if (fval.has_critical())
						ss << fval.critical();
					if (!fval.has_minimum() && !fval.has_maximum())
						continue;
					ss << ";";
					if (fval.has_minimum())
						ss << fval.minimum();
					if (!fval.has_maximum())
						continue;
					ss << ";";
					if (fval.has_maximum())
						ss << fval.maximum();
				}
			}
			return ss.str();
		}

		Plugin::Common::ResultCode functions::nagios_status_to_gpb(int ret)
		{
			if (ret == NSCAPI::returnOK)
				return Plugin::Common_ResultCode_OK;
			if (ret == NSCAPI::returnWARN)
				return Plugin::Common_ResultCode_WARNING;
			if (ret == NSCAPI::returnCRIT)
				return Plugin::Common_ResultCode_CRITCAL;
			return Plugin::Common_ResultCode_UNKNOWN;
		}

		int functions::gbp_to_nagios_status(Plugin::Common::ResultCode ret)
		{
			if (ret == Plugin::Common_ResultCode_OK)
				return NSCAPI::returnOK;
			if (ret == Plugin::Common_ResultCode_WARNING)
				return NSCAPI::returnWARN;
			if (ret == Plugin::Common_ResultCode_CRITCAL)
				return NSCAPI::returnCRIT;
			return NSCAPI::returnUNKNOWN;
		}

		Plugin::Common::ResultCode functions::parse_nagios(const std::string &status)
		{
			std::string lcstat = boost::to_lower_copy(status);
			if (lcstat == "o" || lcstat == "ok")
				return Plugin::Common_ResultCode_OK;
			if (lcstat == "w" || lcstat == "warn" || lcstat == "warning")
				return Plugin::Common_ResultCode_WARNING;
			if (lcstat == "c" || lcstat == "crit" || lcstat == "critical")
				return Plugin::Common_ResultCode_CRITCAL;
			return Plugin::Common_ResultCode_UNKNOWN;
		}

		NSCAPI::messageTypes functions::gpb_to_log(Plugin::LogEntry::Entry::Level ret)
		{
			if (ret == Plugin::LogEntry_Entry_Level_LOG_CRITICAL)
				return NSCAPI::log_level::critical;
			if (ret == Plugin::LogEntry_Entry_Level_LOG_DEBUG)
				return NSCAPI::log_level::debug;
			if (ret == Plugin::LogEntry_Entry_Level_LOG_ERROR)
				return NSCAPI::log_level::error;
			if (ret == Plugin::LogEntry_Entry_Level_LOG_INFO)
				return NSCAPI::log_level::info;
			if (ret == Plugin::LogEntry_Entry_Level_LOG_WARNING)
				return NSCAPI::log_level::warning;
			return NSCAPI::log_level::error;
		}

		//////////////////////////////////////////////////////////////////////////
		//
		// deprecated: wstring functions
		//
		int functions::parse_simple_submit_request_payload(const Plugin::QueryResponseMessage::Response &payload, std::wstring &alias, std::wstring &message, std::wstring &perf) {
			alias = utf8::cvt<std::wstring>(payload.alias());
			if (alias.empty())
				alias = utf8::cvt<std::wstring>(payload.command());
			message = utf8::cvt<std::wstring>(payload.message());
			perf = utf8::cvt<std::wstring>(build_performance_data(payload));
			return gbp_to_nagios_status(payload.result());
		}
		int functions::parse_simple_submit_request_payload(const Plugin::QueryResponseMessage::Response &payload, std::string &alias, std::string &message, std::string &perf) {
			alias = payload.alias();
			if (alias.empty())
				alias = payload.command();
			message = payload.message();
			perf = build_performance_data(payload);
			return gbp_to_nagios_status(payload.result());
		}
		int functions::parse_simple_submit_request_payload(const Plugin::QueryResponseMessage::Response &payload, std::wstring &alias, std::wstring &message) {
			alias = utf8::cvt<std::wstring>(payload.alias());
			if (alias.empty())
				alias = utf8::cvt<std::wstring>(payload.command());
			message = utf8::cvt<std::wstring>(payload.message());
			return gbp_to_nagios_status(payload.result());
		}

		namespace functions {

			std::string settings_query::key_values::get_string() const {
				if (str_value)
					return *str_value;
				if (int_value)
					return strEx::s::xtos(*int_value);
				if (bool_value)
					return *bool_value?"true":"false";
				return "";
			}

			long long settings_query::key_values::get_int() const {
				if (str_value)
					return strEx::s::stox<long long>(*str_value);
				if (int_value)
					return *int_value;
				if (bool_value)
					return *bool_value?1:0;
				return 0;
			}

			bool settings_query::key_values::get_bool() const {
				if (str_value) {
					std::string s = *str_value;
					std::transform(s.begin(), s.end(), s.begin(), ::tolower);
					return s == "true" || s == "1";
				}
				if (int_value)
					return *int_value == 1;
				if (bool_value)
					return *bool_value;
				return "";
			}


			settings_query::settings_query(int plugin_id) : plugin_id(plugin_id) {
				create_simple_header(request_message.mutable_header());
			}

			void settings_query::set(const std::string path, const std::string key, const std::string value) {
				::Plugin::SettingsRequestMessage::Request *r = request_message.add_payload();
				r->set_plugin_id(plugin_id);
				r->mutable_update()->mutable_node()->set_path(path);
				r->mutable_update()->mutable_node()->set_key(key);
				r->mutable_update()->mutable_value()->set_string_data(value);
				r->mutable_update()->mutable_value()->set_type(::Plugin::Common::DataType::Common_DataType_STRING);
			}
			void settings_query::get(const std::string path, const std::string key, const std::string def) {
				::Plugin::SettingsRequestMessage::Request *r = request_message.add_payload();
				r->set_plugin_id(plugin_id);
				r->mutable_query()->mutable_node()->set_path(path);
				r->mutable_query()->mutable_node()->set_key(key);
				r->mutable_query()->set_type(::Plugin::Common::DataType::Common_DataType_STRING);
				r->mutable_query()->mutable_default_value()->set_string_data(def);
				r->mutable_query()->mutable_default_value()->set_type(::Plugin::Common::DataType::Common_DataType_STRING);
				r->mutable_query()->set_recursive(false);
			}
			void settings_query::get(const std::string path, const std::string key, const char* def) {
				::Plugin::SettingsRequestMessage::Request *r = request_message.add_payload();
				r->set_plugin_id(plugin_id);
				r->mutable_query()->mutable_node()->set_path(path);
				r->mutable_query()->mutable_node()->set_key(key);
				r->mutable_query()->set_type(::Plugin::Common::DataType::Common_DataType_STRING);
				r->mutable_query()->mutable_default_value()->set_string_data(def);
				r->mutable_query()->mutable_default_value()->set_type(::Plugin::Common::DataType::Common_DataType_STRING);
				r->mutable_query()->set_recursive(false);
			}
			void settings_query::get(const std::string path, const std::string key, const long long def) {
				::Plugin::SettingsRequestMessage::Request *r = request_message.add_payload();
				r->set_plugin_id(plugin_id);
				r->mutable_query()->mutable_node()->set_path(path);
				r->mutable_query()->mutable_node()->set_key(key);
				r->mutable_query()->set_type(::Plugin::Common::DataType::Common_DataType_INT);
				r->mutable_query()->mutable_default_value()->set_int_data(def);
				r->mutable_query()->mutable_default_value()->set_type(::Plugin::Common::DataType::Common_DataType_INT);
				r->mutable_query()->set_recursive(false);
			}
			void settings_query::get(const std::string path, const std::string key, const bool def) {
				::Plugin::SettingsRequestMessage::Request *r = request_message.add_payload();
				r->set_plugin_id(plugin_id);
				r->mutable_query()->mutable_node()->set_path(path);
				r->mutable_query()->mutable_node()->set_key(key);
				r->mutable_query()->set_type(::Plugin::Common::DataType::Common_DataType_BOOL);
				r->mutable_query()->mutable_default_value()->set_bool_data(def);
				r->mutable_query()->mutable_default_value()->set_type(::Plugin::Common::DataType::Common_DataType_BOOL);
				r->mutable_query()->set_recursive(false);
			}


			void settings_query::save() {
				::Plugin::SettingsRequestMessage::Request *r = request_message.add_payload();
				r->set_plugin_id(plugin_id);
				r->mutable_control()->set_command(Plugin::Settings_Command_SAVE);
			}
			void settings_query::load() {
				::Plugin::SettingsRequestMessage::Request *r = request_message.add_payload();
				r->set_plugin_id(plugin_id);
				r->mutable_control()->set_command(Plugin::Settings_Command_LOAD);
			}
			void settings_query::reload() {
				::Plugin::SettingsRequestMessage::Request *r = request_message.add_payload();
				r->set_plugin_id(plugin_id);
				r->mutable_control()->set_command(Plugin::Settings_Command_RELOAD);
			}
			const std::string settings_query::request() const {
				return request_message.SerializeAsString();
			}

			bool settings_query::validate_response() {
				response_message.ParsePartialFromString(response_buffer);
				bool ret = true;
				for (int i=0;i<response_message.payload_size();++i) {
					if (response_message.payload(i).result().status() != Plugin::Common::Status::STATUS_OK)
						ret = false;
				}
				return ret;
			}
			std::string settings_query::get_response_error() const {
				std::string ret;
				for (int i=0;i<response_message.payload_size();++i) {
					ret += response_message.payload(i).result().message();
				}
				return ret;
			}
			std::list<settings_query::key_values> settings_query::get_query_key_response() const {
				std::list<key_values> ret;
				for (int i=0;i<response_message.payload_size();++i) {
					::Plugin::SettingsResponseMessage::Response pl = response_message.payload(i);
					if (pl.has_query()) {
						::Plugin::SettingsResponseMessage::Response::Query q = pl.query();
						if (q.node().has_key() && q.has_value()) {
							if (q.value().type() == Plugin::Common_DataType_STRING)
								ret.push_back(key_values(q.node().path(), q.node().key(), q.value().string_data()));
							else if (q.value().type() == Plugin::Common_DataType_INT)
								ret.push_back(key_values(q.node().path(), q.node().key(), q.value().int_data()));
							else if (q.value().type() == Plugin::Common_DataType_BOOL)
								ret.push_back(key_values(q.node().path(), q.node().key(), q.value().bool_data()));
						} else {
							ret.push_back(key_values(q.node().path()));
						}
					}
				}
				return ret;
			}
		}
 	}
}
