// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "cli_parser.hpp"

#include <config.h>

#include <nsclient/logger/logger.hpp>

#include "../libs/settings_manager/settings_manager_impl.h"
#include "NSClient++.h"
#include "service_manager.hpp"
#include "settings_client.hpp"
#ifndef WIN32
#include <pid_file.hpp>
#endif
#ifdef HAVE_ONBOARDING
#include <onboarding/onboarding.hpp>
#endif
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <settings/settings_core.hpp>
#include <str/format.hpp>

#define LOG_MODULE "client"
namespace po = boost::program_options;

cli_parser::cli_parser(const std::shared_ptr<NSClient> &core)
    : core_(core),
      common_light("Common options"),
      common("Common options"),
      settings("Settings options"),
      service("Service Options"),
      client("Client Options"),
      help(false),
      version(false),
      log_debug(false),
      no_stderr(false) {
  // clang-format off

      common_light.add_options()
	("settings", po::value<std::string>(&settings_store), "Override (temporarily) settings subsystem to use")
	("debug", po::bool_switch(&log_debug), "Set log level to debug (and show debug information)")
	("log", po::value<std::vector<std::string> >(&log_level), "The log level to use")
	("log-backend", po::value<std::string>(&log_backend), "The log backend to use (file or console)")
	("define", po::value<std::vector<std::string> >(&defines), "Defines to use to override settings. Syntax is PATH:KEY=VALUE")
	("path-override", po::value<std::vector<std::string> >(&path_overrides),
	 "Override a path-resolver token. Syntax is KEY=VALUE, e.g. --path-override module-path=/foo/bar. "
	 "Repeatable. Wins over any [paths] entry in boot.ini. Use this from scripts/build tooling that needs to redirect "
	 "where modules, certs, logs, etc. are looked up without editing config files. "
	 "(Renamed from --path to avoid colliding with the `settings --path` subcommand option.)")
      ;

      common.add_options()
        ("help", po::bool_switch(&help), "Show the help message for a given command")
        ("no-stderr", po::bool_switch(&no_stderr), "Do not report errors on stderr")
        ("version", po::bool_switch(&version), "Show version information")
      ;

      settings.add_options()
        ("migrate-to", po::value<std::string>(), "Migrate (copy) settings from current store to given target store")
        ("migrate-layout", po::value<std::string>()->implicit_value("modern"),
         "Move this installation's writable state (configuration, certificates, logs, cache) to the folder the given layout uses "
         "and record the choice in boot.ini. 'modern' keeps it under %ProgramData% with access restricted to SYSTEM and "
         "administrators; 'legacy' keeps it beside the executable. Needs an elevated prompt, and a service restart to take effect.")
        ("dry-run", "Show what --migrate-layout would do, without changing anything.")
        ("migrate-from", po::value<std::string>(), "Migrate (copy) settings from old given store to current store")
        ("generate", po::value<std::string>()->implicit_value("settings"), "Deprecated use update in stead.")
        ("update", po::value<std::string>()->implicit_value("settings"), "Save config file (adding comments in place and moving sensitive keys to/from credential manager).")
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
        ("activate-module", po::value<std::vector<std::string> >()->multitoken(),
         "Add one or more modules (and their configuration options) to the configuration. "
         "Accepts several names in one go, e.g. --activate-module CheckHelpers CheckSystem CheckDisk")
        ("sort", "Re-write the active settings store with sections (and keys within each section) sorted alphabetically. "
                 "[/modules] is kept as the first section. Only meaningful for the INI backend; other backends fall back to a regular save.")
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
  // clang-format on
}

void cli_parser::init_logger() const {
  if (!log_backend.empty()) {
    core_->get_logger()->set_backend(log_backend);
  }
  for (const std::string &level : log_level) {
    core_->get_logger()->set_log_level(level);
  }
}

bool cli_parser::process_common_options(const std::string &context, const po::options_description &desc) {
  log_level.emplace_back("console");
  if (log_debug) log_level.emplace_back("debug");
  if (no_stderr) log_level.emplace_back("no-std-err");
  init_logger();
  if (core_->get_logger()->should_debug()) {
    for (const std::string &a : unknown_options) {
      core_->get_logger()->debug(LOG_MODULE, __FILE__, __LINE__, "Extra options: " + utf8::cvt<std::string>(a));
    }
  }

  if (!settings_store.empty()) core_->set_settings_context(settings_store);

  // Parse --path-override KEY=VALUE entries and stash on core_. NSClient
  // installs them in load_configuration_1 as the highest-precedence path layer
  // BEFORE init_settings loads boot.ini, so they can even relocate ${boot-conf}
  // and always win over boot.ini's own [paths] section.
  if (!path_overrides.empty()) {
    std::map<std::string, std::string> overrides;
    for (const std::string &raw : path_overrides) {
      const auto eq = raw.find('=');
      if (eq == std::string::npos || eq == 0) {
        core_->get_logger()->warning(LOG_MODULE, __FILE__, __LINE__, "Ignoring malformed --path-override argument (expected KEY=VALUE): " + raw);
        continue;
      }
      overrides[raw.substr(0, eq)] = raw.substr(eq + 1);
    }
    if (!overrides.empty()) core_->set_cli_path_overrides(std::move(overrides));
  }

  if (help) {
    std::cout << desc << std::endl;
    return true;
  }
  if (version) {
    const std::string message = std::string(APPLICATION_NAME) + ", version: " + CURRENT_SERVICE_VERSION + ", Platform: " + SZARCH;
    std::cout << message << std::endl;
    return true;
  }
  return false;
}

cli_parser::handler_map cli_parser::get_handlers() {
  handler_map handlers;
  handlers["settings"] = [this](const int argc, char **argv) { return this->parse_settings(argc, argv); };
  handlers["service"] = [this](const int argc, char **argv) { return this->parse_service(argc, argv); };
  handlers["client"] = [this](const int argc, char **argv) { return this->parse_client(argc, argv); };
  handlers["help"] = [this](const int argc, char **argv) { return this->parse_help(argc, argv); };
  handlers["unit"] = [this](const int argc, char **argv) { return this->parse_unittest(argc, argv); };
  handlers["enroll"] = [this](const int argc, char **argv) { return this->parse_enroll(argc, argv); };
  return handlers;
}

cli_parser::alias_map cli_parser::get_aliases() {
  alias_map aliases;
  aliases["nrpe"] = "NRPEClient";
  aliases["nscp"] = "NSCPClient";
  aliases["nsca"] = "NSCAClient";
  aliases["smtp"] = "SMTPClient";
  aliases["nrdp"] = "NRDPClient";
  aliases["nsca-ng"] = "NSCANgClient";
  aliases["icinga"] = "IcingaClient";
  aliases["graphite"] = "GraphiteClient";
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

int cli_parser::parse(int argc, char *argv[]) {
  setlocale(LC_ALL, "");
  handler_map handlers = get_handlers();
  alias_map aliases = get_aliases();
  if (argc > 1 && argv[1][0] != L'-') {
    std::string mod = utf8::cvt<std::string>(argv[1]);
    handler_map::const_iterator it = handlers.find(mod);
    if (it != handlers.end()) return it->second(argc - 1, &argv[1]);

    alias_map::const_iterator alias_it = aliases.find(mod);
    if (alias_it != aliases.end()) return parse_client(argc - 1, &argv[1], alias_it->second);

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
  for (const handler_map::value_type &e : handlers) {
    std::cout << "    " << describe(e.first) << std::endl;
    if (!all_context.empty()) all_context += ", ";
    all_context += e.first;
  }
  std::cout << "  Available aliases are: " << std::endl;
  for (const alias_map::value_type &e : aliases) {
    std::cout << "    " << describe(e.first, e.second) << std::endl;
    if (!all_context.empty()) all_context += ", ";
    all_context += e.first;
  }
  std::cout << "  A short list of all available contexts are: " << std::endl << all_context << std::endl;

  return 1;
}

po::basic_parsed_options<char> cli_parser::do_parse(int argc, char *argv[], const po::options_description &desc) {
  po::basic_parsed_options<char> parsed = po::command_line_parser(argc, argv)
                                              .options(desc)
                                              .style(po::command_line_style::default_style & ~po::command_line_style::allow_guessing)
                                              .allow_unregistered()
                                              .run();
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
    for (const handler_map::value_type &itm : handlers) {
      std::cout << itm.first << ", ";
    }
    std::cout << std::endl;
    std::cout << "Or one of the following client aliases: ";
    alias_map aliases = get_aliases();
    for (const alias_map::value_type &itm : aliases) {
      std::cout << itm.first << ", ";
    }
    std::cout << std::endl;
  } catch (std::exception &e) {
    std::cerr << "Unable to parse root option: " << e.what() << std::endl;
  } catch (...) {
    std::cerr << "Unable to parse root option" << std::endl;
  }
}

int cli_parser::parse_help(int argc, char *argv[]) {
  display_help();
  return 1;
}

int cli_parser::parse_settings(int argc, char *argv[]) {
  try {
    po::options_description all("Allowed options (settings)");
    all.add(common_light).add(common).add(settings);

    po::variables_map vm;
    po::store(do_parse(argc, argv, all), vm);
    po::notify(vm);

    if (process_common_options("settings", all)) return 1;

    bool def = vm.count("add-defaults") == 1 || vm.count("add-missing") == 1;
    bool rem_def = vm.count("remove-defaults") == 1;
    bool load_all = vm.count("load-all") == 1;
    bool use_samples = vm.count("use-samples") == 1;

    nsclient_core::settings_client settings_cli(core_, def, rem_def, load_all, use_samples);
    int ret = -1;
    try {
      if (vm.count("generate")) {
        std::string option = vm["generate"].as<std::string>();
        ret = settings_cli.generate(option);
      } else if (vm.count("update")) {
        std::string option = vm["update"].as<std::string>();
        ret = settings_cli.generate(option);
      } else if (vm.count("migrate-to")) {
        ret = settings_cli.migrate_to(vm["migrate-to"].as<std::string>());
      } else if (vm.count("migrate-layout")) {
        ret = settings_cli.migrate_layout(vm["migrate-layout"].as<std::string>(), vm.count("dry-run") == 1);
      } else if (vm.count("migrate-from")) {
        ret = settings_cli.migrate_from(vm["migrate-from"].as<std::string>());
      } else if (vm.count("set")) {
        ret = settings_cli.set(vm["path"].as<std::string>(), vm["key"].as<std::string>(), vm["set"].as<std::string>());
      } else if (vm.count("list")) {
        ret = settings_cli.list(vm["path"].as<std::string>());
      } else if (vm.count("show")) {
        const std::string show_path = vm["path"].as<std::string>();
        const std::string show_key = vm["key"].as<std::string>();
        // Neither given is meaningful - show() then describes the active
        // store. One without the other is a typo, and used to be answered
        // with silence and a success exit: both options carry a
        // default_value(""), so vm.count() reports them present even when the
        // operator never typed them and the guard below could never fire.
        if (show_path.empty() != show_key.empty()) {
          std::cerr << "Invalid command line please use --path and --key with show" << std::endl;
          ret = -1;
        } else {
          ret = settings_cli.show(show_path, show_key);
        }
      } else if (vm.count("activate-module")) {
        ret = settings_cli.activate(vm["activate-module"].as<std::vector<std::string> >());
      } else if (vm.count("validate")) {
        ret = settings_cli.validate();
      } else if (vm.count("sort")) {
        ret = settings_cli.sort();
      } else if (vm.count("switch")) {
        settings_cli.switch_context(vm["switch"].as<std::string>());
        settings_cli.list_settings_info();
        ret = 0;
      } else {
        std::cout << all << std::endl;
        settings_cli.list_settings_info();
        return 1;
      }
    } catch (const std::exception &e) {
      std::cerr << e.what() << "\n";
    }
    return ret;
  } catch (const std::exception &e) {
    std::cerr << std::string("Unable to parse command line (settings): ") << e.what() << "\n";
    return 1;
  }
}

int cli_parser::parse_service(int argc, char *argv[]) {
  try {
    po::options_description all("Allowed options (service)");
    all.add(common_light).add(common).add(service);

    po::variables_map vm;
    po::store(do_parse(argc, argv, all), vm);
    po::notify(vm);

    // The service is the one invocation that logs to a file: it runs
    // unattended, so console output goes nowhere. Everything else defaults to
    // the console and needs no write access anywhere. Decided here rather than
    // in the registered command line, because an already-installed service
    // keeps its old ImagePath until it is re-registered and would silently
    // stop logging on upgrade. An explicit --log-backend still wins.
    if (vm.count("run") && log_backend.empty()) {
      log_backend = "threaded-file";
    }

    if (process_common_options("service", all)) return 1;

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
        if (vm.count("pid")) pfile = vm["pid"].as<std::string>();
        pidfile pid(pfile);
        if (vm.count("pid")) pid.create();
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
        std::cout << "Installed services: " << std::endl;
        ;
        std::cout << name << ": " << service_manager.info() << std::endl;
        {
          nsclient::client::service_manager lsm("nsclientpp");
          auto cmd = utf8::cvt<std::string>(lsm.info());
          if (!cmd.empty()) {
            std::cout << "nsclientpp (legacy): " << cmd << std::endl;
          }
        }
        if (vm.count("info") == 0) {
          return 1;
        }
      }
    }
    return 0;
  } catch (std::exception &e) {
    std::cerr << "Unable to parse command line (settings): " << e.what() << "\n";
    return 1;
  }
}

void apply_overrides(const std::vector<std::string> &defines) {
  for (const std::string &s : defines) {
    std::string::size_type p1 = s.find(':');
    if (p1 == std::string::npos) {
      std::cerr << "Failed to parse: " << s << std::endl;
      continue;
    }
    const std::string::size_type p2 = s.find('=', p1);
    if (p2 == std::string::npos) {
      std::cerr << "Failed to parse: " << s << std::endl;
      continue;
    }
    settings_manager::get_settings()->set_string(s.substr(0, p1), s.substr(p1 + 1, p2 - p1 - 1), s.substr(p2 + 1));
  }
}

struct client_arguments {
  std::string module;
  bool boot;
  bool load_all;
  client_arguments() : boot(false), load_all(false) {}

  bool run_pre(std::shared_ptr<NSClient> core_, const std::vector<std::string> &defines) {
    try {
      if (module == "CommandClient") boot = true;
      core_->load_configuration_1();
      apply_overrides(defines);
      // A failed boot here means no modules can be loaded at all (no modules
      // folder, for instance). Carrying on produces a process that starts,
      // answers nothing and exits again - say so instead.
      if (!core_->load_configuration_2(true)) {
        std::cerr << "Client: Failed to boot (see the log above)" << std::endl;
        return false;
      }
      if (load_all) core_->boot_load_all_plugin_files();
      if (module.empty() || module == "CommandClient")
        core_->boot_load_active_plugins();
      else
        core_->boot_load_single_plugin(module);
      core_->boot_start_plugins(boot);
      return true;
    } catch (const std::exception &e) {
      std::cerr << "Client: Unable to parse command line: " << utf8::utf8_from_native(e.what()) << std::endl;
      return false;
    } catch (...) {
      std::cerr << "Client: Unable to parse command line: UNKNOWN" << std::endl;
      return false;
    }
  }
  int run_exec(std::shared_ptr<NSClient> core_, const std::string &command, const std::vector<std::string> &arguments, std::list<std::string> &result) const {
    try {
      int ret = 0;
      ret = core_->get_plugin_manager()->simple_exec(module + "." + command, arguments, result);
      if (ret == NSCAPI::cmd_return_codes::returnIgnored) {
        ret = 1;
        result.push_back("Command not found: " + command);
        core_->get_plugin_manager()->simple_exec(module + ".help", arguments, result);
      }
      return ret;
    } catch (const std::exception &e) {
      std::cerr << "Client: Unable to parse command line: " << utf8::utf8_from_native(e.what()) << std::endl;
      return NSCAPI::exec_return_codes::returnERROR;
    } catch (...) {
      std::cerr << "Client: Unable to parse command line: UNKNOWN" << std::endl;
      return NSCAPI::exec_return_codes::returnERROR;
    }
  }

  static bool run_reload(std::shared_ptr<NSClient> core_) {
    try {
      core_->reload("instant,service");
      return true;
    } catch (const std::exception &e) {
      std::cerr << "Client: Unable to parse command line: " << utf8::utf8_from_native(e.what()) << std::endl;
      return false;
    } catch (...) {
      std::cerr << "Client: Unable to parse command line: UNKNOWN" << std::endl;
      return false;
    }
  }

  int run_query(std::shared_ptr<NSClient> core_, const std::string &command, const std::vector<std::string> &arguments, std::list<std::string> &result) const {
    try {
      int ret = 0;
      ret = core_->get_plugin_manager()->simple_query(module, command, arguments, result);
      if (ret == NSCAPI::cmd_return_codes::returnIgnored) {
        result.push_back("Command not found: " + command);
        std::string commands;
        for (const std::string &c : core_->get_plugin_manager()->get_commands()->list_commands()) {
          str::format::append_list(commands, c);
        }
        result.push_back("Available commands: " + commands);
      }
      return ret;
    } catch (const std::exception &e) {
      std::cerr << "Client: Unable to parse command line: " << utf8::utf8_from_native(e.what()) << std::endl;
      return NSCAPI::exec_return_codes::returnERROR;
    } catch (...) {
      std::cerr << "Client: Unable to parse command line: UNKNOWN" << std::endl;
      return NSCAPI::exec_return_codes::returnERROR;
    }
  }
  static bool run_post(std::shared_ptr<NSClient> core_) {
    try {
      core_->stop_nsclient();
      return true;
    } catch (const std::exception &e) {
      std::cerr << "Client: Unable to parse command line: " << utf8::utf8_from_native(e.what()) << std::endl;
      return false;
    } catch (...) {
      std::cerr << "Client: Unable to parse command line: UNKNOWN" << std::endl;
      return false;
    }
  }
};

int cli_parser::parse_client(int argc, char *argv[], const std::string &module_) {
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

    if (module_ == "CommandClient" && vm.count("log") == 0) {
      core_->get_logger()->set_log_level("debug");
    }

    if (process_common_options("client", all)) return 1;

    args.load_all = vm.count("load-all") == 1;

    if (vm.count("module")) args.module = vm["module"].as<std::string>();

    if (vm.count("boot")) args.boot = true;

    std::vector<std::string> kvp_args;
    if (vm.count("argument")) kvp_args = vm["argument"].as<std::vector<std::string> >();

    std::vector<std::string> arguments;
    for (const std::string &a : unknown_options) arguments.push_back(utf8::cvt<std::string>(a));

    for (std::string &s : kvp_args) {
      std::string::size_type pos = s.find('=');
      if (pos == std::string::npos)
        arguments.push_back("--" + s);
      else {
        arguments.push_back("--" + s.substr(0, pos));
        arguments.push_back(s.substr(pos + 1));
      }
    }

    if (vm.count("raw-argument")) kvp_args = vm["raw-argument"].as<std::vector<std::string> >();
    for (std::string &s : kvp_args) {
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
    if (!vm.count("exec") && !vm.count("query")) {
      ret = args.run_exec(core_, "", arguments, resp);
    }
    client_arguments::run_post(core_);

    for (std::string &r : resp) {
      std::cout << utf8::to_encoding(r, "") << std::endl;
    }
    return ret;

  } catch (const std::exception &e) {
    std::cerr << "Client: Unable to parse command line: " << utf8::utf8_from_native(e.what()) << std::endl;
    return 1;
  } catch (...) {
    std::cerr << "Client: Unable to parse command line: UNKNOWN" << std::endl;
    return 1;
  }
}

int cli_parser::parse_unittest(int argc, char *argv[]) {
  try {
    client_arguments args;
    settings_store = "dummy";
    po::options_description all("Allowed options (client)");
    std::string lang, script;
    std::vector<std::string> cases;
    bool show_all = false;

    po::options_description unittest("Unit-test Options");
    // clang-format off
    unittest.add_options()
      ("language,l", po::value<std::string>(&lang)->default_value("python"), "Language tests are written in")
      ("script", po::value<std::string>(&script), "The script to test")
      ("show-all", po::bool_switch(&show_all), "Show all results (not just errors)")
      ("case,c", po::value<std::vector<std::string> >(&cases), "A list of expressions matching cases to run.")
    ;
    // clang-format on
    all.add(common_light).add(common).add(unittest);

    po::positional_options_description p;
    p.add("arguments", -1);

    po::variables_map vm;
    po::store(do_parse(argc, argv, all), vm);
    po::notify(vm);

    if (process_common_options("unitest", all)) return 1;

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
    install_args.emplace_back("--script");
    install_args.emplace_back(script);
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
      if (!client_arguments::run_reload(core_)) {
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
    client_arguments::run_post(core_);
    for (const std::string &r : resp) {
      std::cout << utf8::to_encoding(r, "") << std::endl;
    }
    return ret;

  } catch (const std::exception &e) {
    std::cerr << "Client: Unable to parse command line: " << utf8::utf8_from_native(e.what()) << std::endl;
    return 1;
  } catch (...) {
    std::cerr << "Client: Unable to parse command line: UNKNOWN" << std::endl;
    return 1;
  }
}

int cli_parser::parse_enroll(int argc, char *argv[]) {
#ifdef HAVE_ONBOARDING
  try {
    onboarding::enrollment_request request;
    std::string state_file;
    bool force = false;
    bool insecure = false;

    po::options_description enroll_desc("Enrollment options");
    // clang-format off
    enroll_desc.add_options()
      ("server", po::value<std::string>(&request.server_url), "Fleet server url, e.g. https://fleet.example.com")
      ("token", po::value<std::string>(&request.bootstrap_token), "One-time bootstrap token from the install command")
      ("hostname", po::value<std::string>(&request.hostname), "Hostname to report to the fleet server (default: this machine's hostname)")
      ("os", po::value<std::string>(&request.os), "Operating system to report to the fleet server (default: detected)")
      ("ca", po::value<std::string>(&request.ca), "CA bundle used to verify the fleet server certificate (default: the platform CA bundle, ${ca-path})")
      ("tls-version", po::value<std::string>(&request.tls_version)->default_value("tlsv1.2+"), "TLS version for the enrollment call")
      ("verify", po::value<std::string>(&request.verify_mode), "TLS verify mode (default: certificate). 'none' disables server verification and requires --insecure")
      ("retries", po::value<unsigned int>(&request.max_attempts)->default_value(3), "Attempts for transient failures (rate limiting, server errors)")
      ("timeout", po::value<unsigned int>(&request.timeout_seconds)->default_value(60), "Seconds a single read or write to the fleet server may take before the attempt is abandoned. 0 waits forever, which lets an unresponsive server block the command indefinitely")
      ("state-file", po::value<std::string>(&state_file), "Where to store the enrolled identity (default: " DEFAULT_FLEET_STATE_LOCATION ")")
      ("force", po::bool_switch(&force), "Overwrite an existing enrollment state file")
      ("insecure", po::bool_switch(&insecure), "Allow an unauthenticated enrollment: plain HTTP, or HTTPS with --verify none. Either way the fleet server is not authenticated, so an on-path attacker can read the bootstrap token and supply the trust anchors this agent will use from then on - only on a trusted network or for testing")
    ;
    // clang-format on

    po::options_description all("Allowed options (enroll)");
    all.add(common_light).add(common).add(enroll_desc);

    po::variables_map vm;
    po::store(do_parse(argc, argv, all), vm);
    po::notify(vm);

    if (process_common_options("enroll", all)) return 1;

    if (request.server_url.empty() || request.bootstrap_token.empty()) {
      std::cerr << "Both --server and --token are required." << std::endl;
      std::cout << all << std::endl;
      return 1;
    }

    // The bootstrap token is a credential that exchanges for a client
    // certificate; over plain HTTP anyone on the network path can read it and
    // enroll as this host. Refuse unless the operator opts in with --insecure,
    // mirroring the NRPE insecure-mode precedent. (Loopback is not special-
    // cased: --insecure is cheap to add and keeps the rule predictable.)
    const std::string::size_type scheme_sep = request.server_url.find("://");
    const std::string scheme = scheme_sep == std::string::npos ? "" : request.server_url.substr(0, scheme_sep);
    if (boost::iequals(scheme, "http") && !insecure) {
      std::cerr << "Refusing to enroll over plain HTTP: the bootstrap token would be sent in cleartext, "
                   "so anyone on the network path could read it and enroll as this host."
                << std::endl;
      std::cerr << "Use an https:// server URL (recommended), or pass --insecure to allow plain HTTP anyway "
                   "(e.g. a trusted local network or for testing)."
                << std::endl;
      return 1;
    }

    // Load paths/settings so ${certificate-path} (and any tokens in a user
    // supplied --state-file) can be resolved.
    if (!core_->load_configuration_1()) {
      std::cerr << "Failed to load configuration" << std::endl;
      return 1;
    }
    if (state_file.empty()) state_file = DEFAULT_FLEET_STATE_LOCATION;
    state_file = core_->get_path()->expand_path(state_file);

    // Enrollment is where this agent decides who the fleet server is: the
    // response carries the certificate every later call pins against and the
    // key that authorises executable bundles. Getting that over an
    // unauthenticated channel means an on-path attacker can substitute both
    // and keep the host indefinitely - after which pinning and bundle
    // signature verification keep succeeding, against the attacker's material.
    // So an unverified enrollment has to be asked for, exactly like plain HTTP
    // above.
    if (boost::iequals(scheme, "https")) {
      if (request.ca.empty()) {
        // Same trust anchor the check modules default to: the exported Windows
        // ROOT bundle, or the distribution CA bundle on Linux.
        request.ca = core_->get_path()->expand_path("${ca-path}");
      }
      if (boost::iequals(request.verify_mode, "none")) {
        if (!insecure) {
          std::cerr << "Refusing to enroll without verifying the fleet server certificate (--verify none)." << std::endl;
          std::cerr << "The enrollment response supplies the certificate this agent will pin for every later call and the key it will trust for "
                       "executable bundles, so an unverified enrollment hands both to whoever answers."
                    << std::endl;
          std::cerr << "Point --ca at the issuing CA (recommended), or pass --insecure to accept an unauthenticated enrollment anyway." << std::endl;
          return 1;
        }
        std::cerr << "WARNING: enrolling without verifying the fleet server certificate. The trust anchors stored by this enrollment are only as "
                     "trustworthy as the network path you ran it over."
                  << std::endl;
      }
    }

    boost::system::error_code fs_error;
    if (boost::filesystem::exists(state_file, fs_error) && !force) {
      std::cerr << "This host is already enrolled (" << state_file << " exists)." << std::endl;
      std::cerr << "Use --force to discard the existing identity and enroll again." << std::endl;
      return 1;
    }
    const boost::filesystem::path state_dir = boost::filesystem::path(state_file).parent_path();
    // Only a directory we created ourselves may have its ownership changed
    // below: an existing one can be the packaged security folder, which also
    // holds shipped read-only material that is not ours to hand over.
    const bool created_state_dir = !state_dir.empty() && boost::filesystem::create_directories(state_dir, fs_error);

    std::cout << "Enrolling with " << request.server_url << "..." << std::endl;
    const onboarding::enrolled_identity state = onboarding::enroll(request);
    onboarding::save_state(state, state_file);

    // Enrollment is normally run with sudo while the service runs unprivileged,
    // so everything written here is root-owned and unreadable to the account
    // that has to load it at boot. The state directory is the reference: it is
    // created and owned by packaging, so whoever owns it is the service.
    const std::string owner_reference = core_->get_path()->expand_path("${data-path}");
    std::string owner_error;
    if (!onboarding::adopt_owner(state_file, owner_reference, owner_error)) {
      // The identity is written and valid - this is a warning, not a failed
      // enrollment. Say exactly what to run, because the symptom otherwise is a
      // healthy-looking agent that never appears in the fleet.
      std::cerr << "WARNING: " << owner_error << std::endl;
      std::cerr << "The service may not be able to read its identity. Fix it with: chown --reference=" << owner_reference << " " << state_file << std::endl;
    }
    if (created_state_dir) {
      std::string dir_error;
      onboarding::adopt_owner(state_dir.string(), owner_reference, dir_error);
    }
    std::cout << "Enrollment successful." << std::endl;
    std::cout << "  Identity stored in: " << state_file << std::endl;
    std::cout << "  Agent API (mTLS):   " << state.mtls_url << std::endl;

    // The core starts the fleet sync automatically whenever the enrollment
    // manifest written above exists - no module to enable. Only the include
    // of the rendered configuration (fleet.ini) needs to be wired into the
    // active settings store. The token is stored unexpanded so the config
    // stays relocatable; a placeholder fleet.ini avoids an include error
    // before the first sync.
    // The same token the core defaults its managed path to, so the include we
    // write and the directory the sync writes into cannot drift apart - and so
    // relocating ${fleet-folder} moves both at once. Identical on Windows and
    // unix; only the token's default differs. Declared out here so the failure
    // path below can tell the operator what to add by hand.
    const std::string fleet_ini_token = "${" FLEET_FOLDER_KEY "}/fleet.ini";
    try {
      const std::string fleet_ini = core_->get_path()->expand_path(fleet_ini_token);
      const boost::filesystem::path fleet_dir = boost::filesystem::path(fleet_ini).parent_path();
      if (!fleet_dir.empty()) boost::filesystem::create_directories(fleet_dir, fs_error);
      if (!boost::filesystem::exists(fleet_ini, fs_error)) {
        std::ofstream placeholder(fleet_ini.c_str());
        placeholder << "; Managed by the fleet sync - populated on the first sync." << std::endl;
      }
      // The sync writes fleet.ini, applied-state.json, the bundle cache and the
      // script tree here, all as the service account.
      if (!fleet_dir.empty()) {
        std::string fleet_error;
        if (!onboarding::adopt_owner(fleet_dir.string(), owner_reference, fleet_error)) {
          std::cerr << "WARNING: " << fleet_error << std::endl;
          std::cerr << "The fleet sync may not be able to apply configuration. Fix it with: chown -R --reference=" << owner_reference << " "
                    << fleet_dir.string() << std::endl;
        }
      }
      // Ask before adding the include, so the answer describes the host as the
      // operator left it rather than as enrollment just changed it.
      const bool local_config = settings_manager::has_local_configuration();
      settings_manager::get_settings()->set_string("/includes", "fleet", fleet_ini_token);
      settings_manager::get_settings()->save(false);
      std::cout << "  Fleet configuration sync starts on the next service start." << std::endl;
      if (local_config) {
        // A lookup reads the local store first and only falls back to the
        // include, so every key already configured here keeps winning after
        // enrollment. That is a legitimate way to run - pin something locally
        // and manage the rest centrally - but it is a surprise when a fleet
        // setting appears to have no effect, so say it once, at the point where
        // the two sources start competing.
        std::cout << std::endl;
        std::cout << "NOTE: this host already has local configuration, and local settings take precedence" << std::endl;
        std::cout << "      over the fleet-managed ones. Any key set in the configuration below will keep" << std::endl;
        std::cout << "      its local value even if the fleet server sends a different one:" << std::endl;
        std::cout << "        " << settings_manager::get_settings()->get_context() << std::endl;
        std::cout << "      The fleet server is told that local overrides exist, but never what they are." << std::endl;
      }
    } catch (const std::exception &e) {
      std::cerr << "Enrolled, but failed to add the fleet.ini include to the configuration: " << utf8::utf8_from_native(e.what()) << std::endl;
      std::cerr << "Add it manually: [/includes] fleet = " << fleet_ini_token << std::endl;
    }
    return 0;
  } catch (const onboarding::onboarding_error &e) {
    std::cerr << "Enrollment failed: " << e.what() << std::endl;
    if (e.retryable()) std::cerr << "This error may be temporary - try again later." << std::endl;
    return 1;
  } catch (const std::exception &e) {
    std::cerr << "Enrollment failed: " << utf8::utf8_from_native(e.what()) << std::endl;
    return 1;
  } catch (...) {
    std::cerr << "Enrollment failed: UNKNOWN" << std::endl;
    return 1;
  }
#else
  std::cerr << "Enrollment is not available in this build (compiled without OpenSSL)." << std::endl;
  return 1;
#endif
}

std::string cli_parser::get_description(const std::string &key) {
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
  } else if (key == "enroll") {
    return "Enroll (onboard) this host with a fleet server using a one-time bootstrap token.";
  } else if (key == "nrpe") {
    return "Use a NRPE client to request information from other systems via NRPE similar to standard NRPE check_nrpe command.";
  } else if (key == "nscp") {
    return "Use a NSCP (the protocol) client to request information from other systems via NSCP.";
  } else if (key == "nsca") {
    return "Use a NSCA to submit passive checks to a remote system. Similar to the send_nsca command";
  } else if (key == "nrdp") {
    return "Use NRDP to submit passive checks to a remote system via HTTP(S).";
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
    return "Various system tools to get information about the system (generally PDH on windows currently)";
  } else {
    return "TODO: describe: " + key;
  }
}
std::string cli_parser::describe(const std::string &key) { return key + "\n      " + get_description(key) + "\n"; }
std::string cli_parser::describe(const std::string &key, const std::string &alias) {
  return key + "   (same as nscp client --module " + alias +
         ")"
         "\n      " +
         get_description(key) + +"\n";
}
