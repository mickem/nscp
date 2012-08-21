#pragma once

#include <boost/program_options.hpp>
#include "settings_client.hpp"
#include <nsclient/logger.hpp>

namespace po = boost::program_options;
class cli_parser {
	

	NSClient* core_;
	po::options_description common;
	po::options_description settings;
	po::options_description service;
	po::options_description client;
	po::options_description unittest;
	po::options_description test;

	bool help;
	bool version;
	bool log_debug;
	std::wstring log_level;
	std::wstring settings_store;
	std::vector<std::wstring> unknown_options;

	static nsclient::logging::logger_interface* get_logger() {
		return nsclient::logging::logger::get_logger(); 
	}

public:
	cli_parser(NSClient* core) 
		: core_(core)
		, common("Common options")
		, settings("Settings options")
		, service("Service Options")
		, client("Client Options")
		, unittest("Unit-test Options")
		, test("Test Options")
		, help(false)
		, version(false)
		, log_debug(false)
	{
		common.add_options()
			("settings", po::value<std::wstring>(&settings_store), "Override (temporarily) settings subsystem to use")
			("help", po::bool_switch(&help), "produce help message")
			("debug", po::bool_switch(&log_debug), "Set log level to debug (and show debug information)")
			("log", po::value<std::wstring>(&log_level), "The log level to use")
			("version", po::bool_switch(&version), "Show version information")
			;

		settings.add_options()
			("migrate-to", po::value<std::wstring>(), "Migrate (copy) settings from current store to target store")
			("migrate-from", po::value<std::wstring>(), "Migrate (copy) settings from current store to target store")
			("generate", po::value<std::wstring>()->implicit_value(_T("settings")), "(re)Generate a commented settings store or similar KEY can be trac, settings or the target store.")
			("add-missing", "Add all default (if missing) values.")
			("validate", "Validate the current configuration (or a given configuration).")
			("load-all", "Load all plugins (currently only used with generate).")
			("path", po::value<std::wstring>()->default_value(_T("")), "Path of key to work with.")
			("key", po::value<std::wstring>()->default_value(_T("")), "Key to work with.")
			("set", po::value<std::wstring>()->implicit_value(_T("")), "Set a key and path to a given value.")
			("switch", po::value<std::wstring>(), "Set default context to use (similar to migrate but does NOT copy values)")
			("show", "Set a value given a key and path.")
			("list", "Set all keys below the path (or root).")
			("add-defaults", "Old name for --add-missing")
			("activate-module", po::value<std::wstring>()->implicit_value(_T("")), "Add a module (and its configuration options) to the configuration.")
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
			("load-all", "Load all plugins.")
			;

		unittest.add_options()
			("language,l", po::value<std::wstring>()->implicit_value(_T("")), "Language tests are written in")
			("argument,a", po::wvalue<std::vector<std::wstring> >(), "List of arguments (gets -- prefixed automatically)")
			("raw-argument", po::wvalue<std::vector<std::wstring> >(), "List of arguments (does not get -- prefixed)")
			;

		test.add_options()
			("log-to-file", "Enable file logger (defaults is console only)")
			;

	}

	bool process_common_options(std::string context, po::options_description &desc) {
		nsclient::logging::logger::get_logger()->set_console_log(true);
		if (log_debug) {
			log_level = _T("debug");
		}
		if (!log_level.empty())
			nsclient::logging::logger::set_log_level(log_level);
		if (nsclient::logging::logger::get_logger()->should_log(NSCAPI::log_level::debug)) {
			BOOST_FOREACH(const std::wstring & a, unknown_options) {
				get_logger()->info(__FILE__, __LINE__, _T("Extra options: ") + a);
			}
		}

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
		aliases["sys"] = "CheckSystem";
		aliases["wmi"] = "CheckWMI";
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

	po::basic_parsed_options<wchar_t> do_parse(int argc, wchar_t* argv[], po::options_description &desc) {
		int pos = 0;
		for (;pos<argc;pos++) {
			if (wcscmp(argv[pos], _T("..")) == 0)
				break;
		}
		po::basic_parsed_options<wchar_t> parsed = po::wcommand_line_parser(pos, argv).options(desc).allow_unregistered().run();
		unknown_options = po::collect_unrecognized(parsed.options, po::include_positional);
		for (int i=pos+1;i<argc;i++) {
			unknown_options.push_back(argv[i]);
		}
		return parsed;
	}

	int parse_help(int argc, wchar_t* argv[]) {
		try {

			po::options_description all("Allowed options");
			all.add(common).add(service).add(settings).add(client).add(test).add(unittest);
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
		try {

			po::options_description all("Allowed options (test)");
			all.add(common).add(test);

			po::variables_map vm;
			po::store(do_parse(argc, argv, all), vm);
			po::notify(vm);

			if (log_level.empty())
				log_level  = _T("debug");

			if (process_common_options("test", all))
				return 1;

			if (vm.count("log-to-file") == 0) {
				nsclient::logging::logger::set_backend("console");
			}

			nsclient::simple_client client(core_);
			client.start(log_level);
			return 0;
		} catch(const std::exception & e) {
			std::cerr << std::string("Unable to parse command line (test): ") << e.what() << "\n";
			return 1;
		}
	}

	int parse_settings(int argc, wchar_t* argv[]) {
		try {
			po::options_description all("Allowed options (settings)");
			all.add(common).add(settings);

			po::variables_map vm;
			po::store(do_parse(argc, argv, all), vm);
			po::notify(vm);

			if (process_common_options("settings", all))
				return 1;

			bool def = vm.count("add-defaults")==1 || vm.count("add-missing")==1;
			bool load_all = vm.count("load-all")==1;

			nsclient::settings_client client(core_, log_level, def, load_all);
			int ret = -1;

			if (vm.count("generate")) {
				ret = client.generate(vm["generate"].as<std::wstring>());
			} else if (vm.count("migrate-to")) {
				ret = client.migrate_to(vm["migrate-to"].as<std::wstring>());
			} else if (vm.count("migrate-from")) {
				ret = client.migrate_from(vm["migrate-from"].as<std::wstring>());
			} else if (vm.count("set")) {
				ret = client.set(vm["path"].as<std::wstring>(), vm["key"].as<std::wstring>(), vm["set"].as<std::wstring>());
			} else if (vm.count("list")) {
				ret = client.list(vm["path"].as<std::wstring>());
			} else if (vm.count("show")) {
				if (vm.count("path") > 0 && vm.count("key") > 0)
				ret = client.show(vm["path"].as<std::wstring>(), vm["key"].as<std::wstring>());
				else {
					std::cerr << "Invalid command line please use --path and --key with show" << std::endl;
					ret = -1;
				}
			} else if (vm.count("activate-module")) {
				client.activate(vm["activate-module"].as<std::wstring>());
			} else if (vm.count("validate")) {
				ret = client.validate();
			} else if (vm.count("switch")) {
				client.switch_context(vm["switch"].as<std::wstring>());
				client.list_settings_info();
				ret = 0;
			} else {
				std::cout << all << std::endl;
				client.list_settings_info();
				return 1;
			}

			return ret;
		} catch(const std::exception & e) {
			std::cerr << std::string("Unable to parse command line (settings): ") << e.what() << "\n";
			return 1;
		}
	}


	int parse_service(int argc, wchar_t* argv[]) {
		try {
			po::options_description all("Allowed options (service)");
			all.add(common).add(service);

			po::variables_map vm;
			po::store(do_parse(argc, argv, all), vm);
			po::notify(vm);

			if (process_common_options("service", all))
				return 1;

			std::wstring name;
			if (vm.count("name")) {
				name = vm["name"].as<std::wstring>();
			} else {
				name = nsclient::client::service_manager::get_default_service_name();
			}
			std::wstring desc;
			if (vm.count("description")) {
				desc = vm["description"].as<std::wstring>();
			} else {
				get_logger()->info(__FILE__, __LINE__, _T("TODO retrieve name from service here"));
			}
			if (nsclient::logging::logger::get_logger()->should_log(NSCAPI::log_level::debug)) {
				get_logger()->debug(__FILE__, __LINE__, _T("Service name: ") + name);
				get_logger()->debug(__FILE__, __LINE__, _T("Service description: ") + desc);
			}

			if (vm.count("run")) {
				try {
					core_->start_and_wait(name);
				} catch (...) {
					get_logger()->error(__FILE__, __LINE__, _T("Unknown exception in service"));
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
						std::wcerr << _T("Invalid syntax: missing argument") << std::endl;
					}
					std::wcout << _T("Installed services: ") << std::endl;;
					std::wcout << name << _T(": ") << service_manager.info() << std::endl;
					{
						nsclient::client::service_manager lsm(_T("nsclientpp"));
						std::wstring cmd = lsm.info();
						if (!cmd.empty()) {
							std::wcout << _T("nsclientpp (legacy): ") << cmd << std::endl;
						}

					}
					return vm.count("info");
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
		bool load_all;
		client_arguments() : mode(none), boot(false), load_all(false) {}

		void debug() {
			if (nsclient::logging::logger::get_logger()->should_log(NSCAPI::log_level::debug)) {
				get_logger()->info(__FILE__, __LINE__, _T("Module: ") + module);
				get_logger()->info(__FILE__, __LINE__, _T("Command: ") + command);
				get_logger()->info(__FILE__, __LINE__, _T("Extra Query: ") + combined_query);
				get_logger()->info(__FILE__, __LINE__, _T("Mode: ") + strEx::itos(mode));
				get_logger()->info(__FILE__, __LINE__, _T("Boot: ") + strEx::itos(boot));
				if (!module.empty() && boot)
					get_logger()->info(__FILE__, __LINE__, _T("Warning module and boot specified only THAT module will be loaded"));
				std::wstring args;
				BOOST_FOREACH(std::wstring s, arguments)
					strEx::append_list(args, s, _T(", "));
				get_logger()->info(__FILE__, __LINE__, _T("Arguments: ") + args);
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
			po::store(do_parse(argc, argv, all), vm);
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
			args.load_all = vm.count("load-all")==1;

			if (vm.count("module"))
				args.module = vm["module"].as<std::wstring>();

			if (vm.count("boot"))
				args.boot = true;

			std::vector<std::wstring> kvp_args;
			if (vm.count("argument"))
				kvp_args = vm["argument"].as<std::vector<std::wstring> >();

			args.arguments = unknown_options;

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
			po::store(do_parse(argc, argv, all), vm);
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
				} else if (lang == _T("lua")) {
						args.command = _T("LUAScript.run");
						args.combined_query = _T("lua_unittest");
						args.mode = client_arguments::combined;
						args.module = _T("LuaScript");
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

			args.arguments = unknown_options;

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

			core_->boot_init(log_level);
			if (args.load_all)                                                                                                                                                    
				core_->preboot_load_all_plugin_files();
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
				ret = core_->simple_query(args.module, args.command, args.arguments, resp);
			} else if (args.mode == client_arguments::exec || args.mode == client_arguments::combined) {
				ret = mainClient.simple_exec(args.command, args.arguments, resp);
				if (ret == NSCAPI::returnIgnored) {
					ret = 1;
					std::wcout << _T("Command not found (by module): ") << args.command << std::endl;
					resp.push_back(_T("Command not found: ") + args.command);
					core_->simple_exec(_T("help"), args.arguments, resp);
				} else if (args.mode == client_arguments::combined) {
					if (ret == NSCAPI::returnOK) {
						core_->reload(_T("service"));
						ret = core_->simple_query(args.module, args.combined_query, args.arguments, resp);
					} else {
						std::wcerr << _T("Failed to execute command, will not attempt query") << std::endl;
					}
				}
			} else if (args.mode == client_arguments::submit) {
				std::wcerr << _T("--submit is currently not supported (but you can use --exec submit which is technically the same)") << std::endl;
			} else {
				std::wcerr << _T("Need to specify one of --exec, --query or --submit") << std::endl;
			}
			core_->stop_unload_plugins_pre();
			core_->stop_exit_pre();
			core_->stop_exit_post();

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





