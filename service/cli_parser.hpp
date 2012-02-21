#pragma once

#include <boost/program_options.hpp>
#include "settings_client.hpp"

namespace po = boost::program_options;
class cli_parser {
	

	NSClient* core_;
	po::options_description root;
	po::options_description settings;
	po::options_description service;
	po::options_description client;
	po::options_description common;
	po::options_description unittest;

	bool help;
	bool version;
	std::wstring log_level;
	std::wstring settings_store;

public:
	cli_parser(NSClient* core) 
		: core_(core)
		, root("Allowed first option (Mode of operation)")
		, common("Common options")
		, settings("Settings options")
		, service("Service Options")
		, client("Client Options")
		, unittest("Unittest Options")
		, help(false)
		, version(false)
	{
		root.add_options()
			("help", po::bool_switch(&help), "produce help message")
			("version", po::bool_switch(&version), "Show version information")
			;
		common.add_options()
			("settings", po::value<std::wstring>(&settings_store), "Override (temporarily) settings subsystem to use")
			("help", po::bool_switch(&help), "produce help message")
			("debug", "Set log level to debug (and show debug information)")
			("log", po::value<std::wstring>(&log_level), "The log level to use")
			("version", po::bool_switch(&version), "Show version information")
			;

		settings.add_options()
			("migrate-to", po::value<std::wstring>(), "Migrate (copy) settings from current store to target store")
			("migrate-from", po::value<std::wstring>(), "Migrate (copy) settings from current store to target store")
			("generate", po::value<std::wstring>(), "(re)Generate a commented settings store or similar KEY can be trac, settings or the target store.")
			("add-defaults", "Add all default (if missing) values.")
			("path", po::value<std::wstring>()->default_value(_T("")), "Path of key to work with.")
			("key", po::value<std::wstring>()->default_value(_T("")), "Key to work with.")
			("set", po::value<std::wstring>()->implicit_value(_T("")), "Set a key and path to a given value.")
			("switch", po::value<std::wstring>(), "Set default context to use (similar to migrate but does NOT copy values)")
			("show", "Set a value given a key and path.")
			("list", "Set all keys below the path (or root).")
			;

		service.add_options()
			("install", "Install service")
			("uninstall", "Uninstall service")
			("start", "Start service")
			("stop", "Stop service")
			("info", "Show information about service")
			("run", "Run as a service")
			("name", po::value<std::wstring>(), "Name of service")
			("description", po::value<std::wstring>()->default_value(_T("")), "Description of service")
			;

		client.add_options()
			("exec,e", po::value<std::wstring>()->implicit_value(_T("")), "Run a command (execute)")
			("boot,b", "Boot the client before executing command (similar as running the command from test)")
			("query,q", po::value<std::wstring>(), "Run a query with a given name")
			("submit,s", po::value<std::wstring>(), "Name of query to ask")
			("module,M", po::value<std::wstring>(), "Name of module to load (if not specified all modules in ini file will be loaded)")
			("argument,a", po::wvalue<std::vector<std::wstring> >(), "List of arguments (gets -- prefixed automatically)")
			("raw-argument", po::wvalue<std::vector<std::wstring> >(), "List of arguments (does not get -- prefixed)")
			;
		unittest.add_options()
			("language,l", po::value<std::wstring>()->implicit_value(_T("")), "Language tests are written in")
			("argument,a", po::wvalue<std::vector<std::wstring> >(), "List of arguments (gets -- prefixed automatically)")
			("raw-argument", po::wvalue<std::vector<std::wstring> >(), "List of arguments (does not get -- prefixed)")
			;

	}

	bool process_common_options(std::string context, po::options_description &desc) {
		core_->set_console_log();
		if (!log_level.empty())
			core_->set_loglevel(log_level);
		if (!settings_store.empty())
			core_->set_settings_context(settings_store);

		if (help) {
			std::cout << desc << std::endl;
			return true;
		}
		if (version) {
			std::cout << APPLICATION_NAME << _T(", Version: ") << CURRENT_SERVICE_VERSION << _T(", Platform: ") << SZARCH << std::endl;
			return true;
		}
		return false;
	}

	typedef boost::function<int(int, wchar_t**)> handler_function;
	typedef std::map<std::string,handler_function> handler_map;
	typedef std::map<std::string,std::string> alias_map;

	handler_map get_handlers() {
		handler_map handlers;
		handlers["settings"] = boost::bind(&cli_parser::parse_settings, this, _1, _2);
		handlers["service"] = boost::bind(&cli_parser::parse_service, this, _1, _2);
		handlers["client"] = boost::bind(&cli_parser::parse_client, this, _1, _2, _T(""));
		handlers["test"] = boost::bind(&cli_parser::parse_test, this, _1, _2);
		handlers["help"] = boost::bind(&cli_parser::parse_help, this, _1, _2);
		handlers["unit"] = boost::bind(&cli_parser::parse_unittest, this, _1, _2);
		return handlers;
	}

	alias_map get_aliases() {
		alias_map aliases;
		aliases["nrpe"] = "NRPEClient";
		aliases["nscp"] = "NSCPClient";
		aliases["nsca"] = "NSCAClient";
		aliases["eventlog"] = "CheckEventLog";
		aliases["python"] = "PythonScript";
		aliases["py"] = "PythonScript";
		aliases["lua"] = "LuaScript";
		aliases["syslog"] = "SyslogClient";
		return aliases;
	}

	int parse(int argc, wchar_t* argv[]) {

		if (argc > 1 && argv[1][0] != L'-') {
			handler_map handlers = get_handlers();
			alias_map aliases = get_aliases();

			std::string mod = utf8::cvt<std::string>(argv[1]);
			handler_map::const_iterator it = handlers.find(mod);
			if (it != handlers.end())
				return it->second(argc-1, &argv[1]);

			alias_map::const_iterator alias_it = aliases.find(mod);
			if (alias_it != aliases.end())
				return parse_client(argc-1, &argv[1], utf8::cvt<std::wstring>(alias_it->second));

			parse_help(argc, argv);
			std::cerr << "Invalid module specified: " << mod << std::endl;
			return 1;
		}
		return parse_help(argc, argv);
	}
	int parse_help(int argc, wchar_t* argv[]) {
		try {

			po::options_description all("Allowed options");
			all.add(root).add(common).add(service).add(settings).add(client);
			std::cout << all << std::endl;

			std::cerr << "First argument has to be one of the following: ";
			handler_map handlers = get_handlers();
			BOOST_FOREACH(const handler_map::value_type &itm, handlers) {
				std::cerr << itm.first << ", ";
			}
			std::cerr << std::endl;
			std::cerr << "Or on of the following client aliases: ";
			alias_map aliases = get_aliases();
			BOOST_FOREACH(const alias_map::value_type &itm, aliases) {
				std::cerr << itm.first << ", ";
			}
			std::cerr << std::endl;
			return 1;
		} catch(std::exception & e) {
			std::cerr << "Unable to parse root option: " << e.what() << std::endl;
			return 1;
		} catch (...) {
			std::cerr << "Unable to parse root option" << std::endl;
			return 1;
		}
	}

	int parse_test(int argc, wchar_t* argv[]) {
		bool server = false;

		core_->set_console_log();
		core_->set_loglevel(_T("debug"));


		try {
			po::options_description all("Allowed options (settings)");
			all.add(common).add(settings);

			po::variables_map vm;
			po::store(po::parse_command_line(argc, argv, all), vm);
			po::notify(vm);

			if (process_common_options("settings", all))
				return 1;

			nsclient::simple_client client(core_);
			client.start();
			return 0;
		} catch(std::exception & e) {
			mainClient.log_error(__FILE__, __LINE__, std::string("Unable to parse command line (settings): ") + e.what());
			return 1;
		}
	}

	int parse_settings(int argc, wchar_t* argv[]) {
		try {
			po::options_description all("Allowed options (settings)");
			all.add(common).add(settings);

			po::variables_map vm;
			po::store(po::parse_command_line(argc, argv, all), vm);
			po::notify(vm);

			if (process_common_options("settings", all))
				return 1;

			bool def = vm.count("add-defaults")==1;

			nsclient::settings_client client(core_);

			std::wstring current = _T(""); //client.get_source();


			client.set_current(current);
			client.set_update_defaults(def);

			if (vm.count("generate")) {
				core_->set_settings_context(vm["generate"].as<std::wstring>());
				client.boot();
				int ret = client.generate(vm["generate"].as<std::wstring>());
				client.exit();
				return ret;
			}
			client.boot();


			int ret = -1;

			if (vm.count("migrate-to")) {
				ret = client.migrate_to(vm["migrate-to"].as<std::wstring>());
			}
			if (vm.count("migrate-from")) {
				ret = client.migrate_from(vm["migrate-from"].as<std::wstring>());
			}
			if (vm.count("set")) {
				ret = client.set(vm["path"].as<std::wstring>(), vm["key"].as<std::wstring>(), vm["set"].as<std::wstring>());
			}
			if (vm.count("list")) {
				ret = client.list(vm["path"].as<std::wstring>());
			}
			if (vm.count("show")) {
				ret = client.show(vm["path"].as<std::wstring>(), vm["key"].as<std::wstring>());
			}
			if (vm.count("switch")) {
				client.switch_context(vm["switch"].as<std::wstring>());
				ret = 0;
			}
			if (vm.count("settings")) {
				client.set_current(vm["settings"].as<std::wstring>());
				ret = 0;
			}
			client.exit();

			return ret;
		} catch(std::exception & e) {
			mainClient.log_error(__FILE__, __LINE__, std::string("Unable to parse command line (settings): ") + e.what());
			return 1;
		}
	}


	int parse_service(int argc, wchar_t* argv[]) {
		try {
			po::options_description all("Allowed options (service)");
			all.add(common).add(service);

			po::variables_map vm;
			po::store(po::parse_command_line(argc, argv, all), vm);
			po::notify(vm);

			if (process_common_options("service", all))
				return 1;

			std::wstring name;
			if (vm.count("name")) {
				name = vm["name"].as<std::wstring>();
			} else {
				mainClient.log_info(__FILE__, __LINE__, _T("TODO retrieve name from service here"));
			}
			std::wstring desc;
			if (vm.count("description")) {
				desc = vm["description"].as<std::wstring>();
			} else {
				mainClient.log_info(__FILE__, __LINE__, _T("TODO retrieve name from service here"));
			}
			if (mainClient.should_log(NSCAPI::log_level::debug)) {
				mainClient.log_info(__FILE__, __LINE__, _T("Service name: ") + name);
				mainClient.log_info(__FILE__, __LINE__, _T("Service description: ") + desc);
			}

			if (vm.count("run")) {
				try {
					mainClient.start_and_wait(name);
				} catch (...) {
					mainClient.log_error(__FILE__, __LINE__, _T("Unknown exception in service"));
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
				} else if (vm.count("info")) {
					service_manager.info();
				} else {
					std::cerr << "Missing argument" << std::endl;
					return 1;
				}
			}
			return 0;
		} catch(std::exception & e) {
			std::cerr << std::string("Unable to parse command line (settings): ") << e.what() << "\n";
			return 1;
		}
	}

	struct client_arguments {
		std::wstring command, combined_query, module;
		std::vector<std::wstring> arguments;
		enum modes { exec, query, submit, none, combined};
		modes mode;
		bool boot;
		client_arguments() : mode(none), boot(false) {}

		void debug() {
			if (mainClient.should_log(NSCAPI::log_level::debug)) {
				mainClient.log_info(__FILE__, __LINE__, _T("Module: ") + module);
				mainClient.log_info(__FILE__, __LINE__, _T("Command: ") + command);
				mainClient.log_info(__FILE__, __LINE__, _T("Extra Query: ") + combined_query);
				mainClient.log_info(__FILE__, __LINE__, _T("Mode: ") + strEx::itos(mode));
				mainClient.log_info(__FILE__, __LINE__, _T("Boot: ") + strEx::itos(boot));
				if (!module.empty() && boot)
					mainClient.log_info(__FILE__, __LINE__, _T("Warning module and boot specified only THAT module will be loaded"));
				std::wstring args;
				BOOST_FOREACH(std::wstring s, arguments)
					strEx::append_list(args, s, _T(", "));
				mainClient.log_info(__FILE__, __LINE__, _T("Arguments: ") + args);
			}

		}
	};
	int parse_client(int argc, wchar_t* argv[], std::wstring module_ = _T("")) {
		try {
			client_arguments args;

			args.module = module_;
			po::options_description all("Allowed options (client)");
			all.add(common).add(client);

			po::positional_options_description p;
			p.add("arguments", -1);

			po::variables_map vm;
			po::wparsed_options parsed = 
				po::wcommand_line_parser(argc, argv).options(all).allow_unregistered().run();
			po::store(parsed, vm);
			po::notify(vm);

			if (process_common_options("client", all))
				return 1;


			if (vm.count("exec")) {
				args.command = vm["exec"].as<std::wstring>();
				args.mode = client_arguments::exec;
				if (vm.count("query")) {
					args.combined_query = vm["query"].as<std::wstring>();
					args.mode = client_arguments::combined;
				}
			} else if (vm.count("query")) {
				args.command = vm["query"].as<std::wstring>();
				args.mode = client_arguments::query;
			} else if (vm.count("submit")) {
				args.command = vm["submit"].as<std::wstring>();
				args.mode = client_arguments::submit;
			}

			if (vm.count("module"))
				args.module = vm["module"].as<std::wstring>();

			if (vm.count("boot"))
				args.boot = true;

			std::vector<std::wstring> kvp_args;
			if (vm.count("argument"))
				kvp_args = vm["argument"].as<std::vector<std::wstring> >();

			args.arguments = po::collect_unrecognized(parsed.options, po::include_positional);

			BOOST_FOREACH(std::wstring s, kvp_args) {
				std::wstring::size_type pos = s.find(L'=');
				if (pos == std::wstring::npos)
					args.arguments.push_back(_T("--") + s);
				else {
					args.arguments.push_back(_T("--") + s.substr(0,pos));
					args.arguments.push_back(s.substr(pos+1));
				}
			}

			if (vm.count("raw-argument"))
				kvp_args = vm["raw-argument"].as<std::vector<std::wstring> >();
			BOOST_FOREACH(std::wstring s, kvp_args) {
				std::wstring::size_type pos = s.find(L'=');
				if (pos == std::wstring::npos)
					args.arguments.push_back(s);
				else {
					args.arguments.push_back(s.substr(0,pos));
					args.arguments.push_back(s.substr(pos+1));
				}
			}
			return exec_client_mode(args);
		} catch(const std::exception & e) {
			std::wcerr << _T("Client: Unable to parse command line: ") << utf8::to_unicode(e.what()) << std::endl;
			return 1;
		} catch(...) {
			std::wcerr << _T("Client: Unable to parse command line: UNKNOWN") << std::endl;
			return 1;
		}
	}

	int parse_unittest(int argc, wchar_t* argv[]) {
		try {
			client_arguments args;
			settings_store = _T("dummy");
			po::options_description all("Allowed options (client)");
			all.add(common).add(unittest);

			po::positional_options_description p;
			p.add("arguments", -1);

			po::variables_map vm;
			po::wparsed_options parsed = 
				po::wcommand_line_parser(argc, argv).options(all).allow_unregistered().run();
			po::store(parsed, vm);
			po::notify(vm);

			if (process_common_options("unitest", all))
				return 1;


			if (vm.count("language")) {
				std::wstring lang = vm["language"].as<std::wstring>();
				if (lang == _T("python") || lang == _T("py")) {
					args.command = _T("python-script");
					args.combined_query = _T("py_unittest");
					args.mode = client_arguments::combined;
					args.module = _T("PythonScript");
				} else {
					std::wcerr << _T("Unknown language: ") << lang << std::endl;
					return 1;
				}
			} else {
				args.command = _T("python-script");
				args.combined_query = _T("py_unittest");
				args.mode = client_arguments::combined;
				args.module = _T("PythonScript");
			}

			std::vector<std::wstring> kvp_args;
			if (vm.count("argument"))
				kvp_args = vm["argument"].as<std::vector<std::wstring> >();

			args.arguments = po::collect_unrecognized(parsed.options, po::include_positional);

			BOOST_FOREACH(std::wstring s, kvp_args) {
				std::wstring::size_type pos = s.find(L'=');
				if (pos == std::wstring::npos)
					args.arguments.push_back(_T("--") + s);
				else {
					args.arguments.push_back(_T("--") + s.substr(0,pos));
					args.arguments.push_back(s.substr(pos+1));
				}
			}

			if (vm.count("raw-argument"))
				kvp_args = vm["raw-argument"].as<std::vector<std::wstring> >();
			BOOST_FOREACH(std::wstring s, kvp_args) {
				std::wstring::size_type pos = s.find(L'=');
				if (pos == std::wstring::npos)
					args.arguments.push_back(s);
				else {
					args.arguments.push_back(s.substr(0,pos));
					args.arguments.push_back(s.substr(pos+1));
				}
			}
			return exec_client_mode(args);
		} catch(const std::exception & e) {
			std::wcerr << _T("Client: Unable to parse command line: ") << utf8::to_unicode(e.what()) << std::endl;
			return 1;
		} catch(...) {
			std::wcerr << _T("Client: Unable to parse command line: UNKNOWN") << std::endl;
			return 1;
		}
	}

	int exec_client_mode(client_arguments &args) {
		try {
			args.debug();

			core_->boot_init();
			if (args.module.empty())
				core_->boot_load_all_plugins();
			else
				core_->boot_load_plugin(args.module);
			core_->boot_start_plugins(args.boot);
			int ret = 0;
			std::list<std::wstring> resp;
			if (args.mode == client_arguments::none) {
				args.mode = client_arguments::exec;
				std::wcerr << _T("Since no mode was specified assuming --exec (other options are --query and --submit)") << std::endl;
			}
			if (args.mode == client_arguments::query) {
				ret = mainClient.simple_query(args.module, args.command, args.arguments, resp);
			} else if (args.mode == client_arguments::exec || args.mode == client_arguments::combined) {
				ret = mainClient.simple_exec(args.module, args.command, args.arguments, resp);
				if (ret == NSCAPI::returnIgnored) {
					ret = 1;
					std::wcout << _T("Command not found (by module): ") << args.command << std::endl;
					resp.push_back(_T("Command not found: ") + args.command);
					mainClient.simple_exec(args.module, _T("help"), args.arguments, resp);
				} else if (args.mode == client_arguments::combined) {
					if (ret == NSCAPI::returnOK) {
						mainClient.reload(_T("service"));
						ret = mainClient.simple_query(args.module, args.combined_query, args.arguments, resp);
					} else {
						std::wcerr << _T("Failed to execute command, will not attempt query") << std::endl;
					}
				}
			} else if (args.mode == client_arguments::submit) {
				std::wcerr << _T("--submit is currently not supported (but you can use --exec submit which is technically the same)") << std::endl;
			} else {
				std::wcerr << _T("Need to specify one of --exec, --query or --submit") << std::endl;
			}
			mainClient.stop_unload_plugins_pre();
			mainClient.stop_exit_pre();
			mainClient.stop_unload_plugins_post();
			mainClient.stop_exit_post();

			BOOST_FOREACH(std::wstring r, resp) {
				std::wcout << r << std::endl;
			}
			return ret;
		} catch(const std::exception & e) {
			std::wcerr << _T("Client: Unable to parse command line: ") << utf8::to_unicode(e.what()) << std::endl;
			return 1;
		} catch(...) {
			std::wcerr << _T("Client: Unable to parse command line: UNKNOWN") << std::endl;
			return 1;
		}
	}
};





