// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <list>
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
    // holds however that module is loaded.
    std::string module;
  };

  ncpa_sources(const nscapi::core_wrapper *core, unsigned int plugin_id) : core_(core), plugin_id_(plugin_id) {}

  // Every registered query and query alias. `with_module` resolves each
  // query's module name, which costs a module-registry lookup; leave it off
  // when the caller does not look at query_info::module.
  std::vector<query_info> list_queries(bool with_module) const;
  // The registration of one query; false when nothing is registered under
  // `name`.
  bool describe_query(const std::string &name, bool with_module, query_info &out) const;

  // Run a query with each argument as one token, the way a REST caller passes
  // `key=value`. The caller identity (this module) is stamped on the request,
  // so the core permission layer sees an NCPAServer subject. Returns the
  // Nagios return code.
  int run_query(const std::string &name, const std::list<std::string> &arguments, std::string &message, std::string &perf) const;

 private:
  const nscapi::core_wrapper *core_;
  unsigned int plugin_id_;
};
