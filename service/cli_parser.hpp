#pragma once

#include <boost/program_options.hpp>
#include "settings_client.hpp"

namespace po = boost::program_options;
class cli_parser {
	

	NSClient* core_;
	po::options_description desc;
	po::options_description settings;
	po::options_description service;

public:
	cli_parser(NSClient* core) 
		: core_(core)
		, desc("Allowed options")
		, settings("Settings options")
		, service("Service Options")
	{
		desc.add_options()
			("help", "produce help message")
			("settings-help", "produce help message for the various settings related options")
			("service-help", "produce help message for the various settings related options")
			("settings", "Enter settings mode and handle settings related commands")
			("service", "Enter service mode and handle service related commands")
			("test", "Start test and debug mode")
			("debug", "Show debug information")
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

	}
	int parse(int argc, wchar_t* argv[]) {
		try {
			po::options_description all("Allowed options");
			all.add(desc).add(service).add(settings);

			po::variables_map vm;
			po::wparsed_options parsed = 
				po::wcommand_line_parser(argc, argv).options(all).allow_unregistered().run();

			//po::store(po::parse_command_line(argc, argv, desc), vm);
			po::store(parsed, vm);
			po::notify(vm);

			if (vm.count("debug")) {
				std::wcout << _T("Enabling debug mode")<<std::endl;
				core_->enableDebug(true);
			}


			if (vm.count("settings")) {
				mainClient.set_console_log();
				return parse_settings(argc, argv);
			}
			if (vm.count("service")) {
				//mainClient.set_console_log();
				return parse_service(argc, argv);
			}
			if (vm.count("test")) {
				mainClient.set_console_log();
				return parse_test(argc, argv);
			}
			if (vm.count("help")) {
				mainClient.set_console_log();
				std::cout << all << "\n";
				return 1;
			}
			if (vm.count("settings-help")) {
				mainClient.set_console_log();
				std::cout << settings << "\n";
				return 1;
			}
			if (vm.count("service-help")) {
				mainClient.set_console_log();
				std::cout << service << "\n";
				return 1;
			}
		} catch(std::exception & e) {
			std::cerr << "Unable to parse command line: " << e.what() << std::endl;
			return 1;
		} catch (...) {
			std::cerr << "Unhanded Exception" << std::endl;
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
			all.add(desc).add(settings);

			po::variables_map vm;
			po::store(po::parse_command_line(argc, argv, all), vm);
			po::notify(vm);

			if (vm.count("help")) {
				std::cout << all << "\n";
				return 1;
			}
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
			std::cerr << "Unable to parse command line (settings): " << e.what() << std::endl;
			return 1;
		}
	}


	int parse_service(int argc, wchar_t* argv[]) {
		try {
			po::options_description all("Allowed options (settings)");
			all.add(desc).add(service);

			po::variables_map vm;
			po::store(po::parse_command_line(argc, argv, all), vm);
			po::notify(vm);

			if (vm.count("help")) {
				std::cout << all << "\n";
				return 1;
			}
			bool debug = false;
			if (vm.count("debug")) {
				std::wcout << _T("----");
				debug = true;
			}
			std::wstring name;
			if (vm.count("name")) {
				name = vm["name"].as<std::wstring>();
			} else {
				std::wcout << _T("TODO retrieve name from service here") << std::endl;
			}
			std::wstring desc;
			if (vm.count("description")) {
				desc = vm["description"].as<std::wstring>();
			} else {
				std::wcout << _T("TODO retrieve name from service here") << std::endl;
			}
			if (debug) {
				std::wcout << _T("Service name: ") << name << std::endl;
				std::wcout << _T("Service description: ") << desc << std::endl;
			}

			std::wstringstream ss;
			ss << _T("run: ") << vm.count("run");
			ss << _T(", name: ") << vm.count("name");
			ss << _T(", info: ") << vm.count("info");

			std::wstring s = ss.str();
			OutputDebugString(s.c_str());
			std::wcout << s << std::endl;

			if (vm.count("run")) {
				try {
					mainClient.enableDebug(true);
					mainClient.start_and_wait(name);
				} catch (...) {
					std::wcerr << _T("Unknown exception in service") << std::endl;
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
			std::cerr << "Unable to parse command line (settings): " << e.what() << std::endl;
			return 1;
		}
	}
};





