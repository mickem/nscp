#include <client/command_line_parser.hpp>
#include <nscapi/functions.hpp>
#include <boost/bind.hpp>
#include <nscapi/nscapi_program_options.hpp>

namespace po = boost::program_options;

po::options_description add_common_options(client::configuration::data_type command_data) {
	po::options_description desc("Common options");
	desc.add_options()
		("host,H", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_host, &command_data->recipient, _1)), 
		"The host of the host running the server")
		("port,P", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_port, &command_data->recipient, _1)), 
		"The port of the host running the server")
		("address", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_address, &command_data->recipient, _1)), 
		"The address (host:port) of the host running the server")
		("timeout,T", po::value<int>(&command_data->timeout), "Number of seconds before connection times out (default=10)")
		("target,t", po::value<std::string>(&command_data->target_id), "Target to use (lookup connection info from config)")
		;
	return desc;
}
po::options_description add_query_options(client::configuration::data_type command_data) {
	po::options_description desc("Query options");
	desc.add_options()
		("command,c", po::value<std::string>(&command_data->command), "The name of the query that the remote daemon should run")
		("arguments,a", po::value<std::vector<std::string> >(&command_data->arguments), "list of arguments")
		("query-command", po::value<std::string>(&command_data->command), "The name of the query that the remote daemon should run")
		("query-arguments", po::value<std::vector<std::string> >(&command_data->arguments), "list of arguments")
		;
	return desc;
}
po::options_description add_submit_options(client::configuration::data_type command_data) {
	po::options_description desc("Submit options");
	desc.add_options()
		("command,c", po::value<std::string>(&command_data->command), "The name of the command that the remote daemon should run")
		("alias,a", po::value<std::string>(&command_data->command), "Same as command")
		("message,m", po::value<std::string>(&command_data->message), "Message")
		("result,r", po::value<std::string>(&command_data->result), "Result code either a number or OK, WARN, CRIT, UNKNOWN")
		;
	return desc;
}
po::options_description add_exec_options(client::configuration::data_type command_data) {
	po::options_description desc("Execute options");
	desc.add_options()
		("command,c", po::value<std::string>(&command_data->command), "The name of the command that the remote daemon should run")
		("arguments,a", po::value<std::vector<std::string> >(&command_data->arguments), "list of arguments")
		;
	return desc;
}
po::options_description create_descriptor(const std::string &command, const std::string &default_command, const client::configuration &config) {
	po::options_description desc = nscapi::program_options::create_desc(command);
	desc.add(add_common_options(config.data));
	if (command == "exec" || (command.empty() && default_command == "exec") ) {
		desc.add(add_exec_options(config.data));
	} else if (command == "query" || (command.empty() && default_command == "query") ) {
		desc.add(add_query_options(config.data));
	} else if (command == "submit" || (command.empty() && default_command == "submit") ) {
		desc.add(add_submit_options(config.data));
	}
	desc.add(config.local);
	return desc;
}

int parse_result(std::string key) {
	if (key == "UNKNOWN" || key == "unknown")
		return NSCAPI::returnUNKNOWN;
	if (key == "warn" || key == "WARN" || key == "WARNING" || key == "warning")
		return NSCAPI::returnWARN;
	if (key == "crit" || key == "CRIT" || key == "CRITICAL" || key == "critical")
		return NSCAPI::returnWARN;
	if (key == "OK" || key == "ok")
		return NSCAPI::returnUNKNOWN;
	try {
		return strEx::s::stox<int>(key);
	} catch (...) {
		return NSCAPI::returnUNKNOWN;
	}
}

void modify_header(client::configuration &config, ::Plugin::Common_Header* header, nscapi::protobuf::functions::destination_container &recipient) {
	nscapi::protobuf::functions::destination_container myself = config.data->host_self;
	if (!header->has_recipient_id()) {
		if (recipient.id.empty())
			recipient.id = "TODO missing id";
		nscapi::protobuf::functions::add_host(header, recipient);
		header->set_recipient_id(recipient.id);
	}
	nscapi::protobuf::functions::add_host(header, myself);
	if (!header->has_source_id())
		header->set_source_id(myself.id);
	header->set_sender_id(myself.id);
}

std::string parse_command(const std::string &command, const std::string &prefix) {
	if (command.length() > prefix.length()) {
		if (command.substr(0,prefix.length()+1) == prefix + "_")
			return command.substr(prefix.length()+1);
	}
	return command;
}



std::string client::command_manager::add_command(std::string name, std::string args) {
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





void client::command_manager::parse_query(const std::string &prefix, const std::string &default_command, const std::string &cmd, client::configuration &config, const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response &response, const Plugin::QueryRequestMessage &request_message) {
	boost::program_options::variables_map vm;
	std::string real_command;
	try {
		command_type::const_iterator cit = commands.find(cmd);
		boost::program_options::variables_map vm;
		if (cit == commands.end()) {
			real_command = parse_command(cmd, prefix);
			po::options_description desc = create_descriptor(real_command, default_command, config);
			if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, response)) 
				return;
		} else {
			std::vector<std::string> args;
			real_command = parse_command(cit->second.command, prefix);
			po::options_description desc = create_descriptor(real_command, default_command, config);
			BOOST_FOREACH(std::string argument, cit->second.arguments) {
				for (int i=0;i<request.arguments_size();i++) {
					strEx::replace(argument, "$ARG" + strEx::s::xtos(i+1) + "$", request.arguments(i));
				}
				args.push_back(argument);
			}
			if (!nscapi::program_options::process_arguments_from_vector(vm, desc, request.command(), args, response)) 
				return;
		}
	} catch (const std::exception &e) {
		return nscapi::protobuf::functions::set_response_bad(response, "Exception processing command line: " + utf8::utf8_from_native(e.what()));
	}

	if (!config.data->target_id.empty()) {
		if (!config.target_lookup)
			return nscapi::protobuf::functions::set_response_bad(response, "Target not found: " + config.data->target_id);
		config.data->recipient.import(config.target_lookup->lookup_target(config.data->target_id));
	}
	if (real_command == "query" || (real_command.empty() && default_command == "query")) {
		do_query(config, request_message.header(), response);
	} else if (real_command == "exec" || (real_command.empty() && default_command == "exec")) {
		return nscapi::protobuf::functions::set_response_bad(response, "Paradigm shift currently not supported");
	} else if (real_command == "submit" || (real_command.empty() && default_command == "submit")) {
		return nscapi::protobuf::functions::set_response_bad(response, "Paradigm shift currently not supported");
	} else {
		return nscapi::protobuf::functions::set_response_bad(response, "Invalid command: "  +real_command);
	}
}

void client::command_manager::parse_query(client::configuration &config, const std::vector<std::string> &args, Plugin::QueryResponseMessage::Response &response) {
	boost::program_options::variables_map vm;
	std::string real_command = "query";
	try {
			po::options_description desc = create_descriptor(real_command, real_command, config);
			if (!nscapi::program_options::process_arguments_from_vector(vm, desc, real_command, args, response)) 
				return;
	} catch (const std::exception &e) {
		return nscapi::protobuf::functions::set_response_bad(response, "Exception processing command line: " + utf8::utf8_from_native(e.what()));
	}

	if (real_command == "query") {
		const ::Plugin::Common::Header header;
		do_query(config, header, response);
	} else {
		return nscapi::protobuf::functions::set_response_bad(response, "Invalid command: "  +real_command);
	}
}

void client::command_manager::do_query(client::configuration &config, const ::Plugin::Common::Header &header, Plugin::QueryResponseMessage::Response &response) {
	Plugin::QueryRequestMessage local_request;
	Plugin::QueryResponseMessage local_response;
	local_request.mutable_header()->CopyFrom(header);
	modify_header(config, local_request.mutable_header(), config.data->recipient);
	// TODO: Copy data from real request here?
	nscapi::protobuf::functions::append_simple_query_request_payload(local_request.add_payload(), config.data->command, config.data->arguments);
	int ret = config.handler->query(config.data, local_request, local_response);
	if (ret == NSCAPI::hasFailed) {
		nscapi::protobuf::functions::set_response_bad(response, "Failed to process request");
		return;
	}
	if (local_response.payload_size() != 1) {
		nscapi::protobuf::functions::set_response_bad(response, "Response returned invalid number of results");
	}
	response.CopyFrom(local_response.payload(0));
}

void client::command_manager::forward_query(client::configuration &config, const Plugin::QueryRequestMessage &request, Plugin::QueryResponseMessage &response) {
	if (request.payload_size() == 1) {
		const  ::Plugin::QueryRequestMessage::Request &req_payload = request.payload(0);
		if (req_payload.arguments_size() > 0) {
			for (int i=0;i<req_payload.arguments_size();++i) {
				if (req_payload.arguments(i) == "--help" || req_payload.arguments(i) == "help") {
					nscapi::protobuf::functions::make_return_header(response.mutable_header(), request.header());
					::Plugin::QueryResponseMessage::Response *rsp_payload = response.add_payload();
					rsp_payload->set_command(req_payload.command());
					nscapi::protobuf::functions::set_response_bad(*rsp_payload, "Command will forward a query as-is to a remote node");
					return;
				} else if (req_payload.arguments(i) == "--help-csv" || req_payload.arguments(i) == "help-csv") {
					nscapi::protobuf::functions::make_return_header(response.mutable_header(), request.header());
					::Plugin::QueryResponseMessage::Response *rsp_payload = response.add_payload();
					rsp_payload->set_command(req_payload.command());
					nscapi::protobuf::functions::set_response_bad(*rsp_payload, "Command will forward a query as-is to a remote node");
					return;
				}
			}
		}
	}
	int ret = config.handler->query(config.data, request, response);
	if (ret == NSCAPI::hasFailed) {
		nscapi::protobuf::functions::make_return_header(response.mutable_header(), request.header());
		nscapi::protobuf::functions::set_response_bad(*response.add_payload(), "Failed to process request");
	}
}

bool client::command_manager::parse_exec(const std::string &prefix, const std::string &default_command, const std::string &cmd, client::configuration &config, const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response &response, const Plugin::ExecuteRequestMessage &request_message) {
	boost::program_options::variables_map vm;
	std::string real_command;
	try {

		//po::positional_options_description p;
		//p.add("arguments", -1);
		command_type::const_iterator cit = commands.find(cmd);
		boost::program_options::variables_map vm;
		if (cit == commands.end()) {
			real_command = parse_command(cmd, prefix);
			po::options_description desc = create_descriptor(real_command, default_command, config);
			if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, response)) 
				return true;
			if (!config.data->target_id.empty()) {
				if (!config.target_lookup) {
					nscapi::protobuf::functions::set_response_bad(response, "Target not found: " + config.data->target_id);
					return true;
				}
				config.data->recipient.apply(config.target_lookup->lookup_target(config.data->target_id));
				if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, response)) 
					return true;
			}
		} else {
			std::vector<std::string> args;
			po::options_description desc = create_descriptor(request.command(), default_command, config);
			real_command = parse_command(cit->second.command, prefix);
			BOOST_FOREACH(std::string argument, cit->second.arguments) {
				for (int i=0;i<request.arguments_size();i++) {
					strEx::replace(argument, "$ARG" + strEx::s::xtos(i+1) + "$", request.arguments(i));
				}
				args.push_back(argument);
			}
			if (!nscapi::program_options::process_arguments_from_vector(vm, desc, request.command(), args, response)) 
				return true;
			if (!config.data->target_id.empty()) {
				if (!config.target_lookup) {
					nscapi::protobuf::functions::set_response_bad(response, "Target not found: " + config.data->target_id);
					return true;
				}
				config.data->recipient.apply(config.target_lookup->lookup_target(config.data->target_id));
				if (!nscapi::program_options::process_arguments_from_vector(vm, desc, request.command(), args, response)) 
					return true;
			}
		}
	} catch (const std::exception &e) {
		nscapi::protobuf::functions::set_response_bad(response, "Exception processing command line: " + utf8::utf8_from_native(e.what()));
		return true;
	}

	if (real_command.empty())
		real_command = default_command;
	if (real_command == "query") {
		Plugin::QueryResponseMessage::Response local_response;
		do_query(config, request_message.header(), local_response);
		std::string s = nscapi::protobuf::functions::build_performance_data(local_response);
		if (!s.empty())
			s = local_response.message() + "|" + s;
		else
			s = local_response.message();
		response.set_message(s);
		response.set_result(local_response.result());
		return true;
	} else if (real_command == "exec") {
		do_exec(config, request_message.header(), response);
		return true;
	} else if (real_command == "submit") {
		Plugin::SubmitResponseMessage::Response local_response;
		do_submit(config, request_message.header(), local_response);
		if (local_response.status().status() == Plugin::Common_Status_StatusType_STATUS_OK || local_response.status().status() == Plugin::Common_Status_StatusType_STATUS_DELAYED) {
			if (local_response.status().message().empty())
				nscapi::protobuf::functions::set_response_good(response, "Submission successful");
			else
				nscapi::protobuf::functions::set_response_good(response, local_response.status().message());
		} else {
			nscapi::protobuf::functions::set_response_bad(response, "Submission failed: " + local_response.status().message());
		}
		return true;
	}
	return false;
}

void client::command_manager::do_exec(client::configuration &config, const ::Plugin::Common::Header &header, Plugin::ExecuteResponseMessage::Response &response) {
	Plugin::ExecuteRequestMessage local_request;
	Plugin::ExecuteResponseMessage local_response;
	local_request.mutable_header()->CopyFrom(header);
	modify_header(config, local_request.mutable_header(), config.data->recipient);
	// TODO: Copy data from real request here?
	nscapi::protobuf::functions::append_simple_exec_request_payload(local_request.add_payload(), config.data->command, config.data->arguments);
	int ret = config.handler->exec(config.data, local_request, local_response);
	if (ret == NSCAPI::hasFailed) {
		nscapi::protobuf::functions::set_response_bad(response, "Failed to process request");
		return;
	}
	if (local_response.payload_size() != 1) {
		nscapi::protobuf::functions::set_response_bad(response, "Response returned invalid number of results");
	}
	response.CopyFrom(local_response.payload(0));
}

void client::command_manager::forward_exec(client::configuration &config, const Plugin::ExecuteRequestMessage &request, Plugin::ExecuteResponseMessage::Response &response) {
	Plugin::ExecuteResponseMessage local_response;
	int ret = config.handler->exec(config.data, request, local_response);
	if (ret == NSCAPI::hasFailed) {
		nscapi::protobuf::functions::set_response_bad(response, "Failed to process request");
		return;
	}
	if (local_response.payload_size() != 1) {
		nscapi::protobuf::functions::set_response_bad(response, "Response returned invalid number of results");
	}
	response.CopyFrom(local_response.payload(0));
}


void client::command_manager::parse_submit(const std::string &prefix, const std::string &default_command, const std::string &cmd, client::configuration &config, const Plugin::QueryResponseMessage::Response &request, Plugin::SubmitResponseMessage::Response &response, const Plugin::SubmitRequestMessage &request_message) {
	boost::program_options::variables_map vm;
	std::string real_command;
	try {

		//po::positional_options_description p;
		//p.add("arguments", -1);
		command_type::const_iterator cit = commands.find(cmd);
		boost::program_options::variables_map vm;
		if (cit == commands.end()) {
			return nscapi::protobuf::functions::set_response_bad(response, "TODO ADD THIS?");
			/*
			real_command = parse_command(cmd, prefix);
			po::options_description desc = create_descriptor(real_command, default_command, config);
			if (!nscapi::program_options::process_arguments_from_request(vm, desc, request.command(), request, response)) 
				return;
				*/
		} else {
			std::vector<std::string> args;
			po::options_description desc = create_descriptor(request.command(), default_command, config);
			real_command = parse_command(cit->second.command, prefix);
			BOOST_FOREACH(std::string argument, cit->second.arguments) {
				for (int i=0;i<request.arguments_size();i++) {
					strEx::replace(argument, "$ARG" + strEx::s::xtos(i+1) + "$", request.arguments(i));
				}
				args.push_back(argument);
			}
			if (!nscapi::program_options::process_arguments_from_vector(vm, desc, request.command(), args, response)) 
				return;
		}
	} catch (const std::exception &e) {
		return nscapi::protobuf::functions::set_response_bad(response, "Exception processing command line: " + utf8::utf8_from_native(e.what()));
	}

	if (!config.data->target_id.empty()) {
		if (!config.target_lookup) 
			return nscapi::protobuf::functions::set_response_bad(response, "Target not found: " + config.data->target_id);
		config.data->recipient.import(config.target_lookup->lookup_target(config.data->target_id));
	}
	if (real_command == "query" || (real_command.empty() && default_command == "query")) {
		return nscapi::protobuf::functions::set_response_bad(response, "Paradigm shift currently not supported");
	} else if (real_command == "exec" || (real_command.empty() && default_command == "exec")) {
		return nscapi::protobuf::functions::set_response_bad(response, "Paradigm shift currently not supported");
	} else if (real_command == "submit" || (real_command.empty() && default_command == "submit")) {
		do_submit(config, request_message.header(), response);
	} else {
		return nscapi::protobuf::functions::set_response_bad(response, "Invalid command: "  +real_command);
	}
}

void client::command_manager::do_submit(client::configuration &config, const ::Plugin::Common::Header &header, Plugin::SubmitResponseMessage::Response &response) {
	Plugin::SubmitRequestMessage local_request;
	Plugin::SubmitResponseMessage local_response;
	local_request.mutable_header()->CopyFrom(header);
	modify_header(config, local_request.mutable_header(), config.data->recipient);
	// TODO: Copy data from real request here?
	nscapi::protobuf::functions::append_simple_submit_request_payload(local_request.add_payload(), config.data->command, parse_result(config.data->result), config.data->message);
	int ret = config.handler->submit(config.data, local_request, local_response);
	if (ret == NSCAPI::hasFailed) {
		nscapi::protobuf::functions::set_response_bad(response, "Failed to process request");
		return;
	}
	if (local_response.payload_size() != 1) {
		nscapi::protobuf::functions::set_response_bad(response, "Response returned invalid number of results");
	}
	response.CopyFrom(local_response.payload(0));
}

void client::command_manager::forward_submit(client::configuration &config, const Plugin::SubmitRequestMessage &request, Plugin::SubmitResponseMessage &response) {
	// Enrich header here with target information!
	if (config.handler->submit(config.data, request, response) != NSCAPI::isSuccess) {
		return nscapi::protobuf::functions::set_response_bad(*response.add_payload(), "Failed to process request");
	}
}