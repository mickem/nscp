#pragma once

#include <NSCAPI.h>
#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/tuple/tuple.hpp>

#include <nscapi/nscapi_protobuf_types.hpp>

#include <protobuf/plugin.pb.h>

namespace client {

	struct cli_exception : public std::exception {
		std::string error_;
	public:
		cli_exception(std::string error) : error_(error) {}
		~cli_exception() throw() {}
		const char* what() const throw() {
			return error_.c_str();
		}
	};

	struct nscp_cli_data {
		std::wstring target_id;
		std::wstring command;
		std::wstring command_line;
		std::wstring message;
		std::wstring result;
		std::vector<std::wstring> arguments;

		nscapi::protobuf::types::destination_container host_self;
		nscapi::protobuf::types::destination_container recipient;

		int timeout;

		nscp_cli_data() : timeout(10) {}
		std::wstring to_wstring() {
			std::wstringstream ss;
			ss << _T("Timeout: ") << timeout;
			ss << _T(", command: ") << command;
			ss << _T(", target: ") << target_id;
			ss << _T(", self: {") << utf8::cvt<std::wstring>(host_self.to_string()) << _T("}");
			ss << _T(", recipient: {") << utf8::cvt<std::wstring>(recipient.to_string()) << _T("}");
			ss << _T(", message: ") << message;
			ss << _T(", result: ") << result;
			int i=0;
			BOOST_FOREACH(std::wstring a, arguments) {
				ss << _T(", argument[") << i++ << _T("]: ") << a;
			}
			return ss.str();
		}
	};

	struct clp_handler;

	struct target_lookup_interface {
		virtual nscapi::protobuf::types::destination_container lookup_target(std::wstring &id) = 0;
	};
	struct configuration : public boost::noncopyable {
		typedef boost::shared_ptr<nscp_cli_data> data_type;
		typedef boost::shared_ptr<clp_handler> handler_type;
		typedef boost::shared_ptr<target_lookup_interface> target_lookup_type;

		std::string title;
		std::string default_command;
		boost::program_options::options_description local;
		data_type data;
		handler_type handler;
		target_lookup_type target_lookup;

		configuration(std::string caption) : data(data_type(new nscp_cli_data())), local("Common options for " + caption) {}

		bool validate() {
			if (!data) return false;
			if (!handler) return false;
			//if (!target_lookup) return false;
			return true;
		}
		std::string to_string() {
			std::stringstream ss;
			ss << "Title: " << title;
			ss << ", data: " << data;
			ss << ", handler: " << handler;
			ss << ", target_lookup: " << target_lookup;
			return ss.str();
		}

	};
	struct command_container {
		std::string command;
		std::string key;
		std::list<std::string> arguments;

		command_container() {}
		command_container(const command_container &other) : command(other.command), key(other.key), arguments(other.arguments) {}
		const command_container& operator=(const command_container &other) {
			command = other.command;
			arguments = other.arguments;
			key = other.key;
			return *this;
		}
	};

	struct clp_handler {
		virtual int query(client::configuration::data_type data, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) = 0;
		virtual int submit(client::configuration::data_type data, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage &response_message) = 0;
		virtual int exec(client::configuration::data_type data, const Plugin::ExecuteRequestMessage &request_message, Plugin::ExecuteResponseMessage &response_message) = 0;
	};
	struct command_manager {
		typedef boost::unordered_map<std::string, command_container> command_type;
		command_type commands;

		std::wstring add_command(std::wstring name, std::wstring args);
		int exec_simple(configuration &config, const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::string &response);

		// Wrappers based on source
		void parse_query(const std::string &prefix, const std::string &default_command, const std::string &cmd, client::configuration &config, const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response &response, const Plugin::QueryRequestMessage &request_message);
		bool parse_exec(const std::string &prefix, const std::string &default_command, const std::string &cmd, client::configuration &config, const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response &response, const Plugin::ExecuteRequestMessage &request_message);
		void parse_submit(const std::string &prefix, const std::string &default_command, const std::string &cmd, client::configuration &config, const Plugin::QueryResponseMessage::Response &request, Plugin::SubmitResponseMessage::Response &response, const Plugin::SubmitRequestMessage &request_message);

		// Actual execution
		void do_query(client::configuration &config, const ::Plugin::Common::Header &header, Plugin::QueryResponseMessage::Response &response);
		void do_exec(client::configuration &config, const ::Plugin::Common::Header &header, Plugin::ExecuteResponseMessage::Response &response);
		void do_submit(client::configuration &config, const ::Plugin::Common::Header &header, Plugin::SubmitResponseMessage::Response &response);
		
		void forward_query(client::configuration &config, const Plugin::QueryRequestMessage &request, Plugin::QueryResponseMessage &response);
		void forward_exec(client::configuration &config, const Plugin::ExecuteRequestMessage &request, Plugin::ExecuteResponseMessage::Response &response);
		void forward_submit(client::configuration &config, const Plugin::SubmitRequestMessage &request, Plugin::SubmitResponseMessage &response);
	};
}
