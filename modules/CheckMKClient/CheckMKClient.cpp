// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckMKClient.h"

#include <memory>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/settings/helper.hpp>

#include "check_mk_client.hpp"

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
    root_ = get_base_path();
    // Build the replacement generation aside and publish it once it is whole.
    // The previous one is dropped when the last query using it lets go, and its
    // destructor is what unloads its scripts and closes their lua_States.
    const std::shared_ptr<scripts::nscp::nscp_runtime_impl> nscp_runtime = std::make_shared<scripts::nscp::nscp_runtime_impl>(get_id(), get_core());
    const std::shared_ptr<lua::lua_runtime> lua_runtime = std::make_shared<lua::lua_runtime>(utf8::cvt<std::string>(root_.string()));
    lua_runtime->register_plugin(std::make_shared<check_mk::check_mk_plugin>());
    const std::shared_ptr<scripts::script_manager<lua::lua_traits> > scripts =
        std::make_shared<scripts::script_manager<lua::lua_traits> >(lua_runtime, nscp_runtime, get_id(), utf8::cvt<std::string>(alias));

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

      ("scripts", sh::fun_values_path([this, scripts] (auto key, auto value) { this->add_script(scripts, key, value); }),
	      "REMOTE TARGET DEFINITIONS", "",
	      "SCRIPT", "For more configuration options add a dedicated section")
      ;

    // clang-format on
    settings.alias().add_key_to_settings().add_string("channel", sh::string_key(&channel_, "CheckMK"), "CHANNEL", "The channel to listen to.")

        ;

    settings.register_all();
    settings.notify();

    client_.finalize(nscapi::settings_proxy::create(get_id(), get_core()));

    if (scripts->empty()) {
      add_script(scripts, "default", "default_check_mk.lua");
    }

    scripts->load_all();

    // Only now is the new generation reachable; anything still inside the old
    // one finishes on it.
    lua_runtime_ = lua_runtime;
    nscp_runtime_ = nscp_runtime;
    handler_->set_scripts(scripts);

    nscapi::core_helper core(get_core(), get_id());
    core.register_channel(channel_);
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

bool CheckMKClient::add_script(const std::shared_ptr<scripts::script_manager<lua::lua_traits> > &scripts, std::string alias, std::string file) {
  try {
    if (file.empty()) {
      file = alias;
      alias = "";
    }

    boost::optional<boost::filesystem::path> ofile = lua::lua_script::find_script(root_, file);
    if (!ofile) return false;
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
  // Unload the scripts before dropping the manager: the core waits for
  // in-flight dispatches before calling this, and unload_all runs the scripts'
  // own teardown while the runtime is still alive.
  if (const std::shared_ptr<scripts::script_manager<lua::lua_traits> > scripts = handler_->get_scripts()) scripts->unload_all();
  handler_->set_scripts(std::shared_ptr<scripts::script_manager<lua::lua_traits> >());
  lua_runtime_.reset();
  nscp_runtime_.reset();
  return true;
}

void CheckMKClient::query_fallback(const PB::Commands::QueryRequestMessage &request_message, PB::Commands::QueryResponseMessage &response_message) {
  client_.do_query(request_message, response_message);
}

bool CheckMKClient::commandLineExec(const int target_mode, const PB::Commands::ExecuteRequestMessage &request, PB::Commands::ExecuteResponseMessage &response) {
  if (target_mode == NSCAPI::target_module) return client_.do_exec(request, response, "submit_");
  return false;
}

void CheckMKClient::handleNotification(const std::string &, const PB::Commands::SubmitRequestMessage &request_message,
                                       PB::Commands::SubmitResponseMessage *response_message) {
  client_.do_submit(request_message, *response_message);
}
