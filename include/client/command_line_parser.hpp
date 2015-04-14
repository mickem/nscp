#pragma once

#include <NSCAPI.h>
#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/tuple/tuple.hpp>

#include <nscapi/nscapi_protobuf_types.hpp>
#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_targets.hpp>

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



	struct NSCAPI_EXPORT destination_container {
		std::string id;
		net::url address;
		std::string comment;
		std::set<std::string> tags;

		int timeout;
		int retry;


		typedef std::map<std::string, std::string> data_map;
		data_map data;


		destination_container() : timeout(10), retry(2) {}

		void apply(nscapi::settings_objects::object_instance obj) {
			BOOST_FOREACH(const nscapi::settings_objects::options_map::value_type &k, obj->get_options()) {
				set_string_data(k.first, k.second);
			}
		}

		void apply(std::string key, const::Plugin::Common::Header &header) {
			for (int i = 0; i < header.hosts_size(); i++) {
				if (header.hosts(i).id() == key) {
					apply_host(header.hosts(i));
				}
			}
		}
		void apply_host(const::Plugin::Common::Host &host) {
			if (host.has_address())
				set_string_data("address", host.address());
			for (int i = 0; i < host.metadata_size(); i++) {
				set_string_data(host.metadata(i).key(), host.metadata(i).value());
			}
		}

		void set_host(std::string value) {
			address.host = value;
		}
		void set_address(std::string value) {
			address = net::parse(value);
		}
		void set_port(std::string value) {
			address.port = strEx::s::stox<unsigned int>(value);
		}
		std::string get_protocol() const {
			return address.protocol;
		}
		bool has_protocol() const {
			return !address.protocol.empty();
		}

		static bool to_bool(std::string value, bool def = false) {
			if (value.empty())
				return def;
			if (value == "true" || value == "1" || value == "True")
				return true;
			return false;
		}
		static int to_int(std::string value, int def = 0) {
			if (value.empty())
				return def;
			try {
				return boost::lexical_cast<int>(value);
			}
			catch (...) {
				return def;
			}
		}

		inline int get_int_data(std::string key, int def = 0) {
			return to_int(data[key], def);
		}
		inline bool get_bool_data(std::string key, bool def = false) {
			return to_bool(data[key], def);
		}
		inline std::string get_string_data(std::string key, std::string def = "") {
			data_map::iterator it = data.find(key);
			if (it == data.end())
				return def;
			return it->second;
		}
		inline bool has_data(std::string key) {
			return data.find(key) != data.end();
		}

		void set_string_data(std::string key, std::string value) {
			if (key == "host")
				set_host(value);
			else if (key == "address")
				set_address(value);
			else if (key == "port")
				address.port = to_int(value, address.port);
			else if (key == "timeout")
				timeout = to_int(value, address.port);
			else if (key == "retry")
				retry = to_int(value, address.port);
			else
				data[key] = value;
		}
		void set_int_data(std::string key, int value) {
			set_string_data(key, strEx::s::xtos(value));
		}
		void set_bool_data(std::string key, bool value) {
			set_string_data(key, value ? "true" : "false");
		}

		std::string to_string() const;
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


	struct nscp_clp_data {
		std::string target_id;
		std::string command;
		std::string command_line;
		std::string message;
		std::string result;
		std::vector<std::string> arguments;

		destination_container host_self;
		std::list<destination_container> targets;

		nscp_clp_data(nscapi::targets::target_object parent) {}
		std::string to_string() {
			std::stringstream ss;
			ss << "Command: " << command;
			ss << ", target: " << target_id;
			ss << ", self: {" << host_self.to_string() << "}";
			BOOST_FOREACH(const client::destination_container &r, targets) {
				ss << ", target: {" << r.to_string() << "}";
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

	struct options_reader {
		virtual void process(boost::program_options::options_description &desc, destination_container &source, destination_container &destination);
	};

	/*
	struct clp_item  {
		nscp_clp_data data;
		//const configuration &config;
		//clp_item(const configuration &config) : config(config) {}
	};
	*/

	struct clp_handler {
		virtual bool query(client::destination_container sender, client::destination_container target, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) = 0;
		virtual bool submit(client::destination_container sender, client::destination_container target, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage &response_message) = 0;
		virtual bool exec(client::destination_container sender, client::destination_container target, const Plugin::ExecuteRequestMessage &request_message, Plugin::ExecuteResponseMessage &response_message) = 0;
	};


	struct configuration : public boost::noncopyable {
		typedef boost::shared_ptr<nscp_clp_data> data_type;
		typedef boost::shared_ptr<clp_handler> handler_type;
		typedef boost::unordered_map<std::string, command_container> command_type;

		nscapi::settings_objects::object_handler targets;
		clp_handler *handler;

		std::string title;
		std::string default_command;
		boost::program_options::options_description local;
		boost::shared_ptr<client::options_reader> reader;
		command_type commands;

		configuration(std::string caption, boost::shared_ptr<client::options_reader> reader) : local("Common options for " + caption), reader(reader) {}

		std::string to_string() {
			std::stringstream ss;
			ss << "Title: " << title;
			ss << ", targets: " + targets.to_string();
			return ss.str();
		}

		void set_path(std::string path) {
			targets.set_path(path);
		}

		void add_target(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string value) {
			targets.add(proxy, key, value);
		}
		void clear() {
			targets.clear();
			commands.clear();
		}



		void do_query(const Plugin::QueryRequestMessage &request, Plugin::QueryResponseMessage &response);
		void forward_query(const Plugin::QueryRequestMessage &request, Plugin::QueryResponseMessage &response);

	private:
		void i_do_query(destination_container &s, destination_container &d, const std::string &command, const Plugin::QueryRequestMessage &request, Plugin::QueryResponseMessage &response, bool use_header);

	};

	struct command_manager {
		typedef boost::unordered_map<std::string, command_container> command_type;
		command_type commands;

		void clear() {
			commands.clear();
		}
	private:

		int exec_simple(configuration &config, const std::string &target, const std::string &command, std::list<std::string> &arguments, std::string &response);

	public:
		// Wrappers based on source
		void parse_query(const std::string &prefix, const std::string &default_command, const std::string &cmd, client::configuration &config, const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response &response, const Plugin::QueryRequestMessage &request_message);
		bool parse_exec(const std::string &prefix, const std::string &default_command, const std::string &cmd, client::configuration &config, const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response &response, const Plugin::ExecuteRequestMessage &request_message);
		void parse_submit(const std::string &prefix, const std::string &default_command, const std::string &cmd, client::configuration &config, const Plugin::QueryResponseMessage::Response &request, Plugin::SubmitResponseMessage::Response &response, const Plugin::SubmitRequestMessage &request_message);

		// Wrappers based on source (for command line clients)
		void parse_query(client::configuration &config, const std::vector<std::string> &args, Plugin::QueryResponseMessage::Response &response);
//		bool parse_exec(const std::string &prefix, const std::string &default_command, const std::string &cmd, client::configuration &config, const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response &response, const Plugin::ExecuteRequestMessage &request_message);
//		void parse_submit(const std::string &prefix, const std::string &default_command, const std::string &cmd, client::configuration &config, const Plugin::QueryResponseMessage::Response &request, Plugin::SubmitResponseMessage::Response &response, const Plugin::SubmitRequestMessage &request_message);

	public:

		std::string add_command(std::string name, std::string args);
		void add_target(const std::string key, const std::string args);

		// Actual execution
		void do_query(client::configuration &config, const ::Plugin::Common::Header &header, Plugin::QueryResponseMessage::Response &response);
		void do_exec(client::configuration &config, const ::Plugin::Common::Header &header, Plugin::ExecuteResponseMessage::Response &response);
		void do_submit(client::configuration &config, const ::Plugin::Common::Header &header, Plugin::SubmitResponseMessage::Response &response);
		
		void forward_query(client::configuration &config, Plugin::QueryRequestMessage &request, Plugin::QueryResponseMessage &response);
		void forward_exec(client::configuration &config, const Plugin::ExecuteRequestMessage &request, Plugin::ExecuteResponseMessage::Response &response);
		void forward_submit(client::configuration &config, const Plugin::SubmitRequestMessage &request, Plugin::SubmitResponseMessage &response);
	};
}
