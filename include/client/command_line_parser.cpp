#include <boost/bind.hpp>

#include <utf8.hpp>

#include <client/command_line_parser.hpp>

#include <nscapi/functions.hpp>
#include <nscapi/nscapi_program_options.hpp>

namespace po = boost::program_options;



void client::options_reader_interface::add_ssl_options(boost::program_options::options_description & desc, client::destination_container & data) {
	desc.add_options()

		("certificate", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, data, "certificate", _1)), 
		"Length of payload (has to be same as on the server)")

		("dh", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, data, "dh", _1)), 
		"Length of payload (has to be same as on the server)")

		("certificate-key", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, data, "certificate key", _1)), 
		"Client certificate to use")

		("certificate-format", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, data, "certificate format", _1)), 
		"Client certificate format")

		("ca", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, data, "ca", _1)), 
		"Certificate authority")

		("verify", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, data, "verify mode", _1)), 
		"Client certificate format")

		("allowed-ciphers", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, data, "allowed ciphers", _1)), 
		"Client certificate format")

		("ssl,n", po::value<bool>()->zero_tokens()->default_value(false)->notifier(boost::bind(&client::destination_container::set_bool_data, data, "ssl", _1)), 
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
		//("target,t", po::value<std::string>(&obj.target_id), "Target to use (lookup connection info from config)")
		("retry", po::value<int>()->notifier(boost::bind(&client::destination_container::set_int_data, &destination, "retry", _1)), 
		"Number of times ti retry a failed connection attempt (default=2)")

		("source-host", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, source, "host", _1)), 
		"Source/sender host name (default is auto which means use the name of the actual host)")

		("sender-host", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, source, "host", _1)), 
		"Source/sender host name (default is auto which means use the name of the actual host)")

		;
	return desc;
}
po::options_description add_query_options(client::destination_container &source, client::destination_container &destination, client::payload_builder &builder) {
	po::options_description desc("Query options");
	desc.add_options()
		("command,c", po::value<std::string >()->notifier(boost::bind(&client::payload_builder::set_command, &builder, _1)), 
		"The name of the command that the remote daemon should run")
		("argument", po::value<std::vector<std::string>>()->notifier(boost::bind(&client::payload_builder::set_arguments, &builder, _1)), 
		"Set command line arguments")
		("separator", po::value<std::string>()->notifier(boost::bind(&client::payload_builder::set_separator, &builder, _1)), 
		"Separator to use for the batch command (default is |)")
		("batch", po::value<std::vector<std::string> >()->notifier(boost::bind(&client::payload_builder::set_batch, &builder, _1)), 
		"Add multiple records using the separator format is: command|argument|argument")
		;
	return desc;
}
po::options_description add_submit_options(client::destination_container &source, client::destination_container &destination, client::payload_builder &builder) {
	po::options_description desc("Submit options");
	desc.add_options()
		("command,c", po::value<std::string >()->notifier(boost::bind(&client::payload_builder::set_command, &builder, _1)), 
		"The name of the command that the remote daemon should run")
		("alias,a", po::value<std::string>()->notifier(boost::bind(&client::payload_builder::set_command, &builder, _1)), 
		"Same as command")
		("message,m", po::value<std::string>()->notifier(boost::bind(&client::payload_builder::set_message, &builder, _1)), 
		"Message")
		("result,r", po::value<std::string>()->notifier(boost::bind(&client::payload_builder::set_result, &builder, _1)), 
		"Result code either a number or OK, WARN, CRIT, UNKNOWN")
		("separator", po::value<std::string>()->notifier(boost::bind(&client::payload_builder::set_separator, &builder, _1)), 
		"Separator to use for the batch command (default is |)")
		("batch", po::value<std::vector<std::string> >()->notifier(boost::bind(&client::payload_builder::set_batch, &builder, _1)), 
		"Add multiple records using the separator format is: command|result|message")
		;
	return desc;
}
po::options_description add_exec_options(client::destination_container &source, client::destination_container &destination, client::payload_builder &builder) {
	po::options_description desc("Execute options");
	desc.add_options()
		("command,c", po::value<std::string >()->notifier(boost::bind(&client::payload_builder::set_command, &builder, _1)), 
		"The name of the command that the remote daemon should run")
		("argument", po::value<std::vector<std::string>>()->notifier(boost::bind(&client::payload_builder::set_arguments, &builder, _1)), 
		"Set command line arguments")
		("separator", po::value<std::string>()->notifier(boost::bind(&client::payload_builder::set_separator, &builder, _1)), 
		"Separator to use for the batch command (default is |)")
		("batch", po::value<std::vector<std::string> >()->notifier(boost::bind(&client::payload_builder::set_batch, &builder, _1)), 
		"Add multiple records using the separator format is: command|argument|argument")
		;
	return desc;
}

std::string client::configuration::add_command(std::string name, std::string args) {
	command_container data;
	bool first = true;
	BOOST_FOREACH(const std::string &s, strEx::s::parse_command(args)) {
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


void client::configuration::do_query(const Plugin::QueryRequestMessage &request, Plugin::QueryResponseMessage &response) {
	Plugin::QueryResponseMessage local_response;

	std::string target = "default";
	if (request.header().has_destination_id())
		target = request.header().destination_id();

	BOOST_FOREACH(const std::string t, strEx::s::splitEx(target, std::string(","))) {
		destination_container d;
		destination_container s;

		// If we have a target, apply it
		nscapi::settings_objects::object_handler::optional_object op = targets.find_object(t);
		if (op)
			d.apply(*op);

		// Next apply the header object
		d.apply(t, request.header());
		s.apply(request.header().sender_id(), request.header());

		if (d.has_data("command")) {
			std::string command = d.get_string_data("command");
			// If we have a header command treat the data as a batch
			i_do_query(s, d, command, request, response, true);

		} else {
			// Parse each objects command and execute them
			for (int i=0;i<request.payload_size();i++) {
				::Plugin::QueryRequestMessage local_request_message;
				const ::Plugin::QueryRequestMessage::Request &local_request = request.payload(i);
				local_request_message.mutable_header()->CopyFrom(request.header());
				local_request_message.add_payload()->CopyFrom(local_request);
				std::string command = local_request.command();
				::Plugin::QueryResponseMessage local_response_message;
				i_do_query(s, d, command, local_request_message, local_response_message, false);
				for (int j=0;j<local_response_message.payload_size();j++) {
					response.add_payload()->CopyFrom(local_response_message.payload(j));
				}
			}
		}
	}
}

po::options_description create_descriptor(const std::string command, client::destination_container &source, client::destination_container &destination) {
	po::options_description desc = nscapi::program_options::create_desc(command);
	desc.add(add_common_options(source, destination));
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
		if (command.substr(0,8) == "forward_") {
			if (!handler->query(s, d, request, response))
				nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
		} else {
			po::options_description desc = create_descriptor(command, s, d);
			payload_builder builder;
			if (command.substr(0,6) == "check_") {
				builder.set_type(payload_builder::type_query);
				desc.add(add_query_options(s, d, builder));
			} else if (command.substr(0,5) == "exec_") {
				builder.set_type(payload_builder::type_exec);
				desc.add(add_exec_options(s, d, builder));
			} else if (command.substr(0,7) == "submit_") {
				builder.set_type(payload_builder::type_submit);
				desc.add(add_submit_options(s, d, builder));
			} else {
				nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " not found");
				return;
			}
			reader->process(desc, s, d);
			if (custom_command) {
				// TODO: Parse argument vector here
			} else if (use_header) {
				// TODO: Parse header here
			} else {
				for (int i=0;i<request.payload_size();i++) {
					::Plugin::ExecuteResponseMessage::Response resp;
					// Apply any arguments from command line
					if (!nscapi::program_options::process_arguments_from_request(vm, desc, request.payload(i), resp)) {
						response.add_payload()->CopyFrom(resp);
						return;
					}
				}
			}

			if (command.substr(0,6) == "check_") {
				Plugin::QueryResponseMessage local_response;
				if (!handler->query(s, d, builder.query_message, local_response)) {
					nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
					return;
				}
				BOOST_FOREACH(const ::Plugin::QueryResponseMessage::Response d, local_response.payload()) {
					response.add_payload()->CopyFrom(d);
				}
			} else if (command.substr(0,5) == "exec_") {
				Plugin::ExecuteResponseMessage local_response;
				if (!handler->exec(s, d, builder.exec_message, local_response)) {
					nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
					return;
				}
				BOOST_FOREACH(const ::Plugin::ExecuteResponseMessage::Response d, local_response.payload()) {
					nscapi::protobuf::functions::copy_response(command, response.add_payload(), d);
				}
				// TODO: Convert reply to native reply
			} else if (command.substr(0,7) == "submit_") {
				Plugin::SubmitResponseMessage local_response;
				if (!handler->submit(s, d, builder.submit_message, local_response)) {
					nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
					return;
				}
				BOOST_FOREACH(const ::Plugin::SubmitResponseMessage::Response d, local_response.payload()) {
					nscapi::protobuf::functions::copy_response(command, response.add_payload(), d);
				}
			} else {
				nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " not found");
			}
		}
	} catch (const std::exception &e) {
		return nscapi::protobuf::functions::set_response_bad(*response.add_payload(), "Exception processing command line: " + utf8::utf8_from_native(e.what()));
	}
}

bool client::configuration::do_exec(const Plugin::ExecuteRequestMessage &request, Plugin::ExecuteResponseMessage &response) {
	Plugin::ExecuteResponseMessage local_response;

	std::string target = "default";
	if (request.header().has_destination_id())
		target = request.header().destination_id();

	BOOST_FOREACH(const std::string t, strEx::s::splitEx(target, std::string(","))) {
		destination_container d;
		destination_container s;

		// If we have a target, apply it
		nscapi::settings_objects::object_handler::optional_object op = targets.find_object(t);
		if (op)
			d.apply(*op);

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
			for (int i=0;i<request.payload_size();i++) {
				::Plugin::ExecuteRequestMessage local_request_message;
				const ::Plugin::ExecuteRequestMessage::Request &local_request = request.payload(i);
				local_request_message.mutable_header()->CopyFrom(request.header());
				local_request_message.add_payload()->CopyFrom(local_request);
				std::string command = local_request.command();
				::Plugin::ExecuteResponseMessage local_response_message;
				if (i_do_exec(s, d, command, local_request_message, local_response_message, false)) {
					found = true;
				}
				for (int j=0;j<local_response_message.payload_size();j++) {
					response.add_payload()->CopyFrom(local_response_message.payload(j));
				}
			}
		}
	}
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
		if (command.substr(0,8) == "forward_") {
			if (!handler->exec(s, d, request, response)) {
				nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
				return true;
			}
		} else {
			po::options_description desc = create_descriptor(command, s, d);
			payload_builder builder;
			if (command.substr(0,6) == "check_") {
				builder.set_type(payload_builder::type_query);
				desc.add(add_query_options(s, d, builder));
			} else if (command.substr(0,6) == "exec_") {
				builder.set_type(payload_builder::type_exec);
				desc.add(add_exec_options(s, d, builder));
			} else if (command.substr(0,6) == "submit_") {
				builder.set_type(payload_builder::type_submit);
				desc.add(add_submit_options(s, d, builder));
			} else {
				nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " not found");
				return false;
			}
			reader->process(desc, s, d);
			if (custom_command) {
				// TODO: Parse argument vector here
			} else if (use_header) {
				// TODO: Parse header here
			} else {
				for (int i=0;i<request.payload_size();i++) {
					::Plugin::ExecuteResponseMessage::Response resp;
					// Apply any arguments from command line
					if (!nscapi::program_options::process_arguments_from_request(vm, desc, request.payload(i), resp)) {
						response.add_payload()->CopyFrom(resp);
						return true;
					}
				}
			}

			if (command.substr(0,6) == "check_") {
				Plugin::QueryResponseMessage local_response;
				if (!handler->query(s, d, builder.query_message, local_response)) {
					nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
					return true;
				}
				BOOST_FOREACH(const ::Plugin::QueryResponseMessage::Response d, local_response.payload()) {
					nscapi::protobuf::functions::copy_response(command, response.add_payload(), d);
				}
			} else if (command.substr(0,6) == "exec_") {
				Plugin::ExecuteResponseMessage local_response;
				if (!handler->exec(s, d, builder.exec_message, local_response)) {
					nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
					return true;
				}
				BOOST_FOREACH(const ::Plugin::ExecuteResponseMessage::Response d, local_response.payload()) {
					response.add_payload()->CopyFrom(d);
				}
			} else if (command.substr(0,6) == "submit_") {
				Plugin::SubmitResponseMessage local_response;
				if (!handler->submit(s, d, builder.submit_message, local_response)) {
					nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
					return true;
				}
				BOOST_FOREACH(const ::Plugin::SubmitResponseMessage::Response d, local_response.payload()) {
					nscapi::protobuf::functions::copy_response(command, response.add_payload(), d);
				}
			}
		}
		return true;
	} catch (const std::exception &e) {
		nscapi::protobuf::functions::set_response_bad(*response.add_payload(), "Exception processing command line: " + utf8::utf8_from_native(e.what()));
		return true;
	}
}



void client::configuration::do_submit(const Plugin::SubmitRequestMessage &request, Plugin::SubmitResponseMessage &response) {
	Plugin::ExecuteResponseMessage local_response;

	std::string target = "default";
	if (request.header().has_destination_id())
		target = request.header().destination_id();

	BOOST_FOREACH(const std::string t, strEx::s::splitEx(target, std::string(","))) {
		destination_container d;
		destination_container s;

		// If we have a target, apply it
		nscapi::settings_objects::object_handler::optional_object op = targets.find_object(t);
		if (op)
			d.apply(*op);

		// Next apply the header object
		d.apply(t, request.header());
		s.apply(request.header().sender_id(), request.header());

		if (d.has_data("command")) {
			std::string command = d.get_string_data("command");
			// If we have a header command treat the data as a batch
			i_do_submit(s, d, command, request, response, true);

		} else {
			// Parse each objects command and execute them
			for (int i=0;i<request.payload_size();i++) {
				::Plugin::SubmitRequestMessage local_request_message;
				const ::Plugin::QueryResponseMessage::Response &local_request = request.payload(i);
				local_request_message.mutable_header()->CopyFrom(request.header());
				local_request_message.add_payload()->CopyFrom(local_request);
				std::string command = local_request.command();
				::Plugin::SubmitResponseMessage local_response_message;
				i_do_submit(s, d, command, local_request_message, local_response_message, false);
				for (int j=0;j<local_response_message.payload_size();j++) {
					response.add_payload()->CopyFrom(local_response_message.payload(j));
				}
			}
		}
	}
}



void client::configuration::i_do_submit(destination_container &s, destination_container &d, std::string command, const Plugin::SubmitRequestMessage &request, Plugin::SubmitResponseMessage &response, bool use_header) {
	try {
		boost::program_options::variables_map vm;
		bool custom_command = false;

		command_type::const_iterator cit = commands.find(command);
		if (cit != commands.end()) {
			command = cit->second.command;
			custom_command = true;
			// TODO: Build argument vector here!
		}
		if (command.substr(0,8) == "forward_") {
			if (!handler->submit(s, d, request, response))
				nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
		} else {
			nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " not found");
			return;
		}
	} catch (const std::exception &e) {
		return nscapi::protobuf::functions::set_response_bad(*response.add_payload(), "Exception processing command line: " + utf8::utf8_from_native(e.what()));
	}
}

void client::configuration::finalize(boost::shared_ptr<nscapi::settings_proxy> settings) {
	targets.add_samples(settings);
	targets.add_missing(settings, "default", "", true);
}
