/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cli_parser.hpp"
#include "NSClient++.h"
#include "settings_client.hpp"
#include "service_manager.hpp"
#include "../libs/settings_manager/settings_manager_impl.h"

#include <config.h>
#include <nsclient/logger/logger.hpp>
#ifndef WIN32
#include <pid_file.hpp>
#endif
#include <settings/settings_core.hpp>
#include <str/format.hpp>

#define LOG_MODULE "client"
namespace po = boost::program_options;

cli_parser::cli_parser(NSClient* core)
	: core_(core)
	, common_light("Common options")
	, common("Common options")
	, settings("Settings options")
	, service("Service Options")
	, client("Client Options")
	, help(false)
	, version(false)
	, log_debug(false)
	, no_stderr(false) {
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
		("restart", "Stop than start service")
		("info", "Show information about service")
		("run", "Run as a service")
		("name", po::value<std::string>(), "Name of service")
#ifndef WIN32
		("pid", po::value<std::string>()->implicit_value(pidfile::get_default_pidfile("nscp")), "Create a pid file")
#endif
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

	test.add_options()
		;
}

void cli_parser::init_logger() {
	BOOST_FOREACH(const std::string &level, log_level) {
		core_->get_logger()->set_log_level(level);
	}
}

bool cli_parser::process_common_options(std::string context, po::options_description &desc) {
	log_level.push_back("console");
	if (log_debug)
		log_level.push_back("debug");
	if (no_stderr)
		log_level.push_back("no-std-err");
	init_logger();
	if (core_->get_logger()->should_debug()) {
		BOOST_FOREACH(const std::string & a, unknown_options) {
			core_->get_logger()->info(LOG_MODULE, __FILE__, __LINE__, "Extra options: " + utf8::cvt<std::string>(a));
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
	aliases["op5"] = "Op5Client";
	return aliases;
}

int cli_parser::parse(int argc, char* argv[]) {
	setlocale(LC_ALL, "");
	handler_map handlers = get_handlers();
	alias_map aliases = get_aliases();
	if (argc > 1 && argv[1][0] != L'-') {
		std::string mod = utf8::cvt<std::string>(argv[1]);
		handler_map::const_iterator it = handlers.find(mod);
		if (it != handlers.end())
			return it->second(argc - 1, &argv[1]);

		alias_map::const_iterator alias_it = aliases.find(mod);
		if (alias_it != aliases.end())
			return parse_client(argc - 1, &argv[1], alias_it->second);

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
	po::basic_parsed_options<char> parsed = po::command_line_parser(argc, argv).options(desc).style(po::command_line_style::default_style & ~po::command_line_style::allow_guessing).allow_unregistered().run();
	unknown_options = po::collect_unrecognized(parsed.options, po::include_positional);
	return parsed;
}

void cli_parser::display_help() {
	try {
		po::options_description all("Allowed options");
		all.add(common_light).add(common).add(service).add(settings).add(client).add(test);
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
	} catch (std::exception & e) {
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

		bool def = vm.count("add-defaults") == 1 || vm.count("add-missing") == 1;
		bool rem_def = vm.count("remove-defaults") == 1;
		bool load_all = vm.count("load-all") == 1;
		bool use_samples = vm.count("use-samples") == 1;

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
		} catch (const std::exception & e) {
			std::cerr << e.what() << "\n";
		}
		return ret;
	} catch (const std::exception & e) {
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
			core_->get_logger()->info(LOG_MODULE, __FILE__, __LINE__, "TODO retrieve name from service here");
		}
		if (core_->get_logger()->should_debug()) {
			core_->get_logger()->info(LOG_MODULE, __FILE__, __LINE__, "Service name: " + name);
			core_->get_logger()->info(LOG_MODULE, __FILE__, __LINE__, "Service description: " + desc);
		}
		if (vm.count("run")) {
			try {
#ifndef WIN32
				std::string pfile = pidfile::get_default_pidfile("nscp");
				if (vm.count("pid"))
					pfile = vm["pid"].as<std::string>();
				pidfile pid(pfile);
				if (vm.count("pid"))
					pid.create();
#endif
				core_->start_and_wait(name);
			} catch (const std::exception &e) {
				core_->get_logger()->error(LOG_MODULE, __FILE__, __LINE__, "Failed to start: " + utf8::utf8_from_native(e.what()));
			} catch (...) {
				core_->get_logger()->error(LOG_MODULE, __FILE__, __LINE__, "Unknown exception in service");
			}
		} else {
			nsclient::client::service_manager service_manager(name);

			if (vm.count("install")) {
				service_manager.install(desc);
			} else if (vm.count("uninstall")) {
				service_manager.uninstall();
			} else if (vm.count("start")) {
				service_manager.start();
			} else if (vm.count("restart")) {
				service_manager.stop();
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
	} catch (std::exception & e) {
		std::cerr << "Unable to parse command line (settings): " << e.what() << "\n";
		return 1;
	}
}

struct client_arguments {
	std::string module;
	bool boot;
	bool load_all;
	client_arguments() : boot(false), load_all(false) {}

	bool run_pre(NSClient* core_, const std::vector<std::string> &defines) {
		try {
			if (module == "CommandClient")
				boot = true;

			core_->load_configuration(true);
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
				settings_manager::get_settings()->set_string(s.substr(0, p1), s.substr(p1 + 1, p2 - p1 - 1), s.substr(p2 + 1));
			}
			if (load_all)
				core_->boot_load_all_plugin_files();
			if (module.empty() || module == "CommandClient")
				core_->boot_load_active_plugins();
			else
				core_->boot_load_single_plugin(module);
			core_->boot_start_plugins(boot);
			return true;
		} catch (const std::exception & e) {
			std::cerr << "Client: Unable to parse command line: " << utf8::utf8_from_native(e.what()) << std::endl;
			return false;
		} catch (...) {
			std::cerr << "Client: Unable to parse command line: UNKNOWN" << std::endl;
			return false;
		}
	}
	int run_exec(NSClient* core_, std::string command, std::vector<std::string> arguments, std::list<std::string> &result) {
		try {
			int ret = 0;
			ret = core_->get_plugin_manager()->simple_exec(module + "." + command, arguments, result);
			if (ret == NSCAPI::cmd_return_codes::returnIgnored) {
				ret = 1;
				result.push_back("Command not found: " + command);
				core_->get_plugin_manager()->simple_exec(module + ".help", arguments, result);
			}
			return ret;
		} catch (const std::exception & e) {
			std::cerr << "Client: Unable to parse command line: " << utf8::utf8_from_native(e.what()) << std::endl;
			return NSCAPI::exec_return_codes::returnERROR;
		} catch (...) {
			std::cerr << "Client: Unable to parse command line: UNKNOWN" << std::endl;
			return NSCAPI::exec_return_codes::returnERROR;
		}
	}

	bool run_reload(NSClient* core_) {
		try {
			core_->reload("instant,service");
			return true;
		} catch (const std::exception & e) {
			std::cerr << "Client: Unable to parse command line: " << utf8::utf8_from_native(e.what()) << std::endl;
			return false;
		} catch (...) {
			std::cerr << "Client: Unable to parse command line: UNKNOWN" << std::endl;
			return false;
		}
	}

	int run_query(NSClient* core_, std::string command, std::vector<std::string> arguments, std::list<std::string> &result) {
		try {
			int ret = 0;
			ret = core_->get_plugin_manager()->simple_query(module, command, arguments, result);
			if (ret == NSCAPI::cmd_return_codes::returnIgnored) {
				result.push_back("Command not found: " + command);
				std::string commands;
				BOOST_FOREACH(const std::string &c, core_->get_plugin_manager()->get_commands()->list_commands()) {
					str::format::append_list(commands, c);
				}
				result.push_back("Available commands: " + commands);
			}
			return ret;
		} catch (const std::exception & e) {
			std::cerr << "Client: Unable to parse command line: " << utf8::utf8_from_native(e.what()) << std::endl;
			return NSCAPI::exec_return_codes::returnERROR;
		} catch (...) {
			std::cerr << "Client: Unable to parse command line: UNKNOWN" << std::endl;
			return NSCAPI::exec_return_codes::returnERROR;
		}
	}
	bool run_post(NSClient* core_) {
		try {
			core_->stop_nsclient();
			return true;
		} catch (const std::exception & e) {
			std::cerr << "Client: Unable to parse command line: " << utf8::utf8_from_native(e.what()) << std::endl;
			return false;
		} catch (...) {
			std::cerr << "Client: Unable to parse command line: UNKNOWN" << std::endl;
			return false;
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
			core_->get_logger()->set_log_level("debug");
		}

		if (process_common_options("client", all))
			return 1;

		args.load_all = vm.count("load-all") == 1;

		if (vm.count("module"))
			args.module = vm["module"].as<std::string>();

		if (vm.count("boot"))
			args.boot = true;

		std::vector<std::string> kvp_args;
		if (vm.count("argument"))
			kvp_args = vm["argument"].as<std::vector<std::string> >();

		std::vector<std::string> arguments;
		BOOST_FOREACH(const std::string &a, unknown_options)
			arguments.push_back(utf8::cvt<std::string>(a));

		BOOST_FOREACH(std::string s, kvp_args) {
			std::string::size_type pos = s.find('=');
			if (pos == std::string::npos)
				arguments.push_back("--" + s);
			else {
				arguments.push_back("--" + s.substr(0, pos));
				arguments.push_back(s.substr(pos + 1));
			}
		}

		if (vm.count("raw-argument"))
			kvp_args = vm["raw-argument"].as<std::vector<std::string> >();
		BOOST_FOREACH(std::string s, kvp_args) {
			std::string::size_type pos = s.find('=');
			if (pos == std::string::npos)
				arguments.push_back(s);
			else {
				arguments.push_back(s.substr(0, pos));
				arguments.push_back(s.substr(pos + 1));
			}
		}
		if (!args.run_pre(core_, defines)) {
			return NSCAPI::exec_return_codes::returnERROR;
		}
		int ret = 0;

		std::list<std::string> resp;
		if (vm.count("exec")) {
			ret = args.run_exec(core_, vm["exec"].as<std::string>(), arguments, resp);
		}
		if (vm.count("query")) {
			ret = args.run_query(core_, vm["query"].as<std::string>(), arguments, resp);
		}
		if (!vm.count("exec") &&  !vm.count("query")) {
			ret = args.run_exec(core_, "", arguments, resp);
		}
		args.run_post(core_);


		BOOST_FOREACH(std::string r, resp) {
			std::cout << utf8::to_encoding(r, "") << std::endl;
		}
		return ret;

	} catch (const std::exception & e) {
		std::cerr << "Client: Unable to parse command line: " << utf8::utf8_from_native(e.what()) << std::endl;
		return 1;
	} catch (...) {
		std::cerr << "Client: Unable to parse command line: UNKNOWN" << std::endl;
		return 1;
	}
}

int cli_parser::parse_unittest(int argc, char* argv[]) {
	try {
		client_arguments args;
		settings_store = "dummy";
		po::options_description all("Allowed options (client)");
		std::string lang, script;
		std::vector<std::string> cases;
		bool show_all = false;

		po::options_description unittest("Unit-test Options");
		unittest.add_options()
			("language,l", po::value<std::string>(&lang)->default_value("python"), "Language tests are written in")
			("script", po::value<std::string>(&script), "The script to test")
			("show-all", po::bool_switch(&show_all), "Show all results (not just errors)")
			("case,c", po::value<std::vector<std::string> >(&cases), "A list of expressions matching cases to run.")
			;
		all.add(common_light).add(common).add(unittest);

		po::positional_options_description p;
		p.add("arguments", -1);

		po::variables_map vm;
		po::store(do_parse(argc, argv, all), vm);
		po::notify(vm);


		if (process_common_options("unitest", all))
			return 1;

		if (lang == "python" || lang == "py") {
			lang = "py";
			args.module = "PythonScript";
		} else if (lang == "lua") {
			args.module = "LUAScript";
		} else {
			std::cerr << "Unknown language: " << lang << std::endl;
			return 1;
		}

		std::vector<std::string> install_args;
		install_args.push_back("--script");
		install_args.push_back(script);
		std::list<std::string> resp;
		if (!args.run_pre(core_, defines)) {
			return NSCAPI::exec_return_codes::returnERROR;
		}
		int ret = 0;
		if (lang == "py") {
			ret = args.run_exec(core_, "python-script", install_args, resp);
		} else {
			ret = args.run_exec(core_, "lua-script", install_args, resp);
		}
		if (ret != 0) {
			std::cerr << "Failed to setup unit test" << std::endl;
		}
		if (ret == 0) {
			if (!args.run_reload(core_)) {
				std::cerr << "Failed to reload configuration" << std::endl;
				ret = NSCAPI::exec_return_codes::returnERROR;
			}
		}

		std::vector<std::string> empty;
		if (ret == 0) {
			if (show_all) {
				ret = args.run_query(core_, lang + "_unittest_show_ok", empty, resp);
			}
		}
		if (ret == 0) {
			if (!cases.empty()) {
				ret = args.run_query(core_, lang + "_unittest_add_case", cases, resp);
			}
		}
		if (ret == 0) {
			ret = args.run_query(core_, lang + "_unittest", empty, resp);
		}
		args.run_post(core_);
		BOOST_FOREACH(std::string r, resp) {
			std::cout << utf8::to_encoding(r, "") << std::endl;
		}
		return ret;

	} catch (const std::exception & e) {
		std::cerr << "Client: Unable to parse command line: " << utf8::utf8_from_native(e.what()) << std::endl;
		return 1;
	} catch (...) {
		std::cerr << "Client: Unable to parse command line: UNKNOWN" << std::endl;
		return 1;
	}
}

std::string cli_parser::get_description(std::string key) {
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
		+"\n";
}