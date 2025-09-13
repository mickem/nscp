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

#include "LUAScript.h"

#include <boost/program_options.hpp>
#include <boost/smart_ptr/make_shared_object.hpp>
#include <file_helpers.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_settings_helper.hpp>

#include "extscr_cli.h"

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

bool LUAScript::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
  try {
    root_ = get_base_path();
    nscp_runtime_.reset(new scripts::nscp::nscp_runtime_impl(get_id(), get_core()));
    lua_runtime_.reset(new lua::lua_runtime(utf8::cvt<std::string>(root_.string())));
    scripts_.reset(new scripts::script_manager<lua::lua_traits>(lua_runtime_, nscp_runtime_, get_id(), utf8::cvt<std::string>(alias)));

    sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
    settings.set_alias(alias, "lua");

    // clang-format off
    settings.alias().add_path_to_settings()

      ("scripts", sh::fun_values_path([this] (auto key, auto value) { this->loadScript(key, value); }),
	      "Lua scripts", "A list of scripts available to run from the LuaScript module.",
	      "SCRIPT DEFINITION", "For more configuration options add a dedicated section")
      ;
    // clang-format on

    settings.register_all();
    settings.notify();

    // 		if (!scriptDirectory_.empty()) {
    // 			addAllScriptsFrom(scriptDirectory_);
    // 		}

    scripts_->load_all();
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("load", e);
    return false;
  } catch (...) {
    NSC_LOG_ERROR_EX("load");
    return false;
  }

  return true;
}

bool LUAScript::startModule() {
  try {
    scripts_->start_all();
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
    if (!ofile) return false;
    NSC_DEBUG_MSG_STD("Adding script: " + ofile->string());
    scripts_->add(alias, ofile->string());
    return true;
  } catch (...) {
    NSC_LOG_ERROR_EX("load script");
  }
  return false;
}

bool LUAScript::unloadModule() {
  if (scripts_) {
    scripts_->unload_all();
    scripts_.reset();
  }
  return true;
}

void LUAScript::query_fallback(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                               const PB::Commands::QueryRequestMessage &request_message) {
  boost::optional<scripts::command_definition<lua::lua_traits> > cmd = scripts_->find_command(scripts::nscp::tags::query_tag, request.command());
  if (!cmd) {
    cmd = scripts_->find_command(scripts::nscp::tags::simple_query_tag, request.command());
    if (!cmd) return nscapi::protobuf::functions::set_response_bad(*response, "Failed to find command: " + request.command());
    return lua_runtime_->on_query(request.command(), cmd->information, cmd->function, true, request, response, request_message);
  }
  return lua_runtime_->on_query(request.command(), cmd->information, cmd->function, false, request, response, request_message);
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
      boost::optional<scripts::command_definition<lua::lua_traits> > cmd = scripts_->find_command(scripts::nscp::tags::simple_exec_tag, request.command());
      if (cmd) {
        lua_runtime_->on_exec(request.command(), cmd->information, cmd->function, true, request, response, request_message);
      }
      return true;
    }

    sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
    auto provider = boost::make_shared<script_provider>(get_id(), get_core(), get_core()->expand_path("${base-path}"));

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

void LUAScript::handleNotification(const std::string &channel, const PB::Commands::QueryResponseMessage::Response &request,
                                   PB::Commands::SubmitResponseMessage::Response *response, const PB::Commands::SubmitRequestMessage &request_message) {
  boost::optional<scripts::command_definition<lua::lua_traits> > cmd = scripts_->find_command(scripts::nscp::tags::submit_tag, channel);
  if (cmd) {
    lua_runtime_->on_submit(channel, cmd->information, cmd->function, false, request, response);
    return;
  }
  cmd = scripts_->find_command(scripts::nscp::tags::simple_submit_tag, channel);
  if (cmd) {
    lua_runtime_->on_submit(channel, cmd->information, cmd->function, true, request, response);
  }
}