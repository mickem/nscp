#pragma once

#include <NSCAPI.h>
#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/tuple/tuple.hpp>

#include <nscapi/functions.hpp>

namespace client {

	namespace po = boost::program_options;

	struct cli_exception : public std::exception {
		std::string error_;
	public:
		cli_exception(std::string error) : error_(error) {}
		~cli_exception() throw() {}
		const char* what() const throw() {
			return error_.c_str();
		}
		const std::wstring wwhat() const throw() {
			return utf8::to_unicode(error_);
		}

	};

	struct nscp_cli_data {
		std::wstring target_id;
		std::wstring command;
		std::wstring command_line;
		std::wstring message;
		std::wstring result;
		std::vector<std::wstring> arguments;

		nscapi::functions::destination_container host_self;
		nscapi::functions::destination_container recipient;

		bool submit;
		bool query;
		bool exec;

		int timeout;
		nscp_cli_data() : timeout(10), submit(false), query(false), exec(false) {}
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
		virtual nscapi::functions::destination_container lookup_target(std::wstring &id) = 0;
	};
	struct configuration /*: boost::noncopyable*/ {
		typedef boost::shared_ptr<nscp_cli_data> data_type;
		typedef boost::shared_ptr<clp_handler> handler_type;
		typedef boost::shared_ptr<target_lookup_interface> target_lookup_type;

		std::string title;
		po::options_description local;
		data_type data;
		handler_type handler;
		target_lookup_type target_lookup;

		configuration() : data(data_type(new nscp_cli_data())) {}

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
		std::list<std::wstring> arguments;
		std::wstring command;
		std::wstring key;

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
		virtual int query(configuration::data_type data, ::Plugin::Common_Header* header, const std::string &request, std::string &reply) = 0;
		virtual int submit(configuration::data_type data, ::Plugin::Common_Header* header, const std::string &request, std::string &response) = 0;
		virtual int exec(configuration::data_type data, ::Plugin::Common_Header* header, const std::string &request, std::string &reply) = 0;
	};
	struct command_manager {
		typedef boost::unordered_map<std::wstring, command_container> command_type;
		command_type commands;

		std::wstring add_command(std::wstring name, std::wstring args);
		int exec_simple(configuration &config, const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::string &response);

		static std::wstring make_key(std::wstring key) {
			return boost::algorithm::to_lower_copy(key);
		}

	};

	struct command_line_parser {
		typedef configuration::data_type data_type;

		static void add_common_options(po::options_description &desc, data_type command_data);
		static void add_query_options(po::options_description &desc, data_type command_data);
		static void add_submit_options(po::options_description &desc, data_type command_data);
		static void add_exec_options(po::options_description &desc, data_type command_data);
		static std::wstring build_help(configuration &config);

		static int do_execute_command_as_exec(configuration &config, const std::wstring &command, std::list<std::wstring> &arguments, std::string &result);
		static int do_execute_command_as_query(configuration &config, const std::wstring &command, std::list<std::wstring> &arguments, const std::string &request, std::string &result);
		static int do_relay_submit(configuration &config, const std::string &request, std::string &response);

		static std::wstring parse_command(std::wstring command, std::wstring prefix) {
			std::wstring cmd = command;
			if (command.length() > prefix.length()) {
				if (command.substr(0,prefix.length()) == prefix)
					cmd = command.substr(prefix.length());
				else if (command.substr(command.length()-prefix.length()) == prefix)
					cmd = command.substr(0, command.length()-prefix.length());
				if (cmd[0] == L'_')
					cmd = cmd.substr(1);
				if (cmd[cmd.length()-1] == L'_')
					cmd = cmd.substr(0, cmd.length()-1);
			}
			return cmd;
		}
		static bool is_command(std::wstring command) {
			return (command == _T("help")) 
				|| (command == _T("query"))
				|| (command == _T("exec"))
				|| (command == _T("submit"))
				|| (command == _T("forward"));
		}

		static int do_query(configuration &config, const std::wstring &command, std::list<std::wstring> &arguments, std::string &result);
		static int do_forward(configuration &config, const std::string &request, std::string &result);
		static int do_exec(configuration &config, const std::wstring &command, std::list<std::wstring> &arguments, std::string &result);
		static int do_submit(configuration &config, const std::wstring &command, std::list<std::wstring> &arguments, std::string &result);

	private:
		static void modify_header(configuration &config, ::Plugin::Common_Header* header, nscapi::functions::destination_container &recipient);
	};
}
