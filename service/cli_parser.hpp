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
		, help(false)
		, version(false)
	{
		root.add_options()
			("help", po::bool_switch(&help), "produce help message")
			("settings-help", "Produce help message for the various settings related options")
			("service-help", "Produce help message for the various settings related service management")
			("client-help", "Produce help message for the various settings related client")
			("test-help", "Produce help message for the various settings related client")
/*
			("settings", "Enter settings mode and handle settings related commands")
			("service", "Enter service mode and handle service related commands")
			("client", "Enter client mode and handle client related commands")
			("test", "Start test and debug mode")
			*/
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



	int parse(int argc, wchar_t* argv[]) {

		typedef boost::function<int(int, wchar_t**)> handler_function;
		typedef std::map<std::string,handler_function> handler_map;
		typedef std::map<std::string,std::string> alias_map;
		handler_map handlers;
		alias_map aliases;
		handlers["settings"] = boost::bind(&cli_parser::parse_settings, this, _1, _2);
		handlers["service"] = boost::bind(&cli_parser::parse_service, this, _1, _2);
		handlers["client"] = boost::bind(&cli_parser::parse_client, this, _1, _2, _T(""));
		handlers["test"] = boost::bind(&cli_parser::parse_test, this, _1, _2);

		aliases["nrpe"] = "NRPEClient";
		aliases["nscp"] = "NSCPClient";
		aliases["nsca"] = "NSCAClient";
		aliases["eventlog"] = "CheckEventLog";
		aliases["python"] = "PythonScript";
		aliases["py"] = "PythonScript";
		aliases["lua"] = "LuaScript";
		aliases["syslog"] = "SyslogClient";

		if (argc > 1 && argv[1][0] != L'-') {
			std::string mod = utf8::cvt<std::string>(argv[1]);
			handler_map::const_iterator it = handlers.find(mod);
			if (it != handlers.end())
				return it->second(argc-1, &argv[1]);

			alias_map::const_iterator alias_it = aliases.find(mod);
			if (alias_it != aliases.end())
				return parse_client(argc-1, &argv[1], utf8::cvt<std::wstring>(alias_it->second));

			std::cerr << "Invalid module specified: " << mod << std::endl;
			std::cerr << "Available modules are: ";
			BOOST_FOREACH(const handler_map::value_type &itm, handlers) {
				std::cerr << itm.first << ", ";
			}
			BOOST_FOREACH(const alias_map::value_type &itm, aliases) {
				std::cerr << itm.first << ", ";
			}
			std::cerr << std::endl;
			return 1;
		}

		try {
			po::options_description all("Allowed options");
			all.add(root).add(common).add(service).add(settings).add(client);

			po::variables_map vm;
			po::wparsed_options parsed = 
				po::wcommand_line_parser((argc>2)?2:argc, argv).options(root).run();

			po::store(parsed, vm);
			po::notify(vm);

			BOOST_FOREACH(handler_map::value_type &it, handlers) {
				if (vm.count(it.first)) {
					return it.second(argc-1, &argv[1]);
				}
			}
			std::cerr << "First argument has to be one of the following: " << std::endl;
			std::cout << root << std::endl;
			return 1;
		} catch(std::exception & e) {
			std::cerr << "Unable to parse root option: " << e.what() << std::endl;
			return 1;
		} catch (...) {
			std::cerr << "Unable to parse root option" << std::endl;
			return 1;
		}
		return 0;
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
			mainClient.log_error(__FILE__, __LINE__, std::string("Unable to parse command line (settings): ") + e.what());
			return 1;
		}
	}

	int parse_client(int argc, wchar_t* argv[], std::wstring module = _T("")) {
		try {
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

			std::wstring command, combined_query;
			enum modes { exec, query, submit, none, combined};
			modes mode = none;

			if (vm.count("exec")) {
				command = vm["exec"].as<std::wstring>();
				mode = exec;
				if (vm.count("query")) {
					combined_query = vm["query"].as<std::wstring>();
					mode = combined;
				}
			} else if (vm.count("query")) {
				command = vm["query"].as<std::wstring>();
				mode = query;
			} else if (vm.count("submit")) {
				command = vm["submit"].as<std::wstring>();
				mode = submit;
			}

			if (vm.count("module"))
				module = vm["module"].as<std::wstring>();

			bool boot = false;
			if (vm.count("boot"))
				boot = true;

			std::vector<std::wstring> kvp_args;
			if (vm.count("argument"))
				kvp_args = vm["argument"].as<std::vector<std::wstring> >();

			std::vector<std::wstring> arguments = po::collect_unrecognized(parsed.options, po::include_positional);

			BOOST_FOREACH(std::wstring s, kvp_args) {
				std::wstring::size_type pos = s.find(L'=');
				if (pos == std::wstring::npos)
					arguments.push_back(_T("--") + s);
				else {
					arguments.push_back(_T("--") + s.substr(0,pos));
					arguments.push_back(s.substr(pos+1));
				}
			}

			if (vm.count("raw-argument"))
				kvp_args = vm["raw-argument"].as<std::vector<std::wstring> >();
			BOOST_FOREACH(std::wstring s, kvp_args) {
				std::wstring::size_type pos = s.find(L'=');
				if (pos == std::wstring::npos)
					arguments.push_back(s);
				else {
					arguments.push_back(s.substr(0,pos));
					arguments.push_back(s.substr(pos+1));
				}
			}

			if (mainClient.should_log(NSCAPI::log_level::debug)) {
				mainClient.log_info(__FILE__, __LINE__, _T("Module: ") + module);
				mainClient.log_info(__FILE__, __LINE__, _T("Command: ") + command);
				mainClient.log_info(__FILE__, __LINE__, _T("Mode: ") + strEx::itos(mode));
				mainClient.log_info(__FILE__, __LINE__, _T("Boot: ") + strEx::itos(boot));
				if (!module.empty() && boot)
					mainClient.log_info(__FILE__, __LINE__, _T("Warning module and boot specified only THAT module will be loaded"));
				std::wstring args;
				BOOST_FOREACH(std::wstring s, arguments)
					strEx::append_list(args, s, _T(", "));
				mainClient.log_info(__FILE__, __LINE__, _T("Arguments: ") + args);
			}

			core_->boot_init();
			if (module.empty())
				core_->boot_load_all_plugins();
			else
				core_->boot_load_plugin(module);
			core_->boot_start_plugins(boot);
			int ret = 0;
			std::list<std::wstring> resp;
			if (mode == none) {
				mode = exec;
				std::wcerr << _T("Since no mode was specified assuming --exec (other options are --query and --submit)") << std::endl;
			}
			if (mode == query) {
				ret = mainClient.simple_query(module, command, arguments, resp);
			} else if (mode == exec || mode == combined) {
				ret = mainClient.simple_exec(module, command, arguments, resp);
				if (ret == NSCAPI::returnIgnored) {
					ret = 1;
					std::wcout << _T("Command not found (by module): ") << command << std::endl;
					resp.push_back(_T("Command not found: ") + command);
					mainClient.simple_exec(module, _T("help"), arguments, resp);
				} else if (mode == combined) {
					if (ret == NSCAPI::returnOK) {
						mainClient.reload(_T("service"));
						ret = mainClient.simple_query(module, combined_query, arguments, resp);
					} else {
						std::wcerr << _T("Failed to execute command, will not attempt query") << std::endl;
					}
				}
			} else if (mode == submit) {
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





