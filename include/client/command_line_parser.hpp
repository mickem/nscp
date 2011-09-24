#pragma once

#include <NSCAPI.h>
#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>

#include <nscapi/functions.hpp>

namespace client {

	namespace po = boost::program_options;

	struct nscp_cli_data {
		std::wstring target_id;
		std::wstring command;
		std::wstring command_line;
		std::wstring message;
		unsigned int result;
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
		nscapi::functions::destination_container host_default_recipient;

		configuration() : data(data_type(new nscp_cli_data())) {}

	};
	struct configuration_instance {
		typedef boost::shared_ptr<nscp_cli_data> data_type;
		typedef boost::shared_ptr<clp_handler> handler_type;
		data_type data;
		handler_type handler;

		configuration_instance() : data(data_type(new nscp_cli_data())) {}
		configuration_instance(configuration &config) : data(config.data), handler(config.handler) {}
		configuration_instance(const configuration_instance &other) : data(other.data), handler(other.handler) {}
		const configuration_instance& operator=(const configuration_instance &other) {
			data = other.data;
			handler = other.handler;
			return *this;
		}
	};

	struct clp_handler {
		virtual int query(configuration::data_type data, std::string request, std::string &reply) = 0;
		virtual std::list<std::string> submit(configuration::data_type data, ::Plugin::Common_Header* header, const std::string &request, std::string &response) = 0;
		virtual int exec(configuration::data_type data, std::string request, std::string &reply) = 0;
	};
	struct command_manager {
		typedef boost::unordered_map<std::wstring, configuration_instance> command_type;
		command_type commands;

		std::wstring add_command(configuration &config, std::wstring name, std::wstring args);
		int exec_simple(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf);

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

		static int commandLineExec(configuration &config, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result);
		static bool relay_submit(configuration &config, const std::string &request, std::string &response);

		static std::list<std::string> simple_submit(configuration &config, const std::wstring &command, std::list<std::wstring> &arguments);

		static int query(configuration &config, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &msg, std::wstring &perf);
		//static std::list<std::string> submit(configuration &config, const std::wstring &command, std::list<std::wstring> &arguments);
		static int exec(configuration &config, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result);

	private:
		static void modify_header(configuration &config, ::Plugin::Common_Header* header, nscapi::functions::destination_container &recipient);
	};
}
