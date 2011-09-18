#include <client/command_line_parser.hpp>
#include <nscapi/functions.hpp>

void client::command_line_parser::add_common_options(po::options_description &desc, data_type command_data) {
	desc.add_options()
		("host,H", po::wvalue<std::wstring>(&command_data->host), "The address of the host running the server")
		("timeout,T", po::value<int>(&command_data->timeout), "Number of seconds before connection times out (default=10)")
		("target,t", po::wvalue<std::wstring>(&command_data->target), "Target to use (lookup connection info from config)")
		("query,q", po::bool_switch(&command_data->query), "Force query mode (only useful when this is not obvious)")
		("submit,s", po::bool_switch(&command_data->submit), "Force submit mode (only useful when this is not obvious)")
		("exec,e", po::bool_switch(&command_data->exec), "Force exec mode (only useful when this is not obvious)")
		;
}
void client::command_line_parser::add_query_options(po::options_description &desc, data_type command_data) {
	desc.add_options()
		("command,c", po::wvalue<std::wstring>(&command_data->command), "The name of the query that the remote daemon should run")
		("arguments,a", po::wvalue<std::vector<std::wstring> >(&command_data->arguments), "list of arguments")
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
	po::options_description query("Query options");
	add_query_options(query, config.data);
	po::options_description submit("Submit options");
	add_submit_options(submit, config.data);
	po::options_description exec("Execute options");
	add_exec_options(exec, config.data);
	po::options_description desc("Allowed options");
	desc.add(common).add(query).add(submit).add(config.local);

	std::stringstream ss;
	ss << "Command line syntax:" << std::endl;
	ss << desc;
	return utf8::cvt<std::wstring>(ss.str());
}


int client::command_line_parser::commandLineExec(configuration &config, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result) {
	if (command == _T("help")) {
		result = build_help(config);
		return NSCAPI::returnUNKNOWN;
	} else if (command == _T("query")) {
		std::wstring msg, perf;
		int ret = query(config, command, arguments, msg, perf);
		result = msg + _T("|") + perf + _T("\n");
		return ret;
	} else if (command == _T("exec")) {
		return exec(config, command, arguments, result);
	} else if (command == _T("submit")) {
		std::list<std::string> errors = submit(config, command, arguments);
		bool has_errors = false;
		BOOST_FOREACH(std::string p, errors) {
			has_errors = true;
			result += utf8::cvt<std::wstring>(p) + _T("\n");
		}
		return has_errors?NSCAPI::returnCRIT:NSCAPI::returnOK;
	}
	return NSCAPI::returnIgnored;
}

std::wstring client::command_manager::add_command(configuration &config, std::wstring name, std::wstring args) {
	boost::program_options::variables_map vm;

	po::options_description common("Common options");
	client::command_line_parser::add_common_options(common, config.data);
	po::options_description query("Query options");
	client::command_line_parser::add_query_options(query, config.data);
	po::options_description submit("Submit options");
	client::command_line_parser::add_submit_options(submit, config.data);
	po::options_description exec("Execute options");
	client::command_line_parser::add_exec_options(exec, config.data);
	po::options_description desc("Allowed options");
	desc.add(common).add(query).add(submit).add(config.local);

	po::positional_options_description p;
	p.add("arguments", -1);

	std::vector<std::wstring> list;
	boost::escaped_list_separator<wchar_t> sep(L'\\', L' ', L'\"');
	typedef boost::tokenizer<boost::escaped_list_separator<wchar_t>,std::wstring::const_iterator, std::wstring > tokenizer_t;
	tokenizer_t tok(args, sep);
	for(tokenizer_t::iterator beg=tok.begin(); beg!=tok.end();++beg){
		list.push_back(*beg);
	}

	po::wparsed_options parsed = po::basic_command_line_parser<wchar_t>(list).options(desc).positional(p).run();
	po::store(parsed, vm);
	po::notify(vm);

	std::wstring key = make_key(name);
	commands[key] = configuration_instance(config);
	return key;
}

int client::command_manager::exec_simple(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf) {
	command_type::const_iterator cit = commands.find(command);
	if (cit == commands.end())
		return NSCAPI::returnIgnored;
	configuration_instance ci = (*cit).second;

	// @todo: add support for extending with arguments here!
	if (ci.data->submit) {
		std::string buffer;
		nscapi::functions::create_simple_query_response(ci.data->command, ci.data->result, ci.data->message, _T(""), buffer);
		std::list<std::string> errors = ci.handler->submit(ci.data, buffer);
		BOOST_FOREACH(std::string l, errors) {
			message += to_wstring(l) + _T("\n");
		}
		return errors.empty()?NSCAPI::returnOK:NSCAPI::returnCRIT;
	} else if (ci.data->query) {
		std::string buffer, reply;
		nscapi::functions::create_simple_query_request(ci.data->command, ci.data->arguments, buffer);
		int ret = ci.handler->exec(ci.data, buffer, reply);
		nscapi::functions::parse_simple_query_response(reply, message, perf);
		return ret;
	} else if (ci.data->exec) {
		std::string buffer, reply;
		nscapi::functions::create_simple_exec_request(ci.data->command, ci.data->arguments, buffer);
		int ret = ci.handler->exec(ci.data, buffer, reply);
		std::list<std::wstring> list;
		nscapi::functions::parse_simple_exec_result(reply, list);
		BOOST_FOREACH(std::wstring l, list) {
			message += l + _T("\n");
		}
		return ret;
	}
	return NSCAPI::returnIgnored;
}
int client::command_line_parser::query(configuration &config, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &msg, std::wstring &perf) {
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

	std::string buffer, reply;
	nscapi::functions::create_simple_query_request(config.data->command, config.data->arguments, buffer);
	int ret = config.handler->query(config.data, buffer, reply);
	nscapi::functions::parse_simple_query_response(reply, msg, perf);
	return ret;
}

int client::command_line_parser::exec(configuration &config, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result) {
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

	std::string buffer, reply;
	nscapi::functions::create_simple_exec_request(config.data->command, config.data->arguments, buffer);
	int ret = config.handler->exec(config.data, buffer, reply);
	std::list<std::wstring> list;
	nscapi::functions::parse_simple_exec_result(reply, list);
	BOOST_FOREACH(std::wstring l, list) {
		result += l + _T("\n");
	}
	return ret;
}

std::list<std::string> client::command_line_parser::submit(configuration &config, const std::wstring &command, std::list<std::wstring> &arguments) {
	boost::program_options::variables_map vm;

	po::options_description common("Common options");
	add_common_options(common, config.data);
	po::options_description submit("Submit  NSCP options");
	add_submit_options(submit, config.data);
	po::options_description desc("Allowed options");
	desc.add(common).add(submit).add(config.local);

	std::vector<std::wstring> vargs(arguments.begin(), arguments.end());
	po::positional_options_description p;
	p.add("arguments", -1);
	po::wparsed_options parsed = po::basic_command_line_parser<wchar_t>(vargs).options(desc).positional(p).run();
	po::store(parsed, vm);
	po::notify(vm);

	std::string buffer;
	nscapi::functions::create_simple_query_response(config.data->command, config.data->result, config.data->message, _T(""), buffer);
	return config.handler->submit(config.data, buffer);
}

