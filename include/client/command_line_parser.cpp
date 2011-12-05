#include <client/command_line_parser.hpp>
#include <nscapi/functions.hpp>

namespace po = boost::program_options;

void client::command_line_parser::add_common_options(po::options_description &desc, data_type command_data) {
	desc.add_options()
		("host,H", po::value<std::string>(&command_data->recipient.address), "The address of the host running the server")
		("timeout,T", po::value<int>(&command_data->timeout), "Number of seconds before connection times out (default=10)")
		("target,t", po::wvalue<std::wstring>(&command_data->target_id), "Target to use (lookup connection info from config)")
		("query,q", po::bool_switch(&command_data->query), "Force query mode (only useful when this is not already obvious)")
		("submit,s", po::bool_switch(&command_data->submit), "Force submit mode (only useful when this is not already obvious)")
		("exec,e", po::bool_switch(&command_data->exec), "Force exec mode (only useful when this is not already obvious)")
		;
}
void client::command_line_parser::add_query_options(po::options_description &desc, data_type command_data) {
	desc.add_options()
		("command,c", po::wvalue<std::wstring>(&command_data->command), "The name of the query that the remote daemon should run")
		("arguments,a", po::wvalue<std::vector<std::wstring> >(&command_data->arguments), "list of arguments")
		("query-command", po::wvalue<std::wstring>(&command_data->command), "The name of the query that the remote daemon should run")
		("query-arguments", po::wvalue<std::vector<std::wstring> >(&command_data->arguments), "list of arguments")
		;
}
void client::command_line_parser::add_submit_options(po::options_description &desc, data_type command_data) {
	desc.add_options()
		("command,c", po::wvalue<std::wstring>(&command_data->command), "The name of the command that the remote daemon should run")
		("message,m", po::wvalue<std::wstring>(&command_data->message), "Message")
		("result,r", po::value<unsigned int>(&command_data->result), "Result code")
		;
}
void client::command_line_parser::add_exec_options(po::options_description &desc, data_type command_data) {
	desc.add_options()
		("command,c", po::wvalue<std::wstring>(&command_data->command), "The name of the command that the remote daemon should run")
		("arguments,a", po::wvalue<std::vector<std::wstring> >(&command_data->arguments), "list of arguments")
		;
}

std::wstring client::command_line_parser::build_help(configuration &config) {
	po::options_description common("Common options");
	add_common_options(common, config.data);
	po::options_description query("Command: query");
	add_query_options(query, config.data);
	po::options_description submit("Command: submit");
	add_submit_options(submit, config.data);
	po::options_description exec("Command: exec");
	add_exec_options(exec, config.data);
	po::options_description desc("Options for the following commands: (query, submit, exec)");
	desc.add(common).add(query).add(submit).add(config.local);
	std::stringstream ss;
	ss << desc;
	return utf8::cvt<std::wstring>(ss.str());
}


int client::command_line_parser::do_execute_command_as_exec(configuration &config, const std::wstring &command, std::list<std::wstring> &arguments, std::string &result) {
	if (!config.validate())
		throw cli_exception("Invalid data: " + config.to_string());
	if (command == _T("help")) {
		return nscapi::functions::create_simple_exec_response_unknown(command, build_help(config), result);
	} else if (command == _T("query")) {
		std::wstring msg, perf;
		int ret = do_query(config, command, arguments, result);
		nscapi::functions::make_exec_from_query(result);
		return ret;
	} else if (command == _T("exec")) {
		return do_exec(config, command, arguments, result);
	} else if (command == _T("submit")) {
		int ret = do_submit(config, command, arguments, result);
		nscapi::functions::make_exec_from_submit(result);
		return ret;
	}
	return NSCAPI::returnIgnored;
}

int client::command_line_parser::do_execute_command_as_query(configuration &config, const std::wstring &command, std::list<std::wstring> &arguments, std::string &result) {
	if (!config.validate())
		throw cli_exception("Invalid data: " + config.to_string());
	if (command == _T("help")) {
		return nscapi::functions::create_simple_query_response_unknown(command, build_help(config), _T(""), result);
	} else if (command == _T("query")) {
		return do_query(config, command, arguments, result);
	} else if (command == _T("exec")) {
		int ret = do_exec(config, command, arguments, result);
		nscapi::functions::make_query_from_exec(result);
		return ret;
	} else if (command == _T("submit")) {
		int ret = do_submit(config, command, arguments, result);
		nscapi::functions::make_query_from_submit(result);
		return ret;
	}
	return NSCAPI::returnIgnored;
}

std::wstring client::command_manager::add_command(std::wstring name, std::wstring args) {
	command_container data;
	boost::escaped_list_separator<wchar_t> sep(L'\\', L' ', L'\"');
	typedef boost::tokenizer<boost::escaped_list_separator<wchar_t>,std::wstring::const_iterator, std::wstring > tokenizer_t;
	tokenizer_t tok(args, sep);
	bool first = true;
	BOOST_FOREACH(const std::wstring &s, tok) {
		if (first) {
			data.command = s;
			first = false;
		} else {
			data.arguments.push_back(s);
		}
	}

	data.key = make_key(name);
	commands[data.key] = data;
	return data.key;
}

int client::command_manager::exec_simple(configuration &config, const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::string &response) {
	command_type::const_iterator cit = commands.find(command);
	if (cit == commands.end())
		return NSCAPI::returnIgnored;
	const command_container ci = (*cit).second;

	std::list<std::wstring> rendered_arguments;
	BOOST_FOREACH(std::wstring a, ci.arguments) {
		int i=1;
		BOOST_FOREACH(const std::wstring &param, arguments) {
			strEx::replace(a, _T("$ARG") + strEx::itos(i++)+_T("$"), param);
		}
		rendered_arguments.push_back(a);
	}
	// TODO: Add support for target here!
	return client::command_line_parser::do_execute_command_as_exec(config, ci.command, rendered_arguments, response);
}
int client::command_line_parser::do_query(configuration &config, const std::wstring &command, std::list<std::wstring> &arguments, std::string &response) {
	boost::program_options::variables_map vm;

	po::options_description common("Common options");
	add_common_options(common, config.data);
	po::options_description query("Query NSCP options");
	add_query_options(query, config.data);
	po::options_description desc("Allowed options");
	desc.add(common).add(query).add(config.local);

	std::vector<std::wstring> vargs(arguments.begin(), arguments.end());
	po::positional_options_description p;
	p.add("arguments", -1);
	po::wparsed_options parsed = po::basic_command_line_parser<wchar_t>(vargs).options(desc).positional(p).run();
	po::store(parsed, vm);
	po::notify(vm);
	if (!config.data->target_id.empty()) {
		if (!config.target_lookup)
			throw cli_exception("No target interface given when looking for targets");
		config.data->recipient.import(config.target_lookup->lookup_target(config.data->target_id));
	}

	Plugin::QueryRequestMessage message;
	nscapi::functions::create_simple_header(message.mutable_header());
	modify_header(config, message.mutable_header(), config.data->recipient);
	nscapi::functions::append_simple_query_request_payload(message.add_payload(), config.data->command, config.data->arguments);
	std::string result;
	return config.handler->query(config.data, message.mutable_header(), message.SerializeAsString(), result);
}

int client::command_line_parser::do_exec(configuration &config, const std::wstring &command, std::list<std::wstring> &arguments, std::string &result) {
	boost::program_options::variables_map vm;

	po::options_description common("Common options");
	add_common_options(common, config.data);
	po::options_description query("Query NSCP options");
	add_exec_options(query, config.data);
	po::options_description desc("Allowed options");
	desc.add(common).add(query).add(config.local);

	std::vector<std::wstring> vargs(arguments.begin(), arguments.end());
	po::positional_options_description p;
	p.add("arguments", -1);
	po::wparsed_options parsed = po::basic_command_line_parser<wchar_t>(vargs).options(desc).positional(p).run();
	po::store(parsed, vm);
	po::notify(vm);
	if (!config.data->target_id.empty()) {
		if (!config.target_lookup)
			throw cli_exception("No target interface given when looking for targets");
		config.data->recipient.import(config.target_lookup->lookup_target(config.data->target_id));
	}

	Plugin::ExecuteRequestMessage message;
	nscapi::functions::create_simple_header(message.mutable_header());
	modify_header(config, message.mutable_header(), config.data->recipient);
	std::string response;
	nscapi::functions::append_simple_exec_request_payload(message.add_payload(), config.data->command, config.data->arguments);
	return config.handler->exec(config.data, message.mutable_header(), message.SerializeAsString(), response);
}

int client::command_line_parser::do_submit(configuration &config, const std::wstring &command, std::list<std::wstring> &arguments, std::string &result) {
	boost::program_options::variables_map vm;
	po::options_description common("Common options");
	add_common_options(common, config.data);
	po::options_description submit("Submit options");
	add_submit_options(submit, config.data);
	po::options_description desc("Allowed options");
	desc.add(common).add(submit).add(config.local);

	std::vector<std::wstring> vargs(arguments.begin(), arguments.end());
	po::wparsed_options parsed = po::basic_command_line_parser<wchar_t>(vargs).options(desc).run();
	po::store(parsed, vm);
	po::notify(vm);
	if (!config.data->target_id.empty()) {
		if (!config.target_lookup)
			throw cli_exception("No target interface given when looking for targets");
		config.data->recipient.import(config.target_lookup->lookup_target(config.data->target_id));
	}

	Plugin::SubmitRequestMessage message;
	nscapi::functions::create_simple_header(message.mutable_header());
	modify_header(config, message.mutable_header(), config.data->recipient);
	message.set_channel("CLI");
	nscapi::functions::append_simple_submit_request_payload(message.add_payload(), config.data->command, config.data->result, config.data->message);

	return config.handler->submit(config.data, message.mutable_header(), message.SerializeAsString(), result);
}
void client::command_line_parser::modify_header(configuration &config, ::Plugin::Common_Header* header, nscapi::functions::destination_container &recipient) {
	nscapi::functions::destination_container myself = config.data->host_self;
	if (!header->has_recipient_id()) {
		if (recipient.id.empty())
			recipient.id = "TODO missing id";
		nscapi::functions::add_host(header, recipient);
		header->set_recipient_id(recipient.id);
	}
	nscapi::functions::add_host(header, myself);
	if (!header->has_source_id())
		header->set_source_id(myself.id);
	header->set_sender_id(myself.id);
}

int client::command_line_parser::do_relay_submit(configuration &config, const std::string &request, std::string &response) {
	Plugin::SubmitRequestMessage message;
	message.ParseFromString(request);
	modify_header(config, message.mutable_header(), config.data->recipient);
	return config.handler->submit(config.data, message.mutable_header(), message.SerializeAsString(), response);
}

