#pragma once

#include <NSCAPI.h>
#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/tuple/tuple.hpp>

#include <nscapi/nscapi_protobuf_types.hpp>
#include <nscapi/nscapi_protobuf.hpp>

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
		std::string target_id;
		std::string command;
		std::string command_line;
		std::string message;
		std::string result;
		std::vector<std::string> arguments;
		int timeout;
		int retry;

		nscapi::protobuf::types::destination_container host_self;
		std::list<nscapi::protobuf::types::destination_container> recipients;


		void set_host(std::string s) {}
		void set_port(std::string s) {}
		void set_address(std::string s) {}


		void set_string_data(std::string key, std::string value) {
			// 			if (key == "host")
			// 				set_host(value);
			// 			else 
			// 				data[key] = value;
		}
		void set_int_data(std::string key, int value) {
			// 			if (key == "host")
			// 				set_host(value);
			// 			else 
			// 				data[key] = value;
		}
		void set_bool_data(std::string key, bool value) {
			// 			if (key == "host")
			// 				set_host(value);
			// 			else 
			// 				data[key] = value;
		}


		nscp_cli_data() : timeout(10), retry(2) {}
		std::string to_string() {
			std::stringstream ss;
			ss << "Timeout: " << timeout;
			ss << ", retry: " << retry;
			ss << ", command: " << command;
			ss << ", target: " << target_id;
			ss << ", self: {" << host_self.to_string() << "}";
			BOOST_FOREACH(const nscapi::protobuf::types::destination_container &r, recipients) {
				ss << ", recipient: {" << r.to_string() << "}";
			}
			ss << ", message: " << message;
			ss << ", result: " << result;
			int i=0;
			BOOST_FOREACH(std::string a, arguments) {
				ss << ", argument[" << i++ << "]: " << a;
			}
			return ss.str();
		}
	};

	struct clp_handler;




	struct hashmap_reader {
		typedef nscapi::targets::target_object object_type;
		typedef nscapi::targets::target_object target_object;

		static void init_default(target_object &target) {
			target.set_property_int("timeout", 30);
			target.set_property_string("encryption", "ase");
			target.set_property_int("payload length", 512);
		}

		static void add_custom_keys(sh::settings_registry &settings, boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool is_sample) {
			nscapi::settings_helper::path_extension root_path = settings.path(object.tpl.path);
			if (is_sample)
				root_path.set_sample();
			root_path.add_key()

				("timeout", sh::int_fun_key<int>(boost::bind(&object_type::set_property_int, &object, "timeout", _1), 30),
				"TIMEOUT", "Timeout when reading/writing packets to/from sockets.")

				("dh", sh::path_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "dh", _1), "${certificate-path}/nrpe_dh_512.pem"),
				"DH KEY", "", true)

				("certificate", sh::path_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "certificate", _1)),
				"SSL CERTIFICATE", "", false)

				("certificate key", sh::path_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "certificate key", _1)),
				"SSL CERTIFICATE", "", true)

				("certificate format", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "certificate format", _1), "PEM"),
				"CERTIFICATE FORMAT", "", true)

				("ca", sh::path_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "ca", _1)),
				"CA", "", true)

				("allowed ciphers", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "allowed ciphers", _1), "ADH"),
				"ALLOWED CIPHERS", "A better value is: ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH", false)

				("verify mode", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "verify mode", _1), "none"),
				"VERIFY MODE", "", false)

				("use ssl", sh::bool_fun_key<bool>(boost::bind(&object_type::set_property_bool, &object, "ssl", _1), false),
				"ENABLE SSL ENCRYPTION", "This option controls if SSL should be enabled.")

				("payload length",  sh::int_fun_key<int>(boost::bind(&object_type::set_property_int, &object, "payload length", _1), 512),
				"PAYLOAD LENGTH", "Length of payload to/from the NRPE agent. This is a hard specific value so you have to \"configure\" (read recompile) your NRPE agent to use the same value for it to work.", true)

				("encryption", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "encryption", _1), "aes"),
				"ENCRYPTION", std::string("Name of encryption algorithm to use.\nHas to be the same as your server i using or it wont work at all."
				"This is also independent of SSL and generally used instead of SSL.\nAvailable encryption algorithms are:\n") + nscp::encryption::helpers::get_crypto_string("\n"))

				("password", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "password", _1), ""),
				"PASSWORD", "The password to use. Again has to be the same as the server or it wont work at all.")

				("encoding", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "encoding", _1), ""),
				"ENCODING", "", true)

				("time offset", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "delay", _1), "0"),
				"TIME OFFSET", "Time offset.", true)
				;
		}

		static void post_process_target(target_object &target) {
			std::list<std::string> err;
			nscapi::targets::helpers::verify_file(target, "certificate", err);
			nscapi::targets::helpers::verify_file(target, "dh", err);
			nscapi::targets::helpers::verify_file(target, "certificate key", err);
			nscapi::targets::helpers::verify_file(target, "ca", err);
			BOOST_FOREACH(const std::string &e, err) {
				NSC_LOG_ERROR(e);
			}
		}
	};

	struct target_lookup_interface {
		virtual nscapi::protobuf::types::destination_container lookup_target(std::string &id) const = 0;
		virtual bool apply(nscapi::protobuf::types::destination_container &dst, const std::string key) = 0;
		virtual bool has_object(std::string alias) const = 0;
	};
	struct configuration : public boost::noncopyable {
		typedef boost::shared_ptr<nscp_cli_data> data_type;
		typedef boost::shared_ptr<clp_handler> handler_type;
		typedef boost::shared_ptr<target_lookup_interface> target_lookup_type;

		std::string title;
		std::string default_command;
		data_type data;
		boost::program_options::options_description local;
		handler_type handler;
		target_lookup_type target_lookup;

		configuration(std::string caption) : data(data_type(new nscp_cli_data())), local("Common options for " + caption) {}
		configuration(std::string caption, handler_type handler, target_lookup_type target_lookup) 
			: data(data_type(new nscp_cli_data())), local("Common options for " + caption) 
			, handler(handler)
			, target_lookup(target_lookup)
		{}

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

		void add_target(const std::string key, const std::string args);

		void clear() {
			commands.clear();
		}

		std::string add_command(std::string name, std::string args);
		int exec_simple(configuration &config, const std::string &target, const std::string &command, std::list<std::string> &arguments, std::string &response);

		// Wrappers based on source
		void parse_query(const std::string &prefix, const std::string &default_command, const std::string &cmd, client::configuration &config, const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response &response, const Plugin::QueryRequestMessage &request_message);
		bool parse_exec(const std::string &prefix, const std::string &default_command, const std::string &cmd, client::configuration &config, const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response &response, const Plugin::ExecuteRequestMessage &request_message);
		void parse_submit(const std::string &prefix, const std::string &default_command, const std::string &cmd, client::configuration &config, const Plugin::QueryResponseMessage::Response &request, Plugin::SubmitResponseMessage::Response &response, const Plugin::SubmitRequestMessage &request_message);

		// Wrappers based on source (for command line clients)
		void parse_query(client::configuration &config, const std::vector<std::string> &args, Plugin::QueryResponseMessage::Response &response);
//		bool parse_exec(const std::string &prefix, const std::string &default_command, const std::string &cmd, client::configuration &config, const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response &response, const Plugin::ExecuteRequestMessage &request_message);
//		void parse_submit(const std::string &prefix, const std::string &default_command, const std::string &cmd, client::configuration &config, const Plugin::QueryResponseMessage::Response &request, Plugin::SubmitResponseMessage::Response &response, const Plugin::SubmitRequestMessage &request_message);

		// Actual execution
		void do_query(client::configuration &config, const ::Plugin::Common::Header &header, Plugin::QueryResponseMessage::Response &response);
		void do_exec(client::configuration &config, const ::Plugin::Common::Header &header, Plugin::ExecuteResponseMessage::Response &response);
		void do_submit(client::configuration &config, const ::Plugin::Common::Header &header, Plugin::SubmitResponseMessage::Response &response);
		
		void forward_query(client::configuration &config, Plugin::QueryRequestMessage &request, Plugin::QueryResponseMessage &response);
		void forward_exec(client::configuration &config, const Plugin::ExecuteRequestMessage &request, Plugin::ExecuteResponseMessage::Response &response);
		void forward_submit(client::configuration &config, const Plugin::SubmitRequestMessage &request, Plugin::SubmitResponseMessage &response);
	};
}
