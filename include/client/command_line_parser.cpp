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


#include <client/command_line_parser.hpp>

#include <nscapi/nscapi_protobuf_nagios.hpp>
#include <nscapi/nscapi_protobuf.hpp>

#include <utf8.hpp>

#include <boost/bind.hpp>
#include <boost/iterator.hpp>
#include <boost/algorithm/string.hpp>

#ifdef _WIN32
#pragma warning( disable : 4100)
#pragma warning( disable : 4101)
#pragma warning( disable : 4456)
#endif

namespace po = boost::program_options;


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
	payload_builder() : submit_payload(NULL), exec_payload(NULL), query_payload(NULL), type(type_none), separator("|") {}

	void set_type(types type_) {
		type = type_;
	}

	void set_separator(const std::string &value) {
		separator = value;
	}
	bool is_query() const {
		return type == type_query;
	}
	bool is_exec() const {
		return type == type_exec;
	}
	bool is_submit() const {
		return type == type_submit;
	}

	void set_result(const std::string &value);
	void set_message(const std::string &value) {
		if (is_submit()) {
			Plugin::QueryResponseMessage::Response::Line *l = get_submit_payload()->add_lines();
			l->set_message(value);
		} else if (is_exec()) {
			throw client::cli_exception("message not supported for exec");
		} else {
			throw client::cli_exception("message not supported for query");
		}
	}
	void set_command(const std::string value) {
		if (is_submit()) {
			get_submit_payload()->set_command(value);
		} else if (is_exec()) {
			get_exec_payload()->set_command(value);
		} else {
			get_query_payload()->set_command(value);
		}
	}
	void set_arguments(const std::vector<std::string> &value) {
		if (is_submit()) {
			throw client::cli_exception("arguments not supported for submit");
		} else if (is_exec()) {
			BOOST_FOREACH(const std::string &a, value)
				get_exec_payload()->add_arguments(a);
		} else {
			BOOST_FOREACH(const std::string &a, value)
				get_query_payload()->add_arguments(a);
		}
	}
	void set_batch(const std::vector<std::string> &data);

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

std::string client::destination_container::to_string() const {
	std::stringstream ss;
	ss << "address: " << address.to_string() << ", timeout: " << timeout << ", retry: " << retry << ", data: { ";
	BOOST_FOREACH(const data_map::value_type &t, data) {
		ss << t.first << ": " << t.second << ", ";
	}
	ss << "}";
	return ss.str();
}

void client::options_reader_interface::add_ssl_options(boost::program_options::options_description & desc, client::destination_container & data) {
	desc.add_options()

		("certificate", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, &data, "certificate", _1)),
			"Length of payload (has to be same as on the server)")

		("dh", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, &data, "dh", _1)),
			"Length of payload (has to be same as on the server)")

		("certificate-key", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, &data, "certificate key", _1)),
			"Client certificate to use")

		("certificate-format", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, &data, "certificate format", _1)),
			"Client certificate format")

		("ca", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, &data, "ca", _1)),
			"Certificate authority")

		("verify", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, &data, "verify mode", _1)),
			"Client certificate format")

		("allowed-ciphers", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, &data, "allowed ciphers", _1)),
			"Client certificate format")

		("ssl,n", po::value<bool>()->implicit_value(true)->notifier(boost::bind(&client::destination_container::set_bool_data, &data, "ssl", _1)),
			"Initial an ssl handshake with the server.")
		;
}

po::options_description add_common_options(client::destination_container &source, client::destination_container &destination) {
	po::options_description desc("Common options");
	desc.add_options()
		("host,H", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_host, &destination, _1)),
			"The host of the host running the server")
		("port,P", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_port, &destination, _1)),
			"The port of the host running the server")
		("address", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_address, &destination, _1)),
			"The address (host:port) of the host running the server")
		("timeout,T", po::value<int>()->notifier(boost::bind(&client::destination_container::set_int_data, &destination, "timeout", _1)),
			"Number of seconds before connection times out (default=10)")
		("target,t", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, &destination, "$target.id$", _1)),
			"Target to use (lookup connection info from config)")
		("retry", po::value<int>()->notifier(boost::bind(&client::destination_container::set_int_data, &destination, "retry", _1)),
			"Number of times ti retry a failed connection attempt (default=2)")
		("retries", po::value<int>()->notifier(boost::bind(&client::destination_container::set_int_data, &destination, "retry", _1)),
			"legacy version of retry")

		("source-host", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, &source, "host", _1)),
			"Source/sender host name (default is auto which means use the name of the actual host)")

		("sender-host", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, &source, "host", _1)),
			"Source/sender host name (default is auto which means use the name of the actual host)")

		;
	return desc;
}
po::options_description add_query_options(client::destination_container &source, client::destination_container &destination, payload_builder &builder) {
	po::options_description desc("Query options");
	desc.add_options()
		("command,c", po::value<std::string >()->notifier(boost::bind(&payload_builder::set_command, &builder, _1)),
			"The name of the command that the remote daemon should run")
		("argument,a", po::value<std::vector<std::string> >()->notifier(boost::bind(&payload_builder::set_arguments, &builder, _1)),
			"Set command line arguments")
		("separator", po::value<std::string>()->notifier(boost::bind(&payload_builder::set_separator, &builder, _1)),
			"Separator to use for the batch command (default is |)")
		("batch", po::value<std::vector<std::string> >()->notifier(boost::bind(&payload_builder::set_batch, &builder, _1)),
			"Add multiple records using the separator format is: command|argument|argument")
		;
	return desc;
}
po::options_description add_submit_options(client::destination_container &source, client::destination_container &destination, payload_builder &builder) {
	po::options_description desc("Submit options");
	desc.add_options()
		("command,c", po::value<std::string >()->notifier(boost::bind(&payload_builder::set_command, &builder, _1)),
			"The name of the command that the remote daemon should run")
		("alias,a", po::value<std::string>()->notifier(boost::bind(&payload_builder::set_command, &builder, _1)),
			"Same as command")
		("message,m", po::value<std::string>()->notifier(boost::bind(&payload_builder::set_message, &builder, _1)),
			"Message")
		("result,r", po::value<std::string>()->notifier(boost::bind(&payload_builder::set_result, &builder, _1)),
			"Result code either a number or OK, WARN, CRIT, UNKNOWN")
		("separator", po::value<std::string>()->notifier(boost::bind(&payload_builder::set_separator, &builder, _1)),
			"Separator to use for the batch command (default is |)")
		("batch", po::value<std::vector<std::string> >()->notifier(boost::bind(&payload_builder::set_batch, &builder, _1)),
			"Add multiple records using the separator format is: command|result|message")
		;
	return desc;
}
po::options_description add_exec_options(client::destination_container &source, client::destination_container &destination, payload_builder &builder) {
	po::options_description desc("Execute options");
	desc.add_options()
		("command,c", po::value<std::string >()->notifier(boost::bind(&payload_builder::set_command, &builder, _1)),
			"The name of the command that the remote daemon should run")
		("argument", po::value<std::vector<std::string> >()->notifier(boost::bind(&payload_builder::set_arguments, &builder, _1)),
			"Set command line arguments")
		("separator", po::value<std::string>()->notifier(boost::bind(&payload_builder::set_separator, &builder, _1)),
			"Separator to use for the batch command (default is |)")
		("batch", po::value<std::vector<std::string> >()->notifier(boost::bind(&payload_builder::set_batch, &builder, _1)),
			"Add multiple records using the separator format is: command|argument|argument")
		;
	return desc;
}

std::string client::configuration::add_command(std::string name, std::string args) {
	command_container data;
	bool first = true;
	BOOST_FOREACH(const std::string &s, str::utils::parse_command(args)) {
		if (first) {
			data.command = s;
			first = false;
		} else {
			data.arguments.push_back(s);
		}
	}

	std::string key = boost::algorithm::to_lower_copy(name);
	data.key = key;
	commands[data.key] = data;
	return key;
}

client::destination_container client::configuration::get_target(const std::string name) const {
	destination_container d;
	object_handler_type::object_instance op = targets.find_object(name);
	if (op)
		d.apply(op);
	else {
		op = targets.find_object("default");
		if (op)
			d.apply(op);
	}
	return d;
}

client::destination_container client::configuration::get_sender() const {
	destination_container s;
	s.set_address(default_sender);
	return s;
}

void client::configuration::do_query(const Plugin::QueryRequestMessage &request, Plugin::QueryResponseMessage &response) {
	Plugin::QueryResponseMessage local_response;

	std::string target = "default";
	if (request.header().has_recipient_id())
		target = request.header().recipient_id();
	else if (request.header().has_destination_id())
		target = request.header().destination_id();

	BOOST_FOREACH(const std::string t, str::utils::split_lst(target, std::string(","))) {
		destination_container d = get_target(t);
		destination_container s = get_sender();

		// Next apply the header object
		d.apply(t, request.header());
		s.apply(request.header().sender_id(), request.header());
		std::string command = request.header().command();

		if (!command.empty()) {
			// If we have a header command treat the data as a batch
			i_do_query(s, d, command, request, response, true);
		} else {
			// Parse each objects command and execute them
			for (int i = 0; i < request.payload_size(); i++) {
				::Plugin::QueryRequestMessage local_request_message;
				const ::Plugin::QueryRequestMessage::Request &local_request = request.payload(i);
				local_request_message.mutable_header()->CopyFrom(request.header());
				local_request_message.add_payload()->CopyFrom(local_request);
				std::string command = local_request.command();
				::Plugin::QueryResponseMessage local_response_message;
				i_do_query(s, d, command, local_request_message, local_response_message, false);
				for (int j = 0; j < local_response_message.payload_size(); j++) {
					response.add_payload()->CopyFrom(local_response_message.payload(j));
				}
			}
		}
	}
}

po::options_description client::configuration::create_descriptor(const std::string command, client::destination_container &source, client::destination_container &destination) {
	po::options_description desc = nscapi::program_options::create_desc(command);
	desc.add(add_common_options(source, destination));
	if (client_desc)
		desc.add(client_desc(source, destination));
	return desc;
}

void client::configuration::i_do_query(destination_container &s, destination_container &d, std::string command, const Plugin::QueryRequestMessage &request, Plugin::QueryResponseMessage &response, bool use_header) {
	try {
		boost::program_options::variables_map vm;
		bool custom_command = false;

		command_type::const_iterator cit = commands.find(command);
		if (cit != commands.end()) {
			command = cit->second.command;
			custom_command = true;
			// TODO: Build argument vector here!
		}
		if (command.substr(0, 8) == "forward_" || command.substr(command.size() - 8, 8) == "_forward") {
			BOOST_FOREACH(const Plugin::QueryRequestMessage::Request &p, request.payload()) {
				if (p.arguments_size() > 0) {
					BOOST_FOREACH(const std::string &a, p.arguments()) {
						if (a == "help-pb") {
							::Plugin::Registry::ParameterDetails details;
							::Plugin::Registry::ParameterDetail *td = details.add_parameter();
							td->set_name("*");
							td->set_short_description("This command will forward all arguments to remote system");
							nscapi::protobuf::functions::set_response_good_wdata(*response.add_payload(), details.SerializeAsString());
							return;
						}
					}
				}
			}
			if (!handler->query(s, d, request, response))
				nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
		} else {
			po::options_description desc = create_descriptor(command, s, d);
			payload_builder builder;
			std::string x = command.substr(command.size() - 6, 6);
			if (command.substr(0, 6) == "check_") {
				builder.set_type(payload_builder::type_query);
				desc.add(add_query_options(s, d, builder));
			} else if (command.substr(command.size()-6, 6) == "_query") {
				builder.set_type(payload_builder::type_query);
				desc.add(add_query_options(s, d, builder));
			} else if (command.substr(0, 5) == "exec_") {
				builder.set_type(payload_builder::type_exec);
				desc.add(add_exec_options(s, d, builder));
			} else if (command.substr(0, 7) == "submit_") {
				builder.set_type(payload_builder::type_submit);
				desc.add(add_submit_options(s, d, builder));
			} else {
				return nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " not found");
			}
			reader->process(desc, s, d);
			if (custom_command) {
				// TODO: Parse argument vector here
			} else if (use_header) {
				// TODO: Parse header here
			} else {
				for (int i = 0; i < request.payload_size(); i++) {
					::Plugin::QueryResponseMessage::Response resp;
					// Apply any arguments from command line
					po::positional_options_description p;
					p.add("argument", -1);

					if (!nscapi::program_options::process_arguments_from_request(vm, desc, request.payload(i), resp, p)) {
						response.add_payload()->CopyFrom(resp);
						return;
					}
				}
			}
			if (client_pre) {
				if (!client_pre(s, d))
					return;
			}

			if (builder.is_query()) {
				Plugin::QueryResponseMessage local_response;
				if (!handler->query(s, d, builder.query_message, local_response)) {
					return nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
				}
				BOOST_FOREACH(const ::Plugin::QueryResponseMessage::Response td, local_response.payload()) {
					response.add_payload()->CopyFrom(td);
				}
			} else if (builder.is_exec()) {
				Plugin::ExecuteResponseMessage local_response;
				if (!handler->exec(s, d, builder.exec_message, local_response)) {
					return nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
				}
				BOOST_FOREACH(const ::Plugin::ExecuteResponseMessage::Response td, local_response.payload()) {
					nscapi::protobuf::functions::copy_response(command, response.add_payload(), td);
				}
				// TODO: Convert reply to native reply
			} else if (builder.is_submit()) {
				Plugin::SubmitResponseMessage local_response;
				if (!handler->submit(s, d, builder.submit_message, local_response)) {
					return nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
				}
				BOOST_FOREACH(const ::Plugin::SubmitResponseMessage::Response td, local_response.payload()) {
					nscapi::protobuf::functions::copy_response(command, response.add_payload(), td);
				}
			} else {
				return nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " not found");
			}
		}
	} catch (const std::exception &e) {
		return nscapi::protobuf::functions::set_response_bad(*response.add_payload(), "Exception processing command line: " + utf8::utf8_from_native(e.what()));
	}
}

bool client::configuration::do_exec(const Plugin::ExecuteRequestMessage &request, Plugin::ExecuteResponseMessage &response, const std::string &t_default_command) {
	Plugin::ExecuteResponseMessage local_response;

	std::string target = "default";
	if (request.header().has_recipient_id())
		target = request.header().recipient_id();
	else if (request.header().has_destination_id())
		target = request.header().destination_id();

	BOOST_FOREACH(const std::string t, str::utils::split_lst(target, std::string(","))) {
		destination_container d = get_target(t);
		destination_container s = get_sender();

		// Next apply the header object
		d.apply(t, request.header());
		s.apply(request.header().sender_id(), request.header());

		if (d.has_data("command")) {
			std::string command = d.get_string_data("command");
			// If we have a header command treat the data as a batch
			return i_do_exec(s, d, command, request, response, true);
		} else {
			bool found = false;
			// Parse each objects command and execute them
			for (int i = 0; i < request.payload_size(); i++) {
				::Plugin::ExecuteRequestMessage local_request_message;
				const ::Plugin::ExecuteRequestMessage::Request &local_request = request.payload(i);
				local_request_message.mutable_header()->CopyFrom(request.header());
				local_request_message.add_payload()->CopyFrom(local_request);
				std::string command = local_request.command();
				if (command.empty())
					command = t_default_command;
				::Plugin::ExecuteResponseMessage local_response_message;
				if (i_do_exec(s, d, command, local_request_message, local_response_message, false)) {
					found = true;
				}
				for (int j = 0; j < local_response_message.payload_size(); j++) {
					response.add_payload()->CopyFrom(local_response_message.payload(j));
				}
			}
			if (!found) {
				nscapi::protobuf::functions::set_response_bad(*response.add_payload(), "failed");
			}
			return found;
		}
	}
	return false;
}

bool client::configuration::i_do_exec(destination_container &s, destination_container &d, std::string command, const Plugin::ExecuteRequestMessage &request, Plugin::ExecuteResponseMessage &response, bool use_header) {
	try {
		boost::program_options::variables_map vm;
		bool custom_command = false;

		command_type::const_iterator cit = commands.find(command);
		if (cit != commands.end()) {
			command = cit->second.command;
			custom_command = true;
			// TODO: Build argument vector here!
		}
		if (command.substr(0, 8) == "forward_") {
			if (!handler->exec(s, d, request, response)) {
				nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
				return true;
			}
		} else {
			po::options_description desc = create_descriptor(command, s, d);
			payload_builder builder;
			if (command.substr(0, 6) == "check_" || command.empty()) {
				builder.set_type(payload_builder::type_query);
				desc.add(add_query_options(s, d, builder));
			} else if (command.substr(0, 5) == "exec_") {
				builder.set_type(payload_builder::type_exec);
				desc.add(add_exec_options(s, d, builder));
			} else if (command.substr(0, 7) == "submit_" || command.substr(command.size() - 7, 7) == "_submit") {
				builder.set_type(payload_builder::type_submit);
				desc.add(add_submit_options(s, d, builder));
			} else {
				nscapi::protobuf::functions::set_response_bad(*response.add_payload(), "Module does not know of any command called: " + command);
				return false;
			}
			reader->process(desc, s, d);
			if (custom_command) {
				// TODO: Parse argument vector here
			} else if (use_header) {
				// TODO: Parse header here
			} else {
				for (int i = 0; i < request.payload_size(); i++) {
					::Plugin::ExecuteResponseMessage::Response resp;
					// Apply any arguments from command line
					// TODO: This is broken as it overwrite the source/targets
					if (!nscapi::program_options::process_arguments_from_request(vm, desc, request.payload(i), resp)) {
						response.add_payload()->CopyFrom(resp);
						return true;
					}
				}
			}
			if (d.has_data("$target.id$")) {
				std::string t = d.get_string_data("$target.id$");

				// If we have a target, apply it
				object_handler_type::object_instance op = targets.find_object(t);
				if (op) {
					d.apply(op);

					// Next apply the header object
					d.apply(t, request.header());
				}

				// If we have --target speciied apply the target and reapply the command line
				if (custom_command) {
					// TODO: Parse argument vector here
				} else if (use_header) {
					// TODO: Parse header here
				} else {
					for (int i = 0; i < request.payload_size(); i++) {
						::Plugin::ExecuteResponseMessage::Response resp;
						// Apply any arguments from command line
						// TODO: This is broken as it overwrite the source/targets
						if (!nscapi::program_options::process_arguments_from_request(vm, desc, request.payload(i), resp)) {
							response.add_payload()->CopyFrom(resp);
							return true;
						}
					}
				}
			}

			if (builder.type == payload_builder::type_query) {
				Plugin::QueryResponseMessage local_response;
				if (!handler->query(s, d, builder.query_message, local_response)) {
					nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
					return true;
				}
				BOOST_FOREACH(const ::Plugin::QueryResponseMessage::Response td, local_response.payload()) {
					nscapi::protobuf::functions::copy_response(command, response.add_payload(), td);
				}
			} else if (builder.type == payload_builder::type_exec) {
				Plugin::ExecuteResponseMessage local_response;
				if (!handler->exec(s, d, builder.exec_message, local_response)) {
					nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
					return true;
				}
				BOOST_FOREACH(const ::Plugin::ExecuteResponseMessage::Response td, local_response.payload()) {
					response.add_payload()->CopyFrom(td);
				}
			} else if (builder.type == payload_builder::type_submit) {
				Plugin::SubmitResponseMessage local_response;
				if (!handler->submit(s, d, builder.submit_message, local_response)) {
					nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
					return true;
				}
				BOOST_FOREACH(const ::Plugin::SubmitResponseMessage::Response td, local_response.payload()) {
					nscapi::protobuf::functions::copy_response(command, response.add_payload(), td);
				}
			}
		}
		return true;
	} catch (const std::exception &e) {
		nscapi::protobuf::functions::set_response_bad(*response.add_payload(), "Exception processing command line: " + utf8::utf8_from_native(e.what()));
		return true;
	}
}


void client::configuration::do_submit_item(const Plugin::SubmitRequestMessage &request, destination_container s, destination_container d, Plugin::SubmitResponseMessage &response) {
	// Parse each objects command and execute them
	BOOST_FOREACH(const ::Plugin::QueryResponseMessage::Response &local_request, request.payload()) {
		::Plugin::SubmitRequestMessage local_request_message;
		local_request_message.mutable_header()->CopyFrom(request.header());
		local_request_message.add_payload()->CopyFrom(local_request);
		::Plugin::SubmitResponseMessage local_response_message;
		i_do_submit(s, d, "forward_raw", local_request_message, local_response_message, false);
		BOOST_FOREACH(const ::Plugin::SubmitResponseMessage_Response &p, local_response_message.payload()) {
			response.add_payload()->CopyFrom(p);
		}
	}
}


void client::configuration::do_submit(const Plugin::SubmitRequestMessage &request, Plugin::SubmitResponseMessage &response) {
	Plugin::ExecuteResponseMessage local_response;

	std::string target = "default";
	if (request.header().has_recipient_id() && !request.header().recipient_id().empty())
		target = request.header().recipient_id();
	else if (request.header().has_destination_id() && !request.header().destination_id().empty())
		target = request.header().destination_id();

	BOOST_FOREACH(const std::string t, str::utils::split_lst(target, std::string(","))) {
		destination_container d = get_target(t);
		destination_container s = get_sender();

		// Next apply the header object
		d.apply(t, request.header());
		s.apply(request.header().sender_id(), request.header());

		if (d.has_data("command")) {
			std::string command = d.get_string_data("command");
			// If we have a header command treat the data as a batch
			i_do_submit(s, d, command, request, response, true);
		} else {
			do_submit_item(request, s, d, response);
		}
	}
}

void client::configuration::i_do_submit(destination_container &s, destination_container &d, std::string command, const Plugin::SubmitRequestMessage &request, Plugin::SubmitResponseMessage &response, bool use_header) {
	try {
		boost::program_options::variables_map vm;

		command_type::const_iterator cit = commands.find(command);
		if (cit != commands.end()) {
			command = cit->second.command;
			// TODO: Build argument vector here!
		}
		if (command.substr(0, 8) == "forward_") {
			if (!handler->submit(s, d, request, response))
				return nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
		} else {
			return nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " not found");
		}
	} catch (const std::exception &e) {
		return nscapi::protobuf::functions::set_response_bad(*response.add_payload(), "Exception processing command line: " + utf8::utf8_from_native(e.what()));
	}
}

void client::configuration::do_metrics(const Plugin::MetricsMessage &request) {
	std::string target = "default";
	if (request.header().has_recipient_id())
		target = request.header().recipient_id();
	else if (request.header().has_destination_id())
		target = request.header().destination_id();

	BOOST_FOREACH(const std::string t, str::utils::split_lst(target, std::string(","))) {
		destination_container d = get_target(t);
		destination_container s = get_sender();

		// Next apply the header object
		d.apply(t, request.header());
		s.apply(request.header().sender_id(), request.header());

		handler->metrics(s, d, request);
	}
}

void client::configuration::finalize(boost::shared_ptr<nscapi::settings_proxy> settings) {
	targets.add_samples(settings);
	targets.add_missing(settings, "default", "");
}
void payload_builder::set_result(const std::string &value) {
	if (is_submit()) {
		get_submit_payload()->set_result(nscapi::protobuf::functions::parse_nagios(value));
	} else if (is_exec()) {
		throw client::cli_exception("result not supported for exec");
	} else {
		throw client::cli_exception("result not supported for query");
	}
}

void payload_builder::set_batch(const std::vector<std::string> &data) {
	if (is_submit()) {
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
