// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <chrono>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <string>
#include <vector>

// The adapters between the NCPA node model and the rest of the agent. Phase 1
// only needs the command registry and query dispatch, for the `plugins/` node;
// the metrics snapshot and the internal queries behind the built-in tree join
// it later.
class ncpa_sources {
 public:
  struct query_info {
    std::string name;
    // The module that registered the query: its name (CheckExternalScripts),
    // never the alias it was loaded under, so a policy that names a module
    // holds however that module is loaded. Empty unless the sources were
    // built `with_module`.
    std::string module;
  };

  // `with_module` resolves each query's module name, which costs a
  // module-registry lookup per refresh; leave it off when nothing looks at
  // query_info::module (only `plugins = scripts` does).
  ncpa_sources(const nscapi::core_wrapper *core, unsigned int plugin_id, bool with_module) : core_(core), plugin_id_(plugin_id), with_module_(with_module) {}

  // Every registered query and query alias, sorted by name.
  std::vector<query_info> list_queries() const;
  // The registration of one query; false when nothing is registered under
  // `name` (compared as the registry does, without regard to case).
  bool describe_query(const std::string &name, query_info &out) const;

  // Run a query with each argument as one token, the way a REST caller passes
  // `key=value`. The caller identity (this module) is stamped on the request,
  // so the core permission layer sees an NCPAServer subject. Returns the
  // Nagios return code.
  int run_query(const std::string &name, const std::list<std::string> &arguments, std::string &message, std::string &perf) const;

 private:
  typedef std::map<std::string, query_info> inventory_map;
  // The registry's query list, kept for kRefreshSeconds. Every poll used to
  // ask the registry whether its query exists (and, for `plugins = scripts`,
  // rebuild the module list) before running it. Registrations do change at
  // runtime - a script module registers commands, a module is loaded over
  // REST - so this is a short-lived cache, not one kept until a reload.
  static constexpr int kRefreshSeconds = 5;
  std::shared_ptr<const inventory_map> inventory() const;

  const nscapi::core_wrapper *core_;
  unsigned int plugin_id_;
  bool with_module_;
  mutable std::mutex cache_mutex_;
  mutable std::shared_ptr<const inventory_map> cache_;
  mutable std::chrono::steady_clock::time_point cache_time_;
};
