// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "LUAScript.h"

#include <boost/thread/locks.hpp>
#include <boost/program_options.hpp>
#include <boost/smart_ptr/make_shared_object.hpp>
#include <file_helpers.hpp>
#include <memory>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_convert.hpp>
#include <nscapi/protobuf/functions_exec.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <nscapi/settings/helper.hpp>
#include <str/utils.hpp>

#include "extscr_cli.h"

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

bool LUAScript::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
  try {
    root_ = get_core()->expand_path("${scripts}");
    nscp_runtime_ = std::make_shared<scripts::nscp::nscp_runtime_impl>(get_id(), get_core());
    // The lua runtime appends "/scripts/lua/lib/?.lua" to this base when building
    // package.path for require(), so it must be the install base ("${base-path}"),
    // not "${scripts}" (which already points at the scripts dir and would double it).
    lua_runtime_ = std::make_shared<lua::lua_runtime>(utf8::cvt<std::string>(get_core()->expand_path("${base-path}")));
    // Published atomically, and read the same way everywhere below: a check
    // thread copying this member while a reload replaces it is a data race on
    // the shared_ptr itself, not merely on what it points at. The reload
    // barrier in dll_plugin serialises the two today, but the barrier is a
    // property of the caller - this makes the member safe on its own terms.
    std::atomic_store(&scripts_,
                      std::make_shared<scripts::script_manager<lua::lua_traits> >(lua_runtime_, nscp_runtime_, get_id(), utf8::cvt<std::string>(alias)));

    sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
    settings.set_alias(alias, "lua");

    // Rebuilt on every load: loadModuleEx runs again on a settings reload, and
    // a list that only ever grew would keep folders an operator had removed.
    allowed_roots_ = nscp::scripts::allowed_roots();
    allowed_roots_.add(root_.string());

    // clang-format off
    settings.alias().add_key_to_settings()
      .add_string("additional script roots", sh::string_fun_key([this](const std::string &value) { this->add_script_roots(value); }, ""),
        "ADDITIONAL SCRIPT ROOTS",
        "Comma separated list of extra folders scripts may be loaded from, on top of the script folder itself. "
        "A script configured below has to live inside one of these, so that a path which climbs out of the script "
        "folder (`../foo.lua`) is refused rather than loaded. Add the folders of any scripts that are not installed "
        "with NSClient++ - a plugin package's own libexec directory, for example. Path tokens are expanded, so "
        "`${shared-path}/extra` works.", true)
      ;

    settings.alias().add_path_to_settings()

      ("scripts", sh::fun_values_path([this] (auto key, auto value) { this->loadScript(key, value); }),
	      "Lua scripts", "A list of scripts available to run from the LuaScript module.",
	      "Script", "A lua script to load")
      ;
    // clang-format on

    settings.register_all();
    settings.notify();

    // 		if (!scriptDirectory_.empty()) {
    // 			addAllScriptsFrom(scriptDirectory_);
    // 		}

    std::atomic_load(&scripts_)->load_all();
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("load", e);
    return false;
  } catch (...) {
    NSC_LOG_ERROR_EX("load");
    return false;
  }

  return true;
}

void LUAScript::add_script_roots(const std::string &value) {
  for (const std::string &entry : str::utils::split_lst(value, std::string(","))) {
    std::string trimmed = entry;
    boost::algorithm::trim(trimmed);
    if (trimmed.empty()) continue;
    allowed_roots_.add(get_core()->expand_path(trimmed));
  }
}

bool LUAScript::startModule() {
  try {
    std::atomic_load(&scripts_)->start_all();
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("start", e);
    return false;
  } catch (...) {
    NSC_LOG_ERROR_EX("start");
    return false;
  }

  return true;
}
bool LUAScript::loadScript(std::string alias, std::string file) {
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
    // The search ends with the value joined onto the script folder, and that
    // join does not stop it climbing back out: `../foo.lua` resolves to
    // ${scripts}/../foo.lua and would otherwise load from the installation
    // directory. The ext-scr CLI has always held show/delete inside the script
    // root; this is the same check on the path that actually runs code.
    if (!allowed_roots_.allows(ofile.value())) {
      NSC_LOG_ERROR("Refusing to load script outside the allowed roots: " + ofile.value().string() + " (allowed: " + allowed_roots_.describe() +
                    "). Add its folder to 'additional script roots' under the lua section if it belongs there.");
      return false;
    }
    NSC_DEBUG_MSG_STD("Adding script: " + ofile.value().string());
    // Reached from settings.notify() inside loadModuleEx, so the manager the
    // same call just published is there - but read it the same way as
    // everywhere else rather than touching the member directly.
    const std::shared_ptr<scripts::script_manager<lua::lua_traits> > scripts = std::atomic_load(&scripts_);
    if (!scripts) {
      NSC_LOG_ERROR("Failed to add script, module is not loaded: " + file);
      return false;
    }
    scripts->add(alias, ofile.value().string());
    return true;
  } catch (...) {
    NSC_LOG_ERROR_EX("load script");
  }
  return false;
}

bool LUAScript::unloadModule() {
  // Take the manager out of the member first, then work through the local
  // copy: a check thread that loaded the pointer just before this keeps the
  // manager alive until it returns, and one arriving after sees null.
  const std::shared_ptr<scripts::script_manager<lua::lua_traits> > scripts =
      std::atomic_exchange(&scripts_, std::shared_ptr<scripts::script_manager<lua::lua_traits> >());
  if (scripts) {
    scripts->unload_all();
  }
  return true;
}

void LUAScript::query_fallback(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                               const PB::Commands::QueryRequestMessage &request_message) {
  // Hold our own reference and register as a dispatcher for the whole call:
  // an unload on another thread otherwise deletes the script (and its
  // lua_State) while it runs. A script that queries a command its own module
  // serves lands here again on this thread, which the counter allows.
  const std::shared_ptr<scripts::script_manager<lua::lua_traits> > scripts = std::atomic_load(&scripts_);
  if (!scripts) return nscapi::protobuf::functions::set_response_bad(*response, "Module is not loaded");
  const scripts::script_manager<lua::lua_traits>::dispatch_guard dispatch(*scripts);
  if (!dispatch.entered()) return nscapi::protobuf::functions::set_response_bad(*response, "Module is unloading");
  boost::optional<scripts::command_definition<lua::lua_traits> > cmd = scripts->find_command(scripts::nscp::tags::query_tag, request.command());
  if (!cmd) {
    cmd = scripts->find_command(scripts::nscp::tags::simple_query_tag, request.command());
    if (!cmd) return nscapi::protobuf::functions::set_response_bad(*response, "Failed to find command: " + request.command());
    return lua_runtime_->on_query(request.command(), cmd.value().information, cmd.value().function, true, request, response, request_message);
  }
  return lua_runtime_->on_query(request.command(), cmd.value().information, cmd.value().function, false, request, response, request_message);
}

bool LUAScript::commandLineExec(const int target_mode, const PB::Commands::ExecuteRequestMessage::Request &request,
                                PB::Commands::ExecuteResponseMessage::Response *response, const PB::Commands::ExecuteRequestMessage &request_message) {
  std::string command = request.command();
  if (command == "ext-scr" && request.arguments_size() > 0)
    command = request.arguments(0);
  else if (command.empty() && target_mode == NSCAPI::target_module && request.arguments_size() > 0)
    command = request.arguments(0);
  else if (command.empty() && target_mode == NSCAPI::target_module)
    command = "help";
  try {
    if (command == "help") {
      nscapi::protobuf::functions::set_response_bad(*response, "Usage: nscp py [add|execute|list|install|delete] --help");
      return true;
    } else if (command == "execute" || command == "lua-script" || command == "lua-run") {
      execute_script(request, response);
      return true;
    }

    sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
    // ${scripts}, not ${base-path}. The two are the same folder apart on
    // Windows, where ${scripts} is ${exe-path}/scripts - so appending
    // "scripts/lua" to the install base happened to land in the right place
    // there, and nowhere near it on Linux, where ${base-path} is the directory
    // holding the binary (/usr/sbin) while ${scripts} is under the package
    // directory. Naming the token that already means "the scripts folder"
    // removes the assumption instead of re-deriving it.
    auto provider = std::make_shared<script_provider>(get_id(), get_core(), get_core()->expand_path("${scripts}"));

    extscr_cli client(provider);
    if (client.run(command, request, response)) {
      return true;
    }
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Error: " + utf8::utf8_from_native(e.what()));
  } catch (...) {
    nscapi::protobuf::functions::set_response_bad(*response, "Error: ");
  }

  return false;
}

void LUAScript::execute_script(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response) {
  po::options_description desc = nscapi::program_options::create_desc(request);
  po::variables_map vm;
  nscapi::program_options::unrecognized_map script_options;
  std::string file;
  // clang-format off
  desc.add_options()
    ("help", "Show help.")
    ("script", po::value<std::string>(&file), "The script to run")
    ("file", po::value<std::string>(&file), "The script to run")
    ;
  // clang-format on

  try {
    nscapi::program_options::basic_command_line_parser cmd(request);
    cmd.options(desc);

    po::parsed_options parsed = cmd.allow_unregistered().run();
    po::store(parsed, vm);
    po::notify(vm);
    script_options = po::collect_unrecognized(parsed.options, po::include_positional);
    if (!script_options.empty() &&
        (script_options[0] == "execute" || script_options[0] == "exec" || script_options[0] == "lua-script" || script_options[0] == "lua-run"))
      script_options.erase(script_options.begin());

  } catch (const std::exception &e) {
    return nscapi::program_options::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), *response);
  }

  if (vm.count("help")) {
    nscapi::protobuf::functions::set_response_good(*response, nscapi::program_options::help(desc));
    return;
  }

  boost::optional<boost::filesystem::path> ofile = lua::lua_script::find_script(root_, file);
  if (!ofile) {
    nscapi::protobuf::functions::set_response_bad(*response, "Script not found: " + file);
    return;
  }
  const std::shared_ptr<scripts::script_manager<lua::lua_traits> > scripts = std::atomic_load(&scripts_);
  if (!scripts) {
    nscapi::protobuf::functions::set_response_bad(*response, "Module is not loaded");
    return;
  }
  scripts::script_information<lua::lua_traits> *info = scripts->add("", ofile.value().string());
  lua_runtime_->load(info);
  std::vector<std::string> opts(script_options.begin(), script_options.end());
  lua_runtime_->exec_main(info, opts, response);
}

void LUAScript::handleNotification(const std::string &channel, const PB::Commands::QueryResponseMessage::Response &request,
                                   PB::Commands::SubmitResponseMessage::Response *response, const PB::Commands::SubmitRequestMessage &request_message) {
  const std::shared_ptr<scripts::script_manager<lua::lua_traits> > scripts = std::atomic_load(&scripts_);
  if (!scripts) return;
  const scripts::script_manager<lua::lua_traits>::dispatch_guard dispatch(*scripts);
  if (!dispatch.entered()) return;
  boost::optional<scripts::command_definition<lua::lua_traits> > cmd = scripts->find_command(scripts::nscp::tags::submit_tag, channel);
  if (cmd) {
    lua_runtime_->on_submit(channel, cmd.value().information, cmd.value().function, false, request, response);
    return;
  }
  cmd = scripts->find_command(scripts::nscp::tags::simple_submit_tag, channel);
  if (cmd) {
    lua_runtime_->on_submit(channel, cmd.value().information, cmd.value().function, true, request, response);
  }
}