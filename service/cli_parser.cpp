#include "cli_parser.hpp"
#include "NSClient++.h"
#include "settings_client.hpp"
#include "service_manager.hpp"
#include <config.h>
#include <nsclient/logger.hpp>
#include <pid_file.hpp>

#include <settings/settings_core.hpp>
#include "../libs/settings_manager/settings_manager_impl.h"


namespace po = boost::program_options;
nsclient::logging::logger_interface* get_logger() {
	return nsclient::logging::logger::get_logger(); 
}
void log_error(unsigned int line, const std::string &message) {
	get_logger()->error("client", __FILE__, line, message);
}
void info(unsigned int line, const std::string &message) {
	get_logger()->info("client", __FILE__, line, message);
}
void info(unsigned int line, const std::string &message, const std::string &utf) {
	get_logger()->info("client", __FILE__, line, message + utf8::cvt<std::string>(utf));
}

cli_parser::cli_parser(NSClient* core) 
	: core_(core)
	, common_light("Common options")
	, common("Common options")
	, settings("Settings options")
	, service("Service Options")
	, client("Client Options")
	, unittest("Unit-test Options")
	, help(false)
	, version(false)
	, log_debug(false)
	, no_stderr(false)
{
	common_light.add_options()
		("settings", po::value<std::string>(&settings_store), "Override (temporarily) settings subsystem to use")
		("debug", po::bool_switch(&log_debug), "Set log level to debug (and show debug information)")
		("log", po::value<std::vector<std::string> >(&log_level), "The log level to use")
		("define", po::value<std::vector<std::string> >(&defines), "Defines to use to override settings. Syntax is PATH:KEY=VALUE")
		;

	common.add_options()
		("help", po::bool_switch(&help), "Show the help message for a given command")
		("no-stderr", po::bool_switch(&no_stderr), "Do not report errors on stderr")
		("version", po::bool_switch(&version), "Show version information")
		;

	settings.add_options()
		("migrate-to", po::value<std::string>(), "Migrate (copy) settings from current store to given target store")
		("migrate-from", po::value<std::string>(), "Migrate (copy) settings from old given store to current store")
		("generate", po::value<std::string>()->implicit_value("settings"), "Add comments to the current settings store (or a given one).")
		("add-missing", "Add all default values for all missing keys.")
		("validate", "Validate the current configuration (or a given configuration).")
		("load-all", "Load all plugins (currently only used with generate).")
		("path", po::value<std::string>()->default_value(""), "Path of key to work with.")
		("key", po::value<std::string>()->default_value(""), "Key to work with.")
		("set", po::value<std::string>()->implicit_value(""), "Set a key and path to a given value (use --key and --path).")
		("switch", po::value<std::string>(), "Set default context to use (similar to migrate but does NOT copy values)")
		("show", "Show a value given a key and path.")
		("list", "List all keys given a path.")
		("add-defaults", "Same as --add-missing")
		("remove-defaults", "Remove all keys which have default values (and empty sections)")
		("use-samples", "Add sample commands provided by some sections such as targets and real time filters")
		("activate-module", po::value<std::string>()->implicit_value(""), "Add a module (and its configuration options) to the configuration.")
		;

	service.add_options()
		("install", "Install service")
		("uninstall", "Uninstall service")
		("start", "Start service")
		("stop", "Stop service")
		("info", "Show information about service")
		("run", "Run as a service")
		("name", po::value<std::string>(), "Name of service")
		("pid", po::value<std::string>()->implicit_value(pidfile::get_default_pidfile("nscp")), "Create a pid file")
		("description", po::value<std::string>()->default_value(""), "Description of service")
		;

	client.add_options()
		("load-all", "Load all plugins.")
		("exec,e", po::value<std::string>()->implicit_value(""), "Run a command (execute)")
		("boot,b", "Boot the client before executing command (similar as running the command from test mode)")
		("query,q", po::value<std::string>(), "Run a query with a given name")
		("submit,s", po::value<std::string>(), "Submit passive check result")
		("module,M", po::value<std::string>(), "Load specific module (in other words do not auto detect module)")
		("argument,a", po::value<std::vector<std::string> >(), "List of arguments (arguments gets -- prefixed automatically (--argument foo=bar is the same as setting \"--foo bar\")")
		("raw-argument", po::value<std::vector<std::string> >(), "List of arguments (does not get -- prefixed)")
		;

	unittest.add_options()
		("language,l", po::value<std::string>()->implicit_value(""), "Language tests are written in")
		("argument,a", po::value<std::vector<std::string> >(), "List of arguments (gets -- prefixed automatically)")
		("raw-argument", po::value<std::vector<std::string> >(), "List of arguments (does not get -- prefixed)")
		;

		test.add_options()
		;

}

void cli_parser::init_logger() {
	BOOST_FOREACH(const std::string &level, log_level) {
		nsclient::logging::logger::set_log_level(level);
	}
}

bool cli_parser::process_common_options(std::string context, po::options_description &desc) {
	log_level.push_back("console");
	if (log_debug)
		log_level.push_back("debug");
	if (no_stderr)
		log_level.push_back("no-std-err");
	init_logger();
	if (nsclient::logging::logger::get_logger()->should_log(NSCAPI::log_level::debug)) {
		BOOST_FOREACH(const std::string & a, unknown_options) {
			get_logger()->info("client", __FILE__, __LINE__, "Extra options: " + utf8::cvt<std::string>(a));
		}
	}

	if (!settings_store.empty())
		core_->set_settings_context(settings_store);

	if (help) {
		std::cout << desc << std::endl;
		return true;
	}
	if (version) {
		std::string message = std::string(APPLICATION_NAME) + ", version: " + CURRENT_SERVICE_VERSION + ", Platform: " + SZARCH;
		std::cout << message << std::endl;
		return true;
	}
	return false;
}

cli_parser::handler_map cli_parser::get_handlers() {
	handler_map handlers;
	handlers["settings"] = boost::bind(&cli_parser::parse_settings, this, _1, _2);
	handlers["service"] = boost::bind(&cli_parser::parse_service, this, _1, _2);
	handlers["client"] = boost::bind(&cli_parser::parse_client, this, _1, _2, "");
	handlers["help"] = boost::bind(&cli_parser::parse_help, this, _1, _2);
	handlers["unit"] = boost::bind(&cli_parser::parse_unittest, this, _1, _2);
	return handlers;
}

cli_parser::alias_map cli_parser::get_aliases() {
	alias_map aliases;
	aliases["nrpe"] = "NRPEClient";
	aliases["nscp"] = "NSCPClient";
	aliases["nsca"] = "NSCAClient";
	aliases["eventlog"] = "CheckEventLog";
	aliases["python"] = "PythonScript";
	aliases["py"] = "PythonScript";
	aliases["lua"] = "LUAScript";
	aliases["syslog"] = "SyslogClient";
	aliases["sys"] = "CheckSystem";
	aliases["wmi"] = "CheckWMI";
	aliases["check_mk"] = "CheckMKClient";
	aliases["mk"] = "CheckMKClient";
	aliases["ext-scr"] = "CheckExternalScripts";
	aliases["ext"] = "CheckExternalScripts";
	aliases["web"] = "WEBServer";
	aliases["test"] = "CommandClient";
	return aliases;
}

int cli_parser::parse(int argc, char* argv[]) {
	handler_map handlers = get_handlers();
	alias_map aliases = get_aliases();
	if (argc > 1 && argv[1][0] != L'-') {
		std::string mod = utf8::cvt<std::string>(argv[1]);
		handler_map::const_iterator it = handlers.find(mod);
		if (it != handlers.end())
			return it->second(argc-1, &argv[1]);

		alias_map::const_iterator alias_it = aliases.find(mod);
		if (alias_it != aliases.end())
			return parse_client(argc-1, &argv[1], alias_it->second);

		parse_help(argc, argv);
		std::cerr << "Invalid module specified: " << mod << std::endl;
		return 1;
	} else if (argc > 2) {
		std::cerr << "First option should be the \"context\": nscp <context>" << std::endl;
		return 1;
	} else if (argc > 1) {
		std::string mod = utf8::cvt<std::string>(argv[1]);
		if (mod == "--version") {
			std::cout << APPLICATION_NAME << ", Version: " << CURRENT_SERVICE_VERSION << ", Platform: " << SZARCH << std::endl;
			return 0;
		}
		if (mod == "--help") {
			display_help();
		}
	}
	std::cout << "Usage: nscp <context>" << std::endl;
	std::cout << "  The <context> is the mode of operation ie. a type of command. " << std::endl;
	std::cout << "You can also use aliases here which are shorthands for 'nscp client --module <plugin>'" << std::endl;
	std::cout << "  Available context are: " << std::endl;
	std::string all_context;
	BOOST_FOREACH(const handler_map::value_type &e, handlers) {
		std::cout << "    " << describe(e.first) << std::endl;
		if (!all_context.empty())
			all_context += ", ";
		all_context += e.first;
	}
	std::cout << "  Available aliases are: " << std::endl;
	BOOST_FOREACH(const alias_map::value_type &e, aliases) {
		std::cout << "    " << describe(e.first, e.second) << std::endl;
		if (!all_context.empty())
			all_context += ", ";
		all_context += e.first;
	}
	std::cout << "  A short list of all available contexts are: " << std::endl << all_context << std::endl;

	return 1;
}

po::basic_parsed_options<char> cli_parser::do_parse(int argc, char* argv[], po::options_description &desc) {
	int pos = 0;
	for (;pos<argc;pos++) {
		if (strcmp(argv[pos], "..") == 0)
			break;
	}
	po::basic_parsed_options<char> parsed = po::command_line_parser(pos, argv).options(desc).allow_unregistered().run();
	unknown_options = po::collect_unrecognized(parsed.options, po::include_positional);
	for (int i=pos+1;i<argc;i++) {
		unknown_options.push_back(argv[i]);
	}
	return parsed;
}

void cli_parser::display_help() {
	try {
		po::options_description all("Allowed options");
		all.add(common_light).add(common).add(service).add(settings).add(client).add(test).add(unittest);
		std::cout << all << std::endl;

		std::cout << "First argument has to be one of the following: ";
		handler_map handlers = get_handlers();
		BOOST_FOREACH(const handler_map::value_type &itm, handlers) {
			std::cout << itm.first << ", ";
		}
		std::cout << std::endl;
		std::cout << "Or on of the following client aliases: ";
		alias_map aliases = get_aliases();
		BOOST_FOREACH(const alias_map::value_type &itm, aliases) {
			std::cout << itm.first << ", ";
		}
		std::cout << std::endl;
	} catch(std::exception & e) {
		std::cerr << "Unable to parse root option: " << e.what() << std::endl;
	} catch (...) {
		std::cerr << "Unable to parse root option" << std::endl;
	}

}

int cli_parser::parse_help(int argc, char* argv[]) {
	display_help();
	return 1;
}

int cli_parser::parse_settings(int argc, char* argv[]) {
	try {
		po::options_description all("Allowed options (settings)");
		all.add(common_light).add(common).add(settings);

		po::variables_map vm;
		po::store(do_parse(argc, argv, all), vm);
		po::notify(vm);

		if (process_common_options("settings", all))
			return 1;

		bool def = vm.count("add-defaults")==1 || vm.count("add-missing")==1;
		bool rem_def = vm.count("remove-defaults")==1;
		bool load_all = vm.count("load-all")==1;
		bool use_samples = vm.count("use-samples")==1;

		nsclient_core::settings_client client(core_, def, rem_def, load_all, use_samples);
		int ret = -1;
		try {
			if (vm.count("generate")) {
				std::string option = vm["generate"].as<std::string>();
				ret = client.generate(option);
			} else if (vm.count("migrate-to")) {
				ret = client.migrate_to(vm["migrate-to"].as<std::string>());
			} else if (vm.count("migrate-from")) {
				ret = client.migrate_from(vm["migrate-from"].as<std::string>());
			} else if (vm.count("set")) {
				ret = client.set(vm["path"].as<std::string>(), vm["key"].as<std::string>(), vm["set"].as<std::string>());
			} else if (vm.count("list")) {
				ret = client.list(vm["path"].as<std::string>());
			} else if (vm.count("show")) {
				if (vm.count("path") > 0 && vm.count("key") > 0)
					ret = client.show(vm["path"].as<std::string>(), vm["key"].as<std::string>());
				else {
					std::cerr << "Invalid command line please use --path and --key with show" << std::endl;
					ret = -1;
				}
			} else if (vm.count("activate-module")) {
				client.activate(vm["activate-module"].as<std::string>());
			} else if (vm.count("validate")) {
				ret = client.validate();
			} else if (vm.count("switch")) {
				client.switch_context(vm["switch"].as<std::string>());
				client.list_settings_info();
				ret = 0;
			} else {
				std::cout << all << std::endl;
				client.list_settings_info();
				return 1;
			}
		} catch(const std::exception & e) {
			std::cerr << e.what() << "\n";
		}
		return ret;
	} catch(const std::exception & e) {
		std::cerr << std::string("Unable to parse command line (settings): ") << e.what() << "\n";
		return 1;
	}
}


int cli_parser::parse_service(int argc, char* argv[]) {
	try {
		po::options_description all("Allowed options (service)");
		all.add(common_light).add(common).add(service);

		po::variables_map vm;
		po::store(do_parse(argc, argv, all), vm);
		po::notify(vm);

		if (process_common_options("service", all))
			return 1;

		std::string name;
		if (vm.count("name")) {
			name = vm["name"].as<std::string>();
		} else {
			name = nsclient::client::service_manager::get_default_service_name();
		}
		std::string desc;
		if (vm.count("description")) {
			desc = vm["description"].as<std::string>();
		} else {
			info(__LINE__, "TODO retrieve name from service here");
		}
		if (nsclient::logging::logger::get_logger()->should_log(NSCAPI::log_level::debug)) {
			info(__LINE__, "Service name: ", name);
			info(__LINE__, "Service description: ", desc);
		}
		if (vm.count("run")) {
			try {
				std::string pfile = pidfile::get_default_pidfile("nscp");
				if (vm.count("pid"))
					pfile = vm["pid"].as<std::string>();
				pidfile pid(pfile);
				if (vm.count("pid"))
					pid.create();
				core_->start_and_wait(name);
			} catch (const std::exception &e) {
				log_error(__LINE__, "Failed to start: " + utf8::utf8_from_native(e.what()));
			} catch (...) {
				log_error(__LINE__, "Unknown exception in service");
			}
		} else {
			nsclient::client::service_manager service_manager(name);

			if (vm.count("install")) {
				service_manager.install(desc);
			} else if (vm.count("uninstall")) {
				service_manager.uninstall();
			} else if (vm.count("start")) {
				service_manager.start();
			} else if (vm.count("stop")) {
				service_manager.stop();
			} else {
				if (vm.count("info") == 0) {
					std::cerr << all << std::endl;
					std::cerr << "Invalid syntax: missing argument" << std::endl;
				}
				std::cout << "Installed services: " << std::endl;;
				std::cout << name << ": " << service_manager.info() << std::endl;
				{
					nsclient::client::service_manager lsm("nsclientpp");
					std::string cmd = utf8::cvt<std::string>(lsm.info());
					if (!cmd.empty()) {
						std::cout << "nsclientpp (legacy): " << cmd << std::endl;
					}

				}
				return vm.count("info");
			}
		}
		return 0;
	} catch(std::exception & e) {
		std::cerr << "Unable to parse command line (settings): " << e.what() << "\n";
		return 1;
	}
}

struct client_arguments {
	std::string command, combined_query, module;
	std::vector<std::string> arguments;
	enum modes { exec, query, submit, none, combined};
	modes mode;
	bool boot;
	bool load_all;
	client_arguments() : mode(none), boot(false), load_all(false) {}

	void debug() {
		if (nsclient::logging::logger::get_logger()->should_log(NSCAPI::log_level::debug)) {
			info(__LINE__, "Module: " + module);
			info(__LINE__, "Command: " + command);
			info(__LINE__, "Extra Query: " + combined_query);
			info(__LINE__, "Mode: " + strEx::s::xtos(mode));
			info(__LINE__, "Boot: " + strEx::s::xtos(boot));
			info(__LINE__, "Load All: " + strEx::s::xtos(load_all));
			if (!module.empty() && boot)
				info(__LINE__, "Warning module and boot specified only THAT module will be loaded");
			std::string args;
			BOOST_FOREACH(std::string s, arguments)
				strEx::append_list(args, s, ", ");
			info(__LINE__, "Arguments: " + args);
		}

	}

	int exec_client_mode(NSClient* core_, const std::vector<std::string> &defines) {
		try {
			if (module == "CommandClient")
				boot = true;
			debug();

			core_->boot_init(true);
			BOOST_FOREACH(const std::string &s, defines) {
				std::string::size_type p1 = s.find(":");
				if (p1 == std::string::npos) {
					std::cerr << "Failed to parse: " << s << std::endl;
					continue;
				}
				std::string::size_type p2 = s.find("=", p1);
				if (p2 == std::string::npos) {
					std::cerr << "Failed to parse: " << s << std::endl;
					continue;
				}
				settings_manager::get_settings()->set_string(s.substr(0, p1), s.substr(p1+1, p2-p1-1), s.substr(p2+1));
			}
			if (load_all)                                                                                                                                                    
				core_->preboot_load_all_plugin_files();
			if (module.empty() || module == "CommandClient")
				core_->boot_load_all_plugins();
			else
				core_->boot_load_plugin(module);
			core_->boot_start_plugins(boot);
			int ret = 0;
			std::list<std::string> resp;
			if (mode == client_arguments::none) {
				mode = client_arguments::exec;
			}
			if (mode == client_arguments::query) {
				ret = core_->simple_query(module, command, arguments, resp);
				if (ret == NSCAPI::cmd_return_codes::returnIgnored) {
					resp.push_back("Command not found: " + command);
					std::string commands;
					BOOST_FOREACH(const std::string &c, core_->list_commands()) {
						strEx::append_list(commands, c);
					}
					resp.push_back("Available commands: " + commands);
				}
			} else if (mode == client_arguments::exec || mode == client_arguments::combined) {
				ret = core_->simple_exec(module + "." + command, arguments, resp);
				if (ret == NSCAPI::cmd_return_codes::returnIgnored) {
					ret = 1;
					resp.push_back("Command not found: " + command);
					core_->simple_exec("help", arguments, resp);
				} else if (mode == client_arguments::combined) {
					if (ret == NSCAPI::exec_return_codes::returnOK) {
						core_->reload("service");
						ret = core_->simple_query(module, combined_query, arguments, resp);
					} else {
						std::cerr << "Failed to execute command, will not attempt query" << std::endl;
					}
				}
			} else if (mode == client_arguments::submit) {
				std::cerr << "--submit is currently not supported" << std::endl;
			} else {
				std::cerr << "Need to specify one of --exec, --query or --submit" << std::endl;
			}
			core_->stop_unload_plugins_pre();
			core_->stop_exit_pre();
			core_->stop_exit_post();

			BOOST_FOREACH(std::string r, resp) {
				std::cout << r << std::endl;
			}
			return ret;
		} catch(const std::exception & e) {
			std::cerr << "Client: Unable to parse command line: " << utf8::utf8_from_native(e.what()) << std::endl;
			return NSCAPI::exec_return_codes::returnERROR;
		} catch(...) {
			std::cerr << "Client: Unable to parse command line: UNKNOWN" << std::endl;
			return NSCAPI::exec_return_codes::returnERROR;
		}
	}
};


int cli_parser::parse_client(int argc, char* argv[], std::string module_) {
	try {
		client_arguments args;

		args.module = module_;
		po::options_description all("Allowed options (client)");
		if (!module_.empty())
			all.add(common_light);
		else
			all.add(common_light).add(common).add(client);

		po::positional_options_description p;
		p.add("arguments", -1);

		po::variables_map vm;
		po::store(do_parse(argc, argv, all), vm);
		po::notify(vm);

		if (module_ == "CommandClient" &&  vm.count("log") == 0) {
			nsclient::logging::logger::set_log_level("debug");
		}

		if (process_common_options("client", all))
			return 1;


		if (vm.count("exec")) {
			args.command = vm["exec"].as<std::string>();
			args.mode = client_arguments::exec;
			if (vm.count("query")) {
				args.combined_query = vm["query"].as<std::string>();
				args.mode = client_arguments::combined;
			}
		} else if (vm.count("query")) {
			args.command = vm["query"].as<std::string>();
			args.mode = client_arguments::query;
		} else if (vm.count("submit")) {
			args.command = vm["submit"].as<std::string>();
			args.mode = client_arguments::submit;
		}

		args.load_all = vm.count("load-all")==1;

		if (vm.count("module"))
			args.module = vm["module"].as<std::string>();

		if (vm.count("boot"))
			args.boot = true;

		std::vector<std::string> kvp_args;
		if (vm.count("argument"))
			kvp_args = vm["argument"].as<std::vector<std::string> >();

		BOOST_FOREACH(const std::string &a, unknown_options)
			args.arguments.push_back(utf8::cvt<std::string>(a));

		BOOST_FOREACH(std::string s, kvp_args) {
			std::string::size_type pos = s.find('=');
			if (pos == std::string::npos)
				args.arguments.push_back("--" + s);
			else {
				args.arguments.push_back("--" + s.substr(0,pos));
				args.arguments.push_back(s.substr(pos+1));
			}
		}

		if (vm.count("raw-argument"))
			kvp_args = vm["raw-argument"].as<std::vector<std::string> >();
		BOOST_FOREACH(std::string s, kvp_args) {
			std::string::size_type pos = s.find('=');
			if (pos == std::string::npos)
				args.arguments.push_back(s);
			else {
				args.arguments.push_back(s.substr(0,pos));
				args.arguments.push_back(s.substr(pos+1));
			}
		}
		return args.exec_client_mode(core_, defines);
	} catch(const std::exception & e) {
		std::cerr << "Client: Unable to parse command line: " << utf8::utf8_from_native(e.what()) << std::endl;
		return 1;
	} catch(...) {
		std::cerr << "Client: Unable to parse command line: UNKNOWN" << std::endl;
		return 1;
	}
}

int cli_parser::parse_unittest(int argc, char* argv[]) {
	try {
		client_arguments args;
		settings_store = "dummy";
		po::options_description all("Allowed options (client)");
		all.add(common_light).add(common).add(unittest);

		po::positional_options_description p;
		p.add("arguments", -1);

		po::variables_map vm;
		po::store(do_parse(argc, argv, all), vm);
		po::notify(vm);

		if (process_common_options("unitest", all))
			return 1;


		if (vm.count("language")) {
			std::string lang = vm["language"].as<std::string>();
			if (lang == "python" || lang == "py") {
				args.command = "python-script";
				args.combined_query = "py_unittest";
				args.mode = client_arguments::combined;
				args.module = "PythonScript";
			} else if (lang == "lua") {
				args.command = "lua-script";
				args.combined_query = "lua_unittest";
				args.mode = client_arguments::combined;
				args.module = "LUAScript";
			} else {
				std::cerr << "Unknown language: " << lang << std::endl;
				return 1;
			}
		} else {
			args.command = "python-script";
			args.combined_query = "py_unittest";
			args.mode = client_arguments::combined;
			args.module = "PythonScript";
		}

		std::vector<std::string> kvp_args;
		if (vm.count("argument"))
			kvp_args = vm["argument"].as<std::vector<std::string> >();

		BOOST_FOREACH(const std::string &ws, unknown_options) {
			std::string s = utf8::cvt<std::string>(ws);
			args.arguments.push_back(s);
		}

		BOOST_FOREACH(const std::string &ws, kvp_args) {
			std::string s = utf8::cvt<std::string>(ws);
			std::string::size_type pos = s.find('=');
			if (pos == std::string::npos)
				args.arguments.push_back("--" + s);
			else {
				args.arguments.push_back("--" + s.substr(0,pos));
				args.arguments.push_back(s.substr(pos+1));
			}
		}

		if (vm.count("raw-argument"))
			kvp_args = vm["raw-argument"].as<std::vector<std::string> >();
		BOOST_FOREACH(std::string ws, kvp_args) {
			std::string s = utf8::cvt<std::string>(ws);
			std::string::size_type pos = s.find('=');
			if (pos == std::string::npos)
				args.arguments.push_back(s);
			else {
				args.arguments.push_back(s.substr(0,pos));
				args.arguments.push_back(s.substr(pos+1));
			}
		}
		return args.exec_client_mode(core_, defines);
	} catch(const std::exception & e) {
		std::cerr << "Client: Unable to parse command line: " << utf8::utf8_from_native(e.what()) << std::endl;
		return 1;
	} catch(...) {
		std::cerr << "Client: Unable to parse command line: UNKNOWN" << std::endl;
		return 1;
	}
}



std::string cli_parser::get_description(std::string key) 
{
	if (key == "settings") {
		return "Change and list settings as well as load and initialize modules.";
	} else if (key == "service") {
		return "Install/uninstall/display NSCP service.";
	} else if (key == "client") {
		return "Act as a client. This will run commands inside various installed modules and scripts.";
	} else if (key == "test") {
		return "The best way to diagnose and find errors with your configuration and setup.";
	} else if (key == "help") {
		return "Display the help screen.";
	} else if (key == "unit") {
		return "Run unit test scripts.";
	} else if (key == "nrpe") {
		return "Use a NRPE client to request information from other systems via NRPE similar to standard NRPE check_nrpe command.";
	} else if (key == "nscp") {
		return "Use a NSCP (the protocol) client to request information from other systems via NSCP.";
	} else if (key == "nsca") {
		return "Use a NSCA to submit passive checks to a remote system. Similar to the send_nsca command";
	} else if (key == "syslog") {
		return "Use SYSLOG (the protocol) to submit messages to a remote system.";
	} else if (key == "py" || key == "python") {
		return "Execute python scripts";
	} else if (key == "lua") {
		return "Execute lua scripts";
	} else if (key == "mk" || key == "check_mk") {
		return "Use a check_mk (the protocol) client to request information from other systems via check_mk.";
	} else if (key == "eventlog") {
		return "Inject event log message into the eventlog (mainly for testing eventlog filtering and setup)";
	} else if (key == "wmi") {
		return "Run WMI queries from command line";
	} else if (key == "sys") {
		return "Various system tools to get information about the system (generally PDH on windows curretly)";
	} else {
		return "TODO: describe: " + key;
	}
}
std::string cli_parser::describe(std::string key) {
	return key + "\n      " + get_description(key) + "\n";
}
std::string cli_parser::describe(std::string key, std::string alias) {
	return key + "   (same as nscp client --module " + alias + ")"
		"\n      " + get_description(key) +
		+ "\n";
}
