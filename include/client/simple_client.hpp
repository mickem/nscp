// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <metrics/metrics_store_map.hpp>
#include <nscapi/log_handler.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <string>
#include <vector>

namespace client {
// A name the interactive prompt knows about, with the one-line description it
// shows as a hint. Used for the built-in verbs, for registered queries and
// aliases, and for modules.
struct command_info {
  std::string name;
  // Argument placeholders shown in the help text, e.g. "<module>". Empty for
  // a verb that takes none, and for registry queries (whose arguments are far
  // too many to summarise on one line - that is what `desc` is for).
  std::string args;
  std::string description;
};

// A module, with the two states the prompt cares about. `loaded` is whether it
// is running right now; `enabled` is whether the configuration says to load it
// at startup. They differ: `load` is the transient one, `enable` the persisted
// one.
struct module_info {
  std::string name;
  std::string description;
  bool loaded = false;
  bool enabled = false;
};

// The verbs cli_client::handle_command() understands itself, as opposed to the
// query names it forwards to the registry. Single source for the `help` output,
// the prompt's completion candidates and its syntax highlighting - they used to
// drift, and `help` listed barely half of what was actually implemented.
const std::vector<command_info> &builtin_commands();

struct cli_handler : nscapi::log_handler {
  virtual void output_message(const std::string &msg) = 0;
  virtual int get_plugin_id() const = 0;
  virtual const nscapi::core_wrapper *get_core() const = 0;
};
class cli_client {
  typedef std::shared_ptr<cli_handler> cli_handler_ptr;
  cli_handler_ptr handler;
  metrics::metrics_store metrics_store;

 public:
  cli_client(const cli_handler_ptr &handler) : handler(handler) {}
  void handle_command(const std::string &command);
  void push_metrics(const PB::Metrics::MetricsMessage &response);

  // Registry lookups an interactive front-end needs for completion, hints and
  // highlighting. Each is a round trip to the core, so callers are expected to
  // cache: the prompt refreshes them on boot and after load/unload/reload.
  std::vector<command_info> list_queries() const;
  // Loaded modules only - a cheap walk of the core's plugin list.
  std::vector<module_info> list_modules() const;
  // Every module the agent could load, including those that are only sitting
  // in the module directory. **Expensive the first time**: the core answers it
  // by dlopen'ing every module in the directory to read its name and
  // description (see registry_query_handler::inventory_plugin_on_disk). It
  // caches the result for the life of the process, so later calls are cheap -
  // but the first one is not something to do on a whim.
  std::vector<module_info> list_all_modules() const;
  // The parameter names a query accepts, for `key=` completion. Empty when the
  // query is unknown or declares none.
  std::vector<std::string> list_parameters(const std::string &query) const;
};
typedef std::shared_ptr<cli_handler> cli_handler_ptr;
}  // namespace client
