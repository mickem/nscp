// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckMKClient.h"

#include <config.h>

#include <memory>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <nscapi/protobuf/settings_functions.hpp>
#include <nscapi/settings/helper.hpp>
#include <scripts/script_root_setting.hpp>
#include <sstream>
#include <vector>

#include "check_mk_client.hpp"
#include "check_mk_install.hpp"

/**
 * Default c-tor
 * @return
 */
CheckMKClient::CheckMKClient()
    : handler_(std::make_shared<check_mk_client::check_mk_client_handler>()),
      client_("check_mk", handler_, std::make_shared<check_mk_handler::options_reader_impl>()) {}

/**
 * Default d-tor
 * @return
 */
CheckMKClient::~CheckMKClient() {}

bool CheckMKClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
  std::map<std::wstring, std::wstring> commands;

  try {
    // ${scripts}, as CheckMKServer and LUAScript use: find_script() looks in
    // <root>/lua, and the install base has no lua folder on any platform.
    // A configured `script root` moves it, as for LUAScript: read first, on
    // its own, since the runtime is built around the folder.
    {
      sh::settings_registry root_settings(nscapi::settings_proxy::create(get_id(), get_core()));
      root_settings.set_alias("check_mk", alias, "client");
      root_ = scripts::root::load(
                  root_settings, [this](const std::string &value) { return get_core()->expand_path(value); },
                  [](const std::string &why) { NSC_LOG_ERROR_STD(why); })
                  .scripts;
    }
    nscp_runtime_.reset(new scripts::nscp::nscp_runtime_impl(get_id(), get_core()));
    lua_runtime_.reset(new lua::lua_runtime(utf8::cvt<std::string>(root_.string())));
    lua_runtime_->register_plugin(std::shared_ptr<check_mk::check_mk_plugin>(new check_mk::check_mk_plugin()));
    handler_->set_scripts(std::make_shared<scripts::script_manager<lua::lua_traits>>(lua_runtime_, nscp_runtime_, get_id(), utf8::cvt<std::string>(alias)));

    sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
    settings.set_alias("check_mk", alias, "client");
    client_.set_path(settings.alias().get_settings_path("targets"));

    // clang-format off
    settings.alias().add_path_to_settings()
      ("CHECK MK CLIENT SECTION", "Section for check_mk active/passive check module.")

      ("handlers", sh::fun_values_path([this] (auto key, auto value) { this->add_command(key, value); }),
	      "CLIENT HANDLER SECTION", "",
	      "CLIENT", "For more configuration options add a dedicated section")

      ("targets", sh::fun_values_path([this] (auto key, auto value) { this->add_target(key, value); }),
	      "REMOTE TARGET DEFINITIONS", "",
	      "TARGET", "For more configuration options add a dedicated section")

      ("scripts", sh::fun_values_path([this] (auto key, auto value) { this->add_script(key, value); }),
	      "LUA SCRIPTS",
	      "Lua scripts to load, as `<alias> = <file>` (resolved under the script root, e.g. `scripts/lua/`). "
	      "A script registers a client callback with `check_mk.client(<function>)` to process the agent output fetched from a remote check_mk agent.",
	      "SCRIPT", "Lua script file to load")
      ;

    // clang-format on
    settings.alias().add_key_to_settings().add_string("channel", sh::string_key(&channel_, "CheckMK"), "CHANNEL", "The channel to listen to.")

        ;

    settings.register_all();
    settings.notify();

    client_.finalize(nscapi::settings_proxy::create(get_id(), get_core()));

    if (handler_->scripts()->empty()) {
      add_script("default", "default_check_mk.lua");
    }

    nscapi::core_helper core(get_core(), get_id());
    core.register_channel(channel_);

    handler_->scripts()->load_all();
  } catch (nsclient::nsclient_exception &e) {
    NSC_LOG_ERROR_EXR("NSClient API exception: ", e);
    return false;
  } catch (std::exception &e) {
    NSC_LOG_ERROR_EXR("loading", e);
    return false;
  } catch (...) {
    NSC_LOG_ERROR_EX("loading");
    return false;
  }
  return true;
}

bool CheckMKClient::add_script(std::string alias, std::string file) {
  try {
    if (file.empty()) {
      file = alias;
      alias = "";
    }

    boost::optional<boost::filesystem::path> ofile = lua::lua_script::find_script(root_, file);
    if (!ofile) {
      NSC_LOG_ERROR("Failed to find script: " + file);
      return false;
    }
    const auto scripts = handler_->scripts();
    if (!scripts) return false;
    scripts->add(alias, ofile.value().string());
    return true;
  } catch (...) {
    NSC_LOG_ERROR("Could not load script: " + file);
  }
  return false;
}

//////////////////////////////////////////////////////////////////////////
// Settings helpers
//

void CheckMKClient::add_target(std::string key, std::string arg) {
  try {
    client_.add_target(nscapi::settings_proxy::create(get_id(), get_core()), key, arg);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
  } catch (...) {
    NSC_LOG_ERROR_EX("Failed to add target: " + key);
  }
}

void CheckMKClient::add_command(std::string key, std::string arg) {
  try {
    nscapi::core_helper core(get_core(), get_id());
    std::string k = client_.add_command(key, arg);
    if (!k.empty()) core.register_command(k.c_str(), "NSCA relay for: " + key);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("Failed to add command: " + key, e);
  } catch (...) {
    NSC_LOG_ERROR_EX("Failed to add command: " + key);
  }
}

/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool CheckMKClient::unloadModule() {
  client_.clear();
  // A send() still running keeps its own reference; the manager (and its
  // scripts) go when that call returns.
  handler_->set_scripts(nullptr);
  lua_runtime_.reset();
  nscp_runtime_.reset();
  return true;
}

void CheckMKClient::query_fallback(const PB::Commands::QueryRequestMessage &request_message, PB::Commands::QueryResponseMessage &response_message) {
  client_.do_query(request_message, response_message);
}

bool CheckMKClient::commandLineExec(const int target_mode, const PB::Commands::ExecuteRequestMessage &request, PB::Commands::ExecuteResponseMessage &response) {
  for (const PB::Commands::ExecuteRequestMessage::Request &payload : request.payload()) {
    if (payload.arguments_size() > 0 && payload.arguments(0) == "install") return install_server(payload, response.add_payload());
    if (payload.arguments_size() > 0 && payload.arguments(0) == "show") return show_server(response.add_payload());
  }
  if (target_mode == NSCAPI::target_module) return client_.do_exec(request, response, "submit_");
  return false;
}

bool CheckMKClient::install_server(const PB::Commands::ExecuteRequestMessage::Request &request,
                                   PB::Commands::ExecuteResponseMessage::Response *response) const {
  namespace pf = nscapi::protobuf::functions;
  namespace mi = check_mk_install;

  // Start from what is configured, so a re-run that only adds a local check
  // does not reset the port or the allowed hosts.
  std::string allowed_hosts, port = "6556";
  {
    pf::settings_query q(get_id());
    q.get(mi::server_path, "allowed hosts", "");
    q.get("/settings/default", "allowed hosts", "127.0.0.1");
    q.get(mi::server_path, "port", "6556");
    get_core()->settings_query(q.request(), q.response());
    if (!q.validate_response()) {
      pf::set_response_bad(*response, q.get_response_error());
      return true;
    }
    std::string inherited = "127.0.0.1";
    for (const pf::settings_query::key_values &val : q.get_query_key_response()) {
      if (val.matches(mi::server_path, "allowed hosts"))
        allowed_hosts = val.get_string();
      else if (val.matches("/settings/default", "allowed hosts"))
        inherited = val.get_string();
      else if (val.matches(mi::server_path, "port"))
        port = val.get_string();
    }
    if (allowed_hosts.empty()) allowed_hosts = inherited;
  }

  std::vector<std::string> locals, mrpes, remove_locals, remove_mrpes;
  po::options_description desc("nscp mk install: set up the Checkmk agent (CheckMKServer)");
  // clang-format off
  desc.add_options()
    ("help", "Show help.")
    ("allowed-hosts", po::value<std::string>(&allowed_hosts)->default_value(allowed_hosts),
      "The Checkmk server(s) allowed to fetch the agent dump: a comma separated list of addresses, host names or networks (10.0.0.0/24).")
    ("port", po::value<std::string>(&port)->default_value(port),
      "The TCP port the agent listens on. 6556 is what Checkmk expects unless the host has an agent port rule.")
    ("local", po::value<std::vector<std::string>>(&locals)->composing(),
      "Add or replace a local check: \"<service name>=<command> [arguments]\". Repeat for more than one.")
    ("mrpe", po::value<std::vector<std::string>>(&mrpes)->composing(),
      "Add or replace an MRPE entry: \"<service name>=<command> [arguments]\". Repeat for more than one.")
    ("remove-local", po::value<std::vector<std::string>>(&remove_locals)->composing(),
      "Remove the local check with this service name.")
    ("remove-mrpe", po::value<std::vector<std::string>>(&remove_mrpes)->composing(),
      "Remove the MRPE entry with this service name.")
    ;
  // clang-format on

  try {
    // Everything after the `install` word.
    std::vector<std::string> args;
    for (int i = 1; i < request.arguments_size(); ++i) args.push_back(request.arguments(i));
    po::variables_map vm;
    po::store(nscapi::program_options::basic_command_line_parser(args).options(desc).run(), vm);
    po::notify(vm);

    if (vm.count("help")) {
      pf::set_response_good(
          *response, nscapi::program_options::help(desc,
                                                   "Usage: nscp mk install [--allowed-hosts <hosts>] [--port <port>] [--local \"<name>=<command> [args]\"]... "
                                                   "[--mrpe \"<name>=<command> [args]\"]... [--remove-local <name>]... [--remove-mrpe <name>]...\n"
                                                   "Set up the Checkmk agent. Run again to change it; `nscp mk show` lists what is configured.\n"));
      return true;
    }

    allowed_hosts = mi::trim(allowed_hosts);
    if (allowed_hosts.empty()) {
      // The listener fails closed on an empty list, so this would install an
      // agent nothing can reach.
      pf::set_response_bad(*response, "--allowed-hosts cannot be empty: name the Checkmk server(s), or 0.0.0.0/0,::/0 to accept anyone.");
      return true;
    }
    port = mi::trim(port);
    if (port.empty() || port.find_first_not_of("0123456789") != std::string::npos) {
      pf::set_response_bad(*response, "--port must be a port number, got: " + port);
      return true;
    }

    std::vector<mi::entry> local_entries, mrpe_entries;
    for (const std::string &s : locals) local_entries.push_back(mi::parse_entry(s, "local"));
    for (const std::string &s : mrpes) mrpe_entries.push_back(mi::parse_entry(s, "mrpe"));

    std::stringstream result;
    pf::settings_query s(get_id());
    for (const char *module : mi::required_modules) s.set(MAIN_MODULES_SECTION, module, "enabled");
    s.set(mi::server_path, "port", port);
    s.set(mi::server_path, "allowed hosts", allowed_hosts);
    result << "Checkmk agent enabled on port " << port << ", for: " << allowed_hosts << std::endl;
    result << "Modules enabled: CheckMKServer, CheckSystem, CheckDisk" << std::endl;
    if (mi::loopback_only(allowed_hosts))
      result << "WARNING: only this machine may connect. Pass --allowed-hosts <address of the Checkmk server> so the site can fetch the agent." << std::endl;

    for (const mi::entry &e : local_entries) {
      s.set(mi::local_path, e.name, e.value());
      result << "Local check \"" << e.name << "\": " << e.command << std::endl;
    }
    for (const mi::entry &e : mrpe_entries) {
      s.set(mi::mrpe_path, e.name, e.value());
      result << "MRPE entry \"" << e.name << "\": " << e.command << std::endl;
    }
    for (const std::string &name : remove_locals) {
      s.erase(mi::local_path, mi::trim(name));
      result << "Removed local check \"" << mi::trim(name) << "\"" << std::endl;
    }
    for (const std::string &name : remove_mrpes) {
      s.erase(mi::mrpe_path, mi::trim(name));
      result << "Removed MRPE entry \"" << mi::trim(name) << "\"" << std::endl;
    }
    s.save();
    get_core()->settings_query(s.request(), s.response());
    if (!s.validate_response()) {
      pf::set_response_bad(*response, s.get_response_error());
      return true;
    }
    result << "Restart NSClient++ to apply. `nscp mk show` lists the configuration." << std::endl;
    pf::set_response_good(*response, result.str());
    return true;
  } catch (const std::invalid_argument &e) {
    pf::set_response_bad(*response, e.what());
    return true;
  } catch (...) {
    nscapi::program_options::report_command_exception(desc, request.command(), *response, "Failed to install the Checkmk agent");
    return true;
  }
}

bool CheckMKClient::show_server(PB::Commands::ExecuteResponseMessage::Response *response) const {
  namespace pf = nscapi::protobuf::functions;
  namespace mi = check_mk_install;

  pf::settings_query q(get_id());
  for (const char *module : mi::required_modules) q.get(MAIN_MODULES_SECTION, module, "");
  q.get(mi::server_path, "port", "6556");
  q.get(mi::server_path, "allowed hosts", "");
  q.get("/settings/default", "allowed hosts", "127.0.0.1");
  q.list_configured(mi::local_path, false);
  q.list_configured(mi::mrpe_path, false);
  get_core()->settings_query(q.request(), q.response());
  if (!q.validate_response()) {
    pf::set_response_bad(*response, q.get_response_error());
    return true;
  }

  std::string port = "6556", allowed_hosts, inherited = "127.0.0.1";
  std::stringstream modules, locals, mrpes;
  for (const pf::settings_query::key_values &val : q.get_query_key_response()) {
    if (val.key().empty()) continue;
    if (val.matches(MAIN_MODULES_SECTION)) {
      const std::string state = val.get_string();
      modules << "  " << val.key() << ": " << (state.empty() ? "not enabled" : state) << std::endl;
    } else if (val.matches(mi::server_path, "port")) {
      port = val.get_string();
    } else if (val.matches(mi::server_path, "allowed hosts")) {
      allowed_hosts = val.get_string();
    } else if (val.matches("/settings/default", "allowed hosts")) {
      inherited = val.get_string();
    } else if (val.matches(mi::local_path)) {
      locals << "  " << val.key() << ": " << mi::command_of(val.get_string()) << std::endl;
    } else if (val.matches(mi::mrpe_path)) {
      mrpes << "  " << val.key() << ": " << mi::command_of(val.get_string()) << std::endl;
    }
  }

  std::stringstream result;
  result << "Checkmk agent (CheckMKServer)" << std::endl;
  result << "Modules:" << std::endl << modules.str();
  result << "Port: " << port << std::endl;
  result << "Allowed hosts: " << (allowed_hosts.empty() ? inherited + " (from /settings/default)" : allowed_hosts) << std::endl;
  result << "Local checks:" << std::endl << (locals.str().empty() ? "  (none)\n" : locals.str());
  result << "MRPE entries:" << std::endl << (mrpes.str().empty() ? "  (none)\n" : mrpes.str());
  pf::set_response_good(*response, result.str());
  return true;
}

void CheckMKClient::handleNotification(const std::string &, const PB::Commands::SubmitRequestMessage &request_message,
                                       PB::Commands::SubmitResponseMessage *response_message) {
  client_.do_submit(request_message, *response_message);
}
