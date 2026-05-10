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

#include <memory>
#include <iostream>
#include <net/check_mk/lua/lua_check_mk.hpp>

//////////////////////////////////////////////////////////////////////////
const std::string MKData::tag = "mk";
int check_mk::check_mk_lua_wrapper::client_callback(lua_State *L) {
  lua::lua_traits::function fundata;
  lua::lua_wrapper lua_instance(L);
  lua_instance.get_user_object_instance<MKData>();
  int count = lua_instance.size();
  if (count < 1) return lua_instance.error("Invalid syntax: client(<function>);");
  std::string funname;
  if (lua_instance.pop_string(funname)) {
    lua_instance.getglobal(funname);
  }
  if (!lua_instance.pop_function_ref(fundata.function_ref)) return lua_instance.error("Invalid function");
  lua::lua_traits::get_info(lua_instance)->register_command("check_mk", "c_callback", "", fundata);
  return 0;
}
int check_mk::check_mk_lua_wrapper::server_callback(lua_State *L) {
  lua::lua_traits::function fundata;
  lua::lua_wrapper lua_instance(L);
  lua_instance.get_user_object_instance<MKData>();
  int count = lua_instance.size();
  if (count < 1) return lua_instance.error("Invalid syntax: server(<function>);");
  std::string funname;
  if (lua_instance.pop_string(funname)) {
    lua_instance.getglobal(funname);
  }
  if (!lua_instance.pop_function_ref(fundata.function_ref)) return lua_instance.error("Invalid function");
  lua::lua_traits::get_info(lua_instance)->register_command("check_mk", "s_callback", "", fundata);
  return 0;
}
const luaL_Reg mk_functions[] = {{"client_callback", &check_mk::check_mk_lua_wrapper::client_callback},
                                 {"client", &check_mk::check_mk_lua_wrapper::client_callback},
                                 {"server_callback", &check_mk::check_mk_lua_wrapper::server_callback},
                                 {"server", &check_mk::check_mk_lua_wrapper::server_callback},
                                 {"__gc", &check_mk::check_mk_lua_wrapper::destroy},
                                 {0}};

const luaL_Reg mk_ctors[] = {{"new", &check_mk::check_mk_lua_wrapper::create}, {0}};

int check_mk::check_mk_lua_wrapper::create(lua_State *L) {
  lua::lua_wrapper instance(L);
  instance.push_user_object_instance<MKData>();
  return 1;
}
MKData *check_mk::check_mk_lua_wrapper::wrap(lua_State *L) {
  lua::lua_wrapper instance(L);
  return instance.push_user_object_instance<MKData>();
}
int check_mk::check_mk_lua_wrapper::destroy(lua_State *L) {
  lua::lua_wrapper instance(L);
  return instance.destroy_user_object_instance<MKData>();
}

//////////////////////////////////////////////////////////////////////////
const std::string MKPaketData::tag = "packet";
int check_mk::check_mk_packet_wrapper::get_section(lua_State *L) {
  lua::lua_wrapper lua_instance(L);
  auto data = lua_instance.get_user_object_instance<MKPaketData>();
  if (data == NULL) {
    return lua_instance.error("Failed to get packet");
  }
  if (lua_instance.size() < 1) return lua_instance.error("Invalid syntax: get_section(id)");
  int id = lua_instance.pop_int() - 1;
  try {
    check_mk::packet::section s = data->packet.get_section(id);
    auto *obj = check_mk::check_mk_section_wrapper::wrap(lua_instance);
    obj->section = s;
    return 1;
  } catch (const std::exception &e) {
    return lua_instance.error(std::string("Failed to get section: ") + e.what());
  }
}
int check_mk::check_mk_packet_wrapper::add_section(lua_State *L) {
  lua::lua_wrapper lua_instance(L);
  auto data = lua_instance.get_user_object_instance<MKPaketData>(1);
  if (data == NULL) {
    return lua_instance.error("Failed to get packet");
  }
  if (lua_instance.size() < 1) return lua_instance.error("Invalid syntax: add_section(title)");
  try {
    auto section_data = lua_instance.get_user_object_instance<MKSectionData>(2);
    if (section_data == NULL) {
      return lua_instance.error("Failed to add section");
    }
    data->packet.add_section(section_data->section);
    return 0;
  } catch (const std::exception &e) {
    return lua_instance.error(std::string("Failed to add section: ") + e.what());
  }
}
int check_mk::check_mk_packet_wrapper::size_section(lua_State *L) {
  lua::lua_wrapper lua_instance(L);
  auto data = lua_instance.get_user_object_instance<MKPaketData>();
  if (data == NULL) {
    return lua_instance.error("Failed to get packet");
  }
  lua_instance.push_int(static_cast<int>(data->packet.section_list.size()));
  return 1;
}
int check_mk::check_mk_packet_wrapper::add_piggyback(lua_State *L) {
  lua::lua_wrapper lua_instance(L);
  auto data = lua_instance.get_user_object_instance<MKPaketData>(1);
  if (data == NULL) return lua_instance.error("Failed to get packet");
  if (lua_instance.size() < 3) return lua_instance.error("Invalid syntax: add_piggyback(host, section)");
  try {
    auto section_data = lua_instance.get_user_object_instance<MKSectionData>(3);
    if (section_data == NULL) return lua_instance.error("Failed to get section");
    const std::string host = lua_instance.get_string(2);
    if (host.empty()) return lua_instance.error("piggyback host must be non-empty");
    data->packet.piggyback_for(host).add_section(section_data->section);
    return 0;
  } catch (const std::exception &e) {
    return lua_instance.error(std::string("Failed to add piggyback: ") + e.what());
  }
}

const luaL_Reg packet_functions[] = {{"get_section", &check_mk::check_mk_packet_wrapper::get_section},
                                     {"add_section", &check_mk::check_mk_packet_wrapper::add_section},
                                     {"add_piggyback", &check_mk::check_mk_packet_wrapper::add_piggyback},
                                     {"size_section", &check_mk::check_mk_packet_wrapper::size_section},
                                     {"__gc", &check_mk::check_mk_packet_wrapper::destroy},
                                     {0}};
const luaL_Reg packet_ctors[] = {{"new", &check_mk::check_mk_packet_wrapper::create}, {0}};
int check_mk::check_mk_packet_wrapper::create(lua_State *L) {
  lua::lua_wrapper instance(L);
  instance.push_user_object_instance<MKPaketData>();
  return 1;
}
MKPaketData *check_mk::check_mk_packet_wrapper::wrap(lua_State *L) {
  lua::lua_wrapper instance(L);
  return instance.push_user_object_instance<MKPaketData>();
}
int check_mk::check_mk_packet_wrapper::destroy(lua_State *L) {
  lua::lua_wrapper instance(L);
  return instance.destroy_user_object_instance<MKData>();
}

//////////////////////////////////////////////////////////////////////////
const std::string MKSectionData::tag = "section";
int check_mk::check_mk_section_wrapper::get_line(lua_State *L) {
  lua::lua_wrapper lua_instance(L);
  auto data = lua_instance.get_user_object_instance<MKSectionData>();
  if (data == NULL) {
    return lua_instance.error("Failed to get line");
  }
  if (lua_instance.size() < 1) return lua_instance.error("Invalid syntax: get_line(id)");
  int id = lua_instance.pop_int() - 1;
  try {
    check_mk::packet::section::line l = data->section.get_line(id);
    auto *obj = check_mk::check_mk_line_wrapper::wrap(lua_instance);
    obj->line = l;
    return 1;
  } catch (const std::exception &e) {
    return lua_instance.error(std::string("Failed to get line: ") + e.what());
  }
}
int check_mk::check_mk_section_wrapper::add_line(lua_State *L) {
  lua::lua_wrapper lua_instance(L);
  auto data = lua_instance.get_user_object_instance<MKSectionData>();
  if (data == NULL) {
    return lua_instance.error("Failed to add line");
  }
  if (lua_instance.size() < 1) return lua_instance.error("Invalid syntax: add_line(s)");
  if (lua_instance.is_string()) {
    try {
      std::string l = lua_instance.pop_string();
      data->section.add_line(check_mk::packet::section::line(l));
      return 0;
    } catch (const std::exception &e) {
      return lua_instance.error(std::string("Failed to add line: ") + e.what());
    }
  } else {
    auto line_data = lua_instance.get_user_object_instance<MKLineData>(2);
    if (line_data == NULL) {
      return lua_instance.error("Failed to add line");
    }
    try {
      data->section.add_line(line_data->line);
      return 0;
    } catch (const std::exception &e) {
      return lua_instance.error(std::string("Failed to get section: ") + e.what());
    }
  }
}
int check_mk::check_mk_section_wrapper::get_title(lua_State *L) {
  lua::lua_wrapper lua_instance(L);
  auto data = lua_instance.get_user_object_instance<MKSectionData>();
  if (data == NULL) {
    return lua_instance.error("Failed to get section");
  }
  lua_instance.push_string(data->section.title);
  return 1;
}
int check_mk::check_mk_section_wrapper::set_title(lua_State *L) {
  lua::lua_wrapper lua_instance(L);
  auto data = lua_instance.get_user_object_instance<MKSectionData>();
  if (data == NULL) {
    return lua_instance.error("Failed to get section");
  }
  std::string title;
  if (!lua_instance.pop_string(title)) {
    return lua_instance.error("Invalid syntax: set_title(title)");
  }
  data->section.title = title;
  return 0;
}
int check_mk::check_mk_section_wrapper::size_line(lua_State *L) {
  lua::lua_wrapper lua_instance(L);
  auto data = lua_instance.get_user_object_instance<MKSectionData>();
  if (data == NULL) {
    return lua_instance.error("Failed to get section");
  }
  lua_instance.push_int(static_cast<int>(data->section.lines.size()));
  return 1;
}
int check_mk::check_mk_section_wrapper::set_separator(lua_State *L) {
  lua::lua_wrapper lua_instance(L);
  auto data = lua_instance.get_user_object_instance<MKSectionData>();
  if (data == NULL) return lua_instance.error("Failed to get section");
  if (lua_instance.size() < 2) return lua_instance.error("Invalid syntax: set_separator(ascii_code)");
  data->section.separator = lua_instance.pop_int();
  return 0;
}
int check_mk::check_mk_section_wrapper::set_cached(lua_State *L) {
  lua::lua_wrapper lua_instance(L);
  auto data = lua_instance.get_user_object_instance<MKSectionData>();
  if (data == NULL) return lua_instance.error("Failed to get section");
  if (lua_instance.size() < 3) return lua_instance.error("Invalid syntax: set_cached(generated_epoch, interval)");
  const int interval = lua_instance.pop_int();
  const int generated = lua_instance.pop_int();
  data->section.cached = std::make_pair(static_cast<long long>(generated), static_cast<long long>(interval));
  return 0;
}
int check_mk::check_mk_section_wrapper::set_persist(lua_State *L) {
  lua::lua_wrapper lua_instance(L);
  auto data = lua_instance.get_user_object_instance<MKSectionData>();
  if (data == NULL) return lua_instance.error("Failed to get section");
  if (lua_instance.size() < 2) return lua_instance.error("Invalid syntax: set_persist(epoch)");
  data->section.persist_until = static_cast<long long>(lua_instance.pop_int());
  return 0;
}
const luaL_Reg section_functions[] = {{"get_line", &check_mk::check_mk_section_wrapper::get_line},
                                      {"add_line", &check_mk::check_mk_section_wrapper::add_line},
                                      {"get_title", &check_mk::check_mk_section_wrapper::get_title},
                                      {"set_title", &check_mk::check_mk_section_wrapper::set_title},
                                      {"set_separator", &check_mk::check_mk_section_wrapper::set_separator},
                                      {"set_cached", &check_mk::check_mk_section_wrapper::set_cached},
                                      {"set_persist", &check_mk::check_mk_section_wrapper::set_persist},
                                      {"size_line", &check_mk::check_mk_section_wrapper::size_line},
                                      {"__gc", &check_mk::check_mk_section_wrapper::destroy},
                                      {0}};
const luaL_Reg section_ctors[] = {{"new", &check_mk::check_mk_section_wrapper::create}, {0}};
int check_mk::check_mk_section_wrapper::create(lua_State *L) {
  lua::lua_wrapper instance(L);
  instance.push_user_object_instance<MKSectionData>();
  return 1;
}
MKSectionData *check_mk::check_mk_section_wrapper::wrap(lua_State *L) {
  lua::lua_wrapper instance(L);
  return instance.push_user_object_instance<MKSectionData>();
}
int check_mk::check_mk_section_wrapper::destroy(lua_State *L) {
  lua::lua_wrapper instance(L);
  return instance.destroy_user_object_instance<MKData>();
}

//////////////////////////////////////////////////////////////////////////
const std::string MKLineData::tag = "line";
int check_mk::check_mk_line_wrapper::get_item(lua_State *L) {
  lua::lua_wrapper lua_instance(L);
  auto data = lua_instance.get_user_object_instance<MKLineData>();
  if (data == NULL) {
    return lua_instance.error("Failed to get line");
  }
  if (lua_instance.size() < 1) return lua_instance.error("Invalid syntax: get_line(id)");
  int id = lua_instance.pop_int() - 1;
  try {
    std::string item = data->line.get_item(id);
    lua_instance.push_string(item);
    return 1;
  } catch (const std::exception &e) {
    return lua_instance.error(std::string("Failed to get item: ") + e.what());
  }
}
int check_mk::check_mk_line_wrapper::get_line(lua_State *L) {
  lua::lua_wrapper lua_instance(L);
  auto data = lua_instance.get_user_object_instance<MKLineData>();
  if (data == NULL) {
    return lua_instance.error("Failed to get line");
  }
  lua_instance.push_string(data->line.get_line());
  return 1;
}
int check_mk::check_mk_line_wrapper::set_line(lua_State *L) {
  lua::lua_wrapper lua_instance(L);
  auto data = lua_instance.get_user_object_instance<MKLineData>();
  if (data == NULL) {
    return lua_instance.error("Failed to get line");
  }
  std::string l;
  if (!lua_instance.pop_string(l)) {
    return lua_instance.error("Invalid syntax: set_line(line)");
  }
  data->line.set_line(l);
  return 0;
}
int check_mk::check_mk_line_wrapper::add_item(lua_State *L) {
  lua::lua_wrapper lua_instance(L);
  auto data = lua_instance.get_user_object_instance<MKLineData>();
  if (data == NULL) {
    return lua_instance.error("Failed to get line");
  }
  std::string l;
  if (!lua_instance.pop_string(l)) {
    return lua_instance.error("Invalid syntax: add_item(line)");
  }
  data->line.items.push_back(l);
  return 0;
}
int check_mk::check_mk_line_wrapper::size_item(lua_State *L) {
  lua::lua_wrapper lua_instance(L);
  auto data = lua_instance.get_user_object_instance<MKLineData>();
  if (data == NULL) {
    return lua_instance.error("Failed to get line");
  }
  lua_instance.push_int(static_cast<int>(data->line.items.size()));
  return 1;
}
const luaL_Reg line_functions[] = {{"add_item", &check_mk::check_mk_line_wrapper::add_item},
                                   {"get_item", &check_mk::check_mk_line_wrapper::get_item},
                                   {"get_line", &check_mk::check_mk_line_wrapper::get_line},
                                   {"set_line", &check_mk::check_mk_line_wrapper::set_line},
                                   {"size_item", &check_mk::check_mk_line_wrapper::size_item},
                                   {"__gc", &check_mk::check_mk_line_wrapper::destroy},
                                   {0}};
const luaL_Reg line_ctors[] = {{"new", &check_mk::check_mk_line_wrapper::create}, {0}};
int check_mk::check_mk_line_wrapper::create(lua_State *L) {
  lua::lua_wrapper instance(L);
  instance.push_user_object_instance<MKLineData>();
  return 1;
}
MKLineData *check_mk::check_mk_line_wrapper::wrap(lua_State *L) {
  lua::lua_wrapper instance(L);
  return instance.push_user_object_instance<MKLineData>();
}
int check_mk::check_mk_line_wrapper::destroy(lua_State *L) {
  lua::lua_wrapper instance(L);
  return instance.destroy_user_object_instance<MKData>();
}

//////////////////////////////////////////////////////////////////////////
const std::string MKMetricsData::tag = "metrics";

metrics::metrics_store &check_mk::shared_metrics_store() {
  static metrics::metrics_store store;
  return store;
}

int check_mk::check_mk_metrics_wrapper::get(lua_State *L) {
  lua::lua_wrapper lua_instance(L);
  lua_instance.get_user_object_instance<MKMetricsData>();
  std::string filter;
  if (lua_instance.size() >= 1 && lua_instance.is_string()) {
    filter = lua_instance.pop_string();
  }
  const auto values = check_mk::shared_metrics_store().get(filter);
  lua_createtable(L, 0, static_cast<int>(values.size()));
  for (const auto &kv : values) {
    lua_pushlstring(L, kv.first.c_str(), kv.first.size());
    lua_pushlstring(L, kv.second.c_str(), kv.second.size());
    lua_settable(L, -3);
  }
  return 1;
}
int check_mk::check_mk_metrics_wrapper::value(lua_State *L) {
  lua::lua_wrapper lua_instance(L);
  lua_instance.get_user_object_instance<MKMetricsData>();
  // Stack on entry: [m, key, default?]; size includes the userdata at pos 1.
  if (lua_instance.size() < 2) return lua_instance.error("Invalid syntax: value(key, [default])");
  std::string fallback;
  bool has_default = false;
  if (lua_instance.size() >= 3 && lua_instance.is_string()) {
    fallback = lua_instance.pop_string();
    has_default = true;
  }
  const std::string key = lua_instance.pop_string();
  // get(filter) does substring matching; we want exact-match here.
  for (const auto &kv : check_mk::shared_metrics_store().get(key)) {
    if (kv.first == key) {
      lua_instance.push_string(kv.second);
      return 1;
    }
  }
  if (has_default) {
    lua_instance.push_string(fallback);
    return 1;
  }
  lua_pushnil(L);
  return 1;
}
int check_mk::check_mk_metrics_wrapper::create(lua_State *L) {
  lua::lua_wrapper instance(L);
  instance.push_user_object_instance<MKMetricsData>();
  return 1;
}
int check_mk::check_mk_metrics_wrapper::destroy(lua_State *L) {
  lua::lua_wrapper instance(L);
  return instance.destroy_user_object_instance<MKMetricsData>();
}
const luaL_Reg metrics_functions[] = {{"get", &check_mk::check_mk_metrics_wrapper::get},
                                      {"value", &check_mk::check_mk_metrics_wrapper::value},
                                      {"__gc", &check_mk::check_mk_metrics_wrapper::destroy},
                                      {0}};
const luaL_Reg metrics_ctors[] = {{"new", &check_mk::check_mk_metrics_wrapper::create}, {0}};

//////////////////////////////////////////////////////////////////////////
const std::string MKSubmissionsData::tag = "submissions";

void check_mk::submission_store::put(kind k, const std::string &name, cached_result r) {
  const boost::lock_guard<boost::mutex> lock(mutex_);
  if (k == kind_mrpe)
    mrpe_[name] = std::move(r);
  else
    local_[name] = std::move(r);
}
std::vector<std::pair<std::string, check_mk::cached_result> > check_mk::submission_store::snapshot(kind k) const {
  const boost::lock_guard<boost::mutex> lock(mutex_);
  const std::map<std::string, cached_result> &src = (k == kind_mrpe) ? mrpe_ : local_;
  std::vector<std::pair<std::string, cached_result> > out;
  out.reserve(src.size());
  for (const auto &kv : src) out.push_back(kv);
  return out;
}

check_mk::submission_store &check_mk::shared_submission_store() {
  static check_mk::submission_store store;
  return store;
}

static void push_cached_entry(lua_State *L, const std::string &name, const check_mk::cached_result &r) {
  lua_createtable(L, 0, 6);
  lua_pushstring(L, "name");
  lua_pushlstring(L, name.c_str(), name.size());
  lua_settable(L, -3);
  lua_pushstring(L, "code");
  lua_pushinteger(L, r.code);
  lua_settable(L, -3);
  lua_pushstring(L, "message");
  lua_pushlstring(L, r.message.c_str(), r.message.size());
  lua_settable(L, -3);
  lua_pushstring(L, "perf");
  lua_pushlstring(L, r.perf.c_str(), r.perf.size());
  lua_settable(L, -3);
  lua_pushstring(L, "generated");
  lua_pushnumber(L, static_cast<lua_Number>(r.generated));
  lua_settable(L, -3);
  lua_pushstring(L, "ttl");
  lua_pushnumber(L, static_cast<lua_Number>(r.ttl));
  lua_settable(L, -3);
}

int check_mk::check_mk_submissions_wrapper::get(lua_State *L) {
  lua::lua_wrapper lua_instance(L);
  lua_instance.get_user_object_instance<MKSubmissionsData>();
  if (lua_instance.size() < 2) return lua_instance.error("Invalid syntax: get(channel)");
  const std::string channel = lua_instance.pop_string();
  submission_store::kind k;
  if (channel == "mrpe")
    k = submission_store::kind_mrpe;
  else if (channel == "local")
    k = submission_store::kind_local;
  else
    return lua_instance.error("Unknown channel: " + channel + " (expected 'mrpe' or 'local')");
  const auto entries = check_mk::shared_submission_store().snapshot(k);
  lua_createtable(L, static_cast<int>(entries.size()), 0);
  int idx = 1;
  for (const auto &kv : entries) {
    lua_pushinteger(L, idx++);
    push_cached_entry(L, kv.first, kv.second);
    lua_settable(L, -3);
  }
  return 1;
}
int check_mk::check_mk_submissions_wrapper::create(lua_State *L) {
  lua::lua_wrapper instance(L);
  instance.push_user_object_instance<MKSubmissionsData>();
  return 1;
}
int check_mk::check_mk_submissions_wrapper::destroy(lua_State *L) {
  lua::lua_wrapper instance(L);
  return instance.destroy_user_object_instance<MKSubmissionsData>();
}
const luaL_Reg submissions_functions[] = {{"get", &check_mk::check_mk_submissions_wrapper::get},
                                          {"__gc", &check_mk::check_mk_submissions_wrapper::destroy},
                                          {0}};
const luaL_Reg submissions_ctors[] = {{"new", &check_mk::check_mk_submissions_wrapper::create}, {0}};

//////////////////////////////////////////////////////////////////////////
void check_mk::check_mk_plugin::load(lua::lua_wrapper &lua_instance) {
  lua_instance.setup_class(MKData::tag, mk_ctors, mk_functions);
  lua_instance.setup_class(MKPaketData::tag, packet_ctors, packet_functions);
  lua_instance.setup_class(MKSectionData::tag, section_ctors, section_functions);
  lua_instance.setup_class(MKLineData::tag, line_ctors, line_functions);
  lua_instance.setup_class(MKMetricsData::tag, metrics_ctors, metrics_functions);
  lua_instance.setup_class(MKSubmissionsData::tag, submissions_ctors, submissions_functions);
  // `Metrics()` / `Submissions()` mirror `Core()` / `Settings()` for ergonomic use.
  lua_instance.setup_global_function("Metrics", &check_mk::check_mk_metrics_wrapper::create);
  lua_instance.setup_global_function("Submissions", &check_mk::check_mk_submissions_wrapper::create);
}
void check_mk::check_mk_plugin::unload(lua::lua_wrapper &) {}