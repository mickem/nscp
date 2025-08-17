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

#pragma once

extern "C" {
#include <lua.h>
}

#include <boost/shared_ptr.hpp>
#include <list>
#include <lua/lua_script.hpp>
#include <nscapi/nscapi_protobuf_command.hpp>
#include <scripts/script_interface.hpp>
#include <string>

namespace lua {
typedef scripts::script_information<lua_traits> script_information;

struct lua_runtime_plugin {
  virtual void load(lua::lua_wrapper &instance) = 0;
  virtual void unload(lua::lua_wrapper &instance) = 0;
};
typedef boost::shared_ptr<lua_runtime_plugin> lua_runtime_plugin_type;

struct lua_runtime : public scripts::script_runtime_interface<lua::lua_traits> {
  std::string base_path;
  std::list<lua_runtime_plugin_type> plugins;

  lua_runtime(std::string base_path) : base_path(base_path) {}

  virtual void register_query(const std::string &command, const std::string &description);
  virtual void register_subscription(const std::string &channel, const std::string &description);

  virtual void on_query(std::string command, script_information *information, lua::lua_traits::function_type function, bool simple,
                        const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                        const PB::Commands::QueryRequestMessage &request_message);
  virtual void on_exec(std::string command, script_information *information, lua::lua_traits::function_type function, bool simple,
                       const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response,
                       const PB::Commands::ExecuteRequestMessage &request_message);
  virtual void on_submit(std::string channel, script_information *information, lua::lua_traits::function_type function, bool simple,
                         const PB::Commands::QueryResponseMessage::Response &request, PB::Commands::SubmitResponseMessage::Response *response);
  virtual void exec_main(script_information *information, const std::vector<std::string> &opts, PB::Commands::ExecuteResponseMessage::Response *response);

  virtual void load(scripts::script_information<lua_traits> *info);
  virtual void start(scripts::script_information<lua_traits> *info);
  virtual void unload(scripts::script_information<lua_traits> *info);

  void register_plugin(lua_runtime_plugin_type plugin) { plugins.push_back(plugin); }

  static lua_State *prep_function(const lua::script_information *information, const lua::lua_traits::function_type &c) {
    lua_State *L = information->user_data.L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, c.function_ref);
    if (c.object_ref != 0) lua_rawgeti(L, LUA_REGISTRYINDEX, c.object_ref);
    return L;
  }
  static lua_State *prep_function(const lua::script_information *information, const std::string &f) {
    lua_State *L = information->user_data.L;
    lua_getglobal(L, f.c_str());
    return L;
  }
  void create_user_data(script_information *info);
};
}  // namespace lua