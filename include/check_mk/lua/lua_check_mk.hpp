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

#include <scripts/functions.hpp>
#include <scripts/script_interface.hpp>
#include <scripts/script_nscp.hpp>

#include <lua/lua_script.hpp>
#include <lua/lua_core.hpp>

#include <check_mk/data.hpp>

struct MKData {
    const static std::string tag;
};
struct MKPaketData {
  const static std::string tag;
  check_mk::packet packet;
};
struct MKSectionData {
   const static std::string tag;
   check_mk::packet::section section;
};
struct MKLineData {
   const static std::string tag;
   check_mk::packet::section::line line;
};


namespace check_mk {
namespace  check_mk_lua_wrapper {
  int client_callback(lua_State *L);
  int server_callback(lua_State *L);
  int create(lua_State *L);
  MKData* wrap(lua_State *L);
  int destroy(lua_State* L);
};

namespace check_mk_packet_wrapper {
  int size_section(lua_State *L);
  int get_section(lua_State *L);
  int add_section(lua_State *L);
  int create(lua_State *L);
  MKPaketData* wrap(lua_State *L);
  int destroy(lua_State* L);
};

namespace check_mk_section_wrapper {
  int get_title(lua_State *L);
  int set_title(lua_State *L);
  int size_line(lua_State *L);
  int get_line(lua_State *L);
  int add_line(lua_State *L);
  int create(lua_State *L);
  MKSectionData* wrap(lua_State *L);
  int destroy(lua_State* L);
};
namespace check_mk_line_wrapper {
  int get_line(lua_State *L);
  int set_line(lua_State *L);
  int size_item(lua_State *L);
  int get_item(lua_State *L);
  int add_item(lua_State *L);
  int create(lua_State *L);
  MKLineData* wrap(lua_State *L);
  int destroy(lua_State* L);
};

struct check_mk_plugin : public lua::lua_runtime_plugin {
  void load(lua::lua_wrapper &instance);
  void unload(lua::lua_wrapper &instance);
};
}  // namespace check_mk