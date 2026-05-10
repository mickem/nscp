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

#include <boost/thread/mutex.hpp>
#include <lua/lua_core.hpp>
#include <lua/lua_script.hpp>
#include <map>
#include <metrics/metrics_store_map.hpp>
#include <net/check_mk/data.hpp>
#include <utility>
#include <vector>

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
struct MKMetricsData {
  const static std::string tag;
};
struct MKSubmissionsData {
  const static std::string tag;
};

namespace check_mk {

// Returns the process-wide metrics store the CheckMKServer module fills via
// submitMetrics. Lua scripts read it through the Metrics() global.
metrics::metrics_store &shared_metrics_store();

// In-memory cache for passive check results submitted to the
// `check_mk-mrpe` / `check_mk-local` channels. Each entry is keyed by
// service name; the latest submission wins.
struct cached_result {
  long long generated;  // unix epoch when this result was submitted
  long long ttl;        // seconds the value is considered fresh
  int code;             // 0=OK, 1=WARN, 2=CRIT, 3=UNKNOWN
  std::string message;  // first line of the check message
  std::string perf;     // performance data (may be empty)
};

class submission_store {
 public:
  enum kind { kind_mrpe, kind_local };
  void put(kind k, const std::string &name, cached_result r);
  std::vector<std::pair<std::string, cached_result> > snapshot(kind k) const;

 private:
  mutable boost::mutex mutex_;
  std::map<std::string, cached_result> mrpe_;
  std::map<std::string, cached_result> local_;
};

submission_store &shared_submission_store();

namespace check_mk_lua_wrapper {
int client_callback(lua_State *L);
int server_callback(lua_State *L);
int create(lua_State *L);
MKData *wrap(lua_State *L);
int destroy(lua_State *L);
};  // namespace check_mk_lua_wrapper

namespace check_mk_packet_wrapper {
int size_section(lua_State *L);
int get_section(lua_State *L);
int add_section(lua_State *L);
// packet:add_piggyback(host, section) - relays `section` to `host` in a
// `<<<<host>>>>` block on the wire.
int add_piggyback(lua_State *L);
int create(lua_State *L);
MKPaketData *wrap(lua_State *L);
int destroy(lua_State *L);
};  // namespace check_mk_packet_wrapper

namespace check_mk_section_wrapper {
int get_title(lua_State *L);
int set_title(lua_State *L);
// Header decorations rendered as `<<<title:sep(N):cached(g,i):persist(p)>>>`.
// set_separator(N)         - ASCII code (0=NUL, 9=tab, 124=pipe, ...).
// set_cached(gen, interval) - generated_epoch + cache lifetime in seconds.
// set_persist(epoch)        - keep this section even on later fetches that omit it.
int set_separator(lua_State *L);
int set_cached(lua_State *L);
int set_persist(lua_State *L);
int size_line(lua_State *L);
int get_line(lua_State *L);
int add_line(lua_State *L);
int create(lua_State *L);
MKSectionData *wrap(lua_State *L);
int destroy(lua_State *L);
};  // namespace check_mk_section_wrapper
namespace check_mk_line_wrapper {
int get_line(lua_State *L);
int set_line(lua_State *L);
int size_item(lua_State *L);
int get_item(lua_State *L);
int add_item(lua_State *L);
int create(lua_State *L);
MKLineData *wrap(lua_State *L);
int destroy(lua_State *L);
};  // namespace check_mk_line_wrapper

namespace check_mk_metrics_wrapper {
// metrics:get([filter]) returns a table { ["key"] = "value", ... } of every
// metric whose key contains `filter` (or all metrics when filter is empty).
int get(lua_State *L);
// metrics:value(key, [default]) returns the value of a single metric as a
// string, or the default (or nil) if the metric is not set.
int value(lua_State *L);
int create(lua_State *L);
int destroy(lua_State *L);
};  // namespace check_mk_metrics_wrapper

namespace check_mk_submissions_wrapper {
// submissions:get(channel) returns an array of tables for every cached
// submission on the given channel ("mrpe" or "local"). Each entry has keys
// `name`, `code`, `message`, `perf`, `generated`, `ttl`.
int get(lua_State *L);
int create(lua_State *L);
int destroy(lua_State *L);
};  // namespace check_mk_submissions_wrapper

struct check_mk_plugin : public lua::lua_runtime_plugin {
  void load(lua::lua_wrapper &instance);
  void unload(lua::lua_wrapper &instance);
};
}  // namespace check_mk