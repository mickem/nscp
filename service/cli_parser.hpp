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

	bool debug;
	bool help;
	bool version;

public:
	cli_parser(NSClient* core) 
		: core_(core)
		, root("Allowed first option (Mode of operation)")
		, common("Common options")
		, settings("Settings options")
		, service("Service Options")
		, client("Client Options")
		, debug(false)
		, help(false)
		, version(false)
	{
		root.add_options()
			("help", po::bool_switch(&help), "produce help message")
			("settings-help", "Produce help message for the various settings related options")
			("service-help", "Produce help message for the various settings related service management")
			("client-help", "Produce help message for the various settings related client")
			("test-help", "Produce help message for the various settings related client")

			("settings", "Enter settings mode and handle settings related commands")
			("service", "Enter service mode and handle service related commands")
			("client", "Enter client mode and handle client related commands")
			("test", "Start test and debug mode")
			("version", po::bool_switch(&version), "Show version information")
			;
		common.add_options()
			("help", po::bool_switch(&help), "produce help message")
			("debug", po::bool_switch(&debug), "Show debug information")
			("version", po::bool_switch(&version), "Show version information")
			;

		settings.add_options()
			("migrate-to", po::value<std::wstring>(), "Migrate (copy) settings from current store to target store")
			("migrate-from", po::value<std::wstring>(), "Migrate (copy) settings from current store to target store")
			("generate", po::value<std::wstring>(), "(re)Generate a commented settings store or similar KEY can be trac, settings or the target store.")
			("add-defaults", "Add all default (if missing) values.")
			("path", po::value<std::wstring>()->default_value(_T("")), "Path of key to work with.")
			("key", po::value<std::wstring>()->default_value(_T("")), "Key to work with.")
			("set", po::value<std::wstring>(), "Set a key and path to a given value.")
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
			("command,C", po::value<std::wstring>(), "Name of command to start")
			("module,M", po::value<std::wstring>(), "Name of module to load (if not specified all modules in ini file will be loaded)")
			("arguments,a", po::wvalue<std::vector<std::wstring> >(), "List of arguments")
			;

	}

	bool process_common_options() {
		if (debug) {
			core_->enableDebug(true);
			core_->log_debug(__FILE__, __LINE__, _T("Enabling debug mode"));
		}

		if (help) {
			po::options_description all("Allowed options");
			all.add(root).add(common).add(service).add(settings).add(client);
			std::cout << all << std::endl;
			return true;
		}
		if (version) {
			std::cout << APPLICATION_NAME << _T(", Version: ") << CURRENT_SERVICE_VERSION << _T(", Platform: ") << SZARCH << std::endl;
			return true;
		}
		return false;
	}
	int parse(int argc, wchar_t* argv[]) {
		try {
			po::options_description all("Allowed options");
			all.add(root).add(common).add(service).add(settings).add(client);

			po::variables_map vm;
			po::wparsed_options parsed = 
				po::wcommand_line_parser((argc>2)?2:argc, argv).options(root).run();

			po::store(parsed, vm);
			po::notify(vm);

			if (process_common_options())
				return 1;

			if (vm.count("settings-help")) {
				std::cout << settings << std::endl;
				return 1;
			}
			if (vm.count("service-help")) {
				std::cout << service << std::endl;
				return 1;
			}
			if (vm.count("client-help")) {
				std::cout << client << std::endl;
				return 1;
			}

			if (vm.count("settings")) {
				mainClient.set_console_log();
				return parse_settings(argc-1, &argv[1]);
			}
			if (vm.count("service")) {
				//mainClient.set_console_log();
				return parse_service(argc-1, &argv[1]);
			}
			if (vm.count("client")) {
				return parse_client(argc-1, &argv[1]);
			}
			if (vm.count("test")) {
				mainClient.set_console_log();
				return parse_test(argc-1, &argv[1]);
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
// 		if (argc > 2 && wcscasecmp( _T("server"), argv[2] ) == 0 ) {
// 			server = true;
// 		}
// 		std::wcout << "Launching test mode - " << (server?_T("server mode"):_T("client mode")) << std::endl;
// 		LOG_MESSAGE_STD(_T("Booting: ") SZSERVICEDISPLAYNAME );
		nsclient::simple_client client(core_);
		client.start();
		return 0;
	}

	int parse_settings(int argc, wchar_t* argv[]) {
		try {
			po::options_description all("Allowed options (settings)");
			all.add(common).add(settings);

			po::variables_map vm;
			po::store(po::parse_command_line(argc, argv, all), vm);
			po::notify(vm);

			if (process_common_options())
				return 1;

			bool def = vm.count("add-defaults")==1;

			nsclient::settings_client client(core_);

			std::wstring current = _T(""); //client.get_source();


			client.set_current(current);
			client.set_update_defaults(def);

			client.boot();

			int ret = -1;

			if (vm.count("migrate-to")) {
				ret = client.migrate_to(vm["migrate-to"].as<std::wstring>());
			}
			if (vm.count("migrate-from")) {
				ret = client.migrate_from(vm["migrate-from"].as<std::wstring>());
			}
			if (vm.count("generate")) {
				ret = client.generate(vm["generate"].as<std::wstring>());
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

			if (process_common_options())
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
			if (debug) {
				mainClient.log_info(__FILE__, __LINE__, _T("Service name: ") + name);
				mainClient.log_info(__FILE__, __LINE__, _T("Service description: ") + desc);
			}

			if (vm.count("run")) {
				try {
					mainClient.enableDebug(true);
					mainClient.start_and_wait(name);
				} catch (...) {
					mainClient.log_error(__FILE__, __LINE__, _T("Unknown exception in service"));
				}
			} else {
				mainClient.set_console_log();
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

	int parse_client(int argc, wchar_t* argv[]) {
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

			if (process_common_options())
				return 1;

			std::wstring command;
			if (vm.count("command"))
				command = vm["command"].as<std::wstring>();

			std::wstring module;
			if (vm.count("module"))
				module = vm["module"].as<std::wstring>();

			std::vector<std::wstring> kvp_args;
			if (vm.count("arguments"))
				kvp_args = vm["arguments"].as<std::vector<std::wstring> >();

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


			mainClient.set_console_log();
			mainClient.enableDebug(debug);
			if (debug) {
				mainClient.log_info(__FILE__, __LINE__, _T("Module: ") + module);
				mainClient.log_info(__FILE__, __LINE__, _T("Command: ") + command);
				std::wstring args;
				BOOST_FOREACH(std::wstring s, arguments)
					strEx::append_list(args, s, _T(", "));
				mainClient.log_info(__FILE__, __LINE__, _T("Arguments: ") + args);
			}
			if (module.empty()) {
				core_->initCore(false);
			}
			int ret = 0;
			std::list<std::wstring> resp;
			if (mainClient.simple_exec(module, command, arguments, resp) == NSCAPI::returnIgnored) {
				ret = 1;
				std::wcout << _T("No handler for that command: ") << command << std::endl;
			}
			mainClient.exitCore(false);

			BOOST_FOREACH(std::wstring r, resp) {
				std::wcout << r << std::endl;
			}
			return ret;
		} catch(std::exception & e) {
			std::cout << "Client: Unable to parse command line: "  << e.what() << std::endl;
			return 1;
		} catch(...) {
			mainClient.log_error(__FILE__, __LINE__, "Unknown exception parsing command line");
			std::cout << "Client: Unknown exception parsing command line" << std::endl;
			return 1;
		}
	}
};





