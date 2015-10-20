#pragma once

#include <NSCAPI.h>
#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/tuple/tuple.hpp>
#include <nscapi/nscapi_program_options.hpp>

#include <nscapi/nscapi_protobuf_types.hpp>
#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
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

	struct payload_builder {
		enum types {
			type_submit,
			type_query,
			type_exec,
			type_none
		};

		::Plugin::SubmitRequestMessage submit_message;
		::Plugin::QueryResponseMessage::Response *submit_payload;

		::Plugin::ExecuteRequestMessage exec_message;
		::Plugin::ExecuteRequestMessage::Request *exec_payload;

		::Plugin::QueryRequestMessage query_message;
		::Plugin::QueryRequestMessage::Request *query_payload;

		types type;
		std::string separator;
		payload_builder() : submit_payload(NULL), type(type_none), separator("|") {}

		void set_type(types type_) {
			type = type_;
		}

		void set_separator(const std::string &value) {
			separator = value;
		}

		void set_result(const std::string &value) {
			if (type == type_submit) {
				get_submit_payload()->set_result(nscapi::protobuf::functions::parse_nagios(value));
			} else if (type == type_exec) {
				throw cli_exception("result not supported for exec");
			} else {
				throw cli_exception("result not supported for query");
			}
		}
		void set_message(const std::string &value) {
			if (type == type_submit) {
				Plugin::QueryResponseMessage::Response::Line *l = get_submit_payload()->add_lines();
				l->set_message(value);
			} else if (type == type_exec) {
				throw cli_exception("message not supported for exec");
			} else {
				throw cli_exception("message not supported for query");
			}
		}
		void set_command(const std::string value) {
			if (type == type_submit) {
				get_submit_payload()->set_command(value);
			} else if (type == type_exec) {
				get_exec_payload()->set_command(value);
			} else {
				get_query_payload()->set_command(value);
			}
		}
		void set_arguments(const std::vector<std::string> &value) {
			if (type == type_submit) {
				throw cli_exception("arguments not supported for submit");
			} else if (type == type_exec) {
				BOOST_FOREACH(const std::string &a, value)
					get_exec_payload()->add_arguments(a);
			} else {
				BOOST_FOREACH(const std::string &a, value)
					get_query_payload()->add_arguments(a);
			}
		}
		void set_batch(const std::vector<std::string> &data) {
			if (type == type_submit) {
				BOOST_FOREACH(const std::string &e, data) {
					submit_payload = submit_message.add_payload();
					std::vector<std::string> line;
					boost::iter_split(line, e, boost::algorithm::first_finder(separator));
					if (line.size() >= 3)
						set_message(line[2]);
					if (line.size() >= 2)
						set_result(line[1]);
					if (line.size() >= 1)
						set_command(line[0]);
				}
			} else if (type == type_exec) {
				BOOST_FOREACH(const std::string &e, data) {
					exec_payload = exec_message.add_payload();
					std::list<std::string> line;
					boost::iter_split(line, e, boost::algorithm::first_finder(separator));
					if (line.size() >= 1) {
						set_command(line.front());
						line.pop_front();
					}
					BOOST_FOREACH(const std::string &a, line) {
						get_exec_payload()->add_arguments(a);
					}
				}
			} else {
				BOOST_FOREACH(const std::string &e, data) {
					query_payload = query_message.add_payload();
					std::list<std::string> line;
					boost::iter_split(line, e, boost::algorithm::first_finder(separator));
					if (line.size() >= 1) {
						set_command(line.front());
						line.pop_front();
					}
					BOOST_FOREACH(const std::string &a, line) {
						get_query_payload()->add_arguments(a);
					}
				}
			}
		}

	private:

		::Plugin::QueryResponseMessage::Response *get_submit_payload() {
			if (submit_payload == NULL)
				submit_payload = submit_message.add_payload();
			return submit_payload;
		}
		::Plugin::QueryRequestMessage::Request *get_query_payload() {
			if (query_payload == NULL)
				query_payload = query_message.add_payload();
			return query_payload;
		}
		::Plugin::ExecuteRequestMessage::Request *get_exec_payload() {
			if (exec_payload == NULL)
				exec_payload = exec_message.add_payload();
			return exec_payload;
		}
	};


	struct destination_container {
		typedef std::map<std::string, std::string> data_map;

		net::url address;
		int timeout;
		int retry;
		data_map data;

		destination_container() : timeout(10), retry(2) {}

		void apply(nscapi::settings_objects::object_instance obj) {
			BOOST_FOREACH(const nscapi::settings_objects::options_map::value_type &k, obj->get_options()) {
				set_string_data(k.first, k.second);
			}
		}

		void apply(const std::string &key, const ::Plugin::Common::Header &header) {
			BOOST_FOREACH(::Plugin::Common_Host host, header.hosts()) {
				if (host.id() == key) {
					apply_host(host);
				}
			}
		}
		void apply_host(const::Plugin::Common::Host &host) {
			if (host.has_address())
				set_string_data("address", host.address());
			BOOST_FOREACH(const ::Plugin::Common_KeyValue &kvp, host.metadata()) {
				set_string_data(kvp.key(), kvp.value());
			}
		}

		void set_host(std::string value) {
			address.host = value;
		}
		std::string get_host() const {
			return address.host;
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

		int get_int_data(std::string key, int def = 0) {
			return to_int(data[key], def);
		}
		bool get_bool_data(std::string key, bool def = false) {
			return to_bool(data[key], def);
		}
		std::string get_string_data(std::string key, std::string def = "") {
			data_map::iterator it = data.find(key);
			if (it == data.end())
				return def;
			return it->second;
		}
		bool has_data(std::string key) {
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
				timeout = to_int(value, timeout);
			else if (key == "retry")
				retry = to_int(value, retry);
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

	struct options_reader_interface : public  nscapi::settings_objects::object_factory_interface<nscapi::settings_objects::object_instance_interface> {
		virtual void process(boost::program_options::options_description &desc, destination_container &source, destination_container &destination) = 0;
		void add_ssl_options(boost::program_options::options_description & desc, client::destination_container & data);

	};
	typedef boost::shared_ptr<options_reader_interface> options_reader_type;

	struct handler_interface {
		virtual bool query(client::destination_container sender, client::destination_container target, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) = 0;
		virtual bool submit(client::destination_container sender, client::destination_container target, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage &response_message) = 0;
		virtual bool exec(client::destination_container sender, client::destination_container target, const Plugin::ExecuteRequestMessage &request_message, Plugin::ExecuteResponseMessage &response_message) = 0;
	};
	typedef boost::shared_ptr<handler_interface> handler_type;

	struct configuration : public boost::noncopyable {
		typedef boost::unordered_map<std::string, command_container> command_type;

		typedef nscapi::settings_objects::object_handler<nscapi::settings_objects::object_instance_interface, options_reader_interface> object_handler_type;
		handler_type handler;
		options_reader_type reader;
		object_handler_type targets;

		std::string title;
		std::string default_command;
		command_type commands;

		configuration(std::string caption, handler_type handler, options_reader_type reader) 
			: handler(handler)
			, reader(reader)
			, targets(reader) 
		{
		}


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
		std::string add_command(std::string name, std::string args);
		void clear() {
			targets.clear();
			commands.clear();
		}
		void finalize(boost::shared_ptr<nscapi::settings_proxy> settings);

		void do_query(const Plugin::QueryRequestMessage &request, Plugin::QueryResponseMessage &response);
		bool do_exec(const Plugin::ExecuteRequestMessage &request, Plugin::ExecuteResponseMessage &response);
		void do_submit(const Plugin::SubmitRequestMessage &request, Plugin::SubmitResponseMessage &response);

		typedef boost::function<boost::program_options::options_description(client::destination_container &source, client::destination_container &destination)> client_desc_fun;
		typedef boost::function<bool(client::destination_container &source, client::destination_container &destination)> client_pre_fun;
		client_desc_fun client_desc;
		client_pre_fun client_pre;


	private:
		boost::program_options::options_description create_descriptor(const std::string command, client::destination_container &source, client::destination_container &destination);
		void i_do_query(destination_container &s, destination_container &d, std::string command, const Plugin::QueryRequestMessage &request, Plugin::QueryResponseMessage &response, bool use_header);
		bool i_do_exec(destination_container &s, destination_container &d, std::string command, const Plugin::ExecuteRequestMessage &request, Plugin::ExecuteResponseMessage &response, bool use_header);
		void i_do_submit(destination_container &s, destination_container &d, std::string command, const Plugin::SubmitRequestMessage &request, Plugin::SubmitResponseMessage &response, bool use_header);
	};
}
