// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/json.hpp>
#include <memory>
#include <string>
#include <vector>

#include "ncpa_check.hpp"

// The node half of the NCPA bridge: the tree `/api/<path>` walks, how a path
// resolves to a node, and how a node renders itself as JSON or runs as a check.
//
// It is a transcription of the node model in NCPA's agent/listener/nodes.py.
// Nothing here dispatches an NSClient++ query or reads a metrics snapshot: a
// tree is built from plain values by ncpa_sources, and the one node that has to
// call into the agent (`plugins`) does it through the abstract dispatcher
// below. That keeps the whole file unit-testable without a core.
namespace ncpa {

class node;
typedef std::shared_ptr<node> node_ptr;

// The `-q key=value` extras check_ncpa.py appends to the query string, which
// are how the `services` and `processes` nodes are filtered. A key may repeat
// (`-q "service=sshd,service=cron"`), so this is a list rather than a map.
struct filter_args {
  std::vector<std::pair<std::string, std::string> > values;

  void add(const std::string &key, const std::string &value);
  std::vector<std::string> all(const std::string &key) const;
  std::string first(const std::string &key, const std::string &fallback = "") const;
  bool has(const std::string &key) const;
};

// Everything a walk or a check needs beyond the tree itself.
struct walk_context {
  request_options opts;
  filter_args extras;
  // Where `delta` keeps its previous samples. May be null, in which case a
  // delta request reports zero.
  delta_store *deltas = nullptr;
  // The API path being served, which (with the peer address) keys the delta
  // samples, exactly as NCPA keys its pickle files.
  std::string accessor;
  std::string remote_addr;
};

// Split an `/api/<accessor>` path into its segments, dropping empty ones.
// Quoted segments are kept whole, as NCPA's getter() does, so a mount point or
// a plugin argument containing a slash inside quotes survives.
std::vector<std::string> split_accessor(const std::string &accessor);

// A mount point as NCPA names it in the tree: every run of slashes and
// backslashes becomes a single "|", so "/" is "|" and "C:\" is "C:|".
std::string encode_mountpoint(const std::string &mountpoint);

class node {
 public:
  explicit node(std::string name) : name_(std::move(name)) {}
  virtual ~node() = default;

  const std::string &name() const { return name_; }

  // The JSON under this node's own key. `{ "<name>": <this> }` is what the API
  // answers, which walk() below composes.
  virtual boost::json::value walk_body(const walk_context &ctx) const = 0;
  boost::json::object walk(const walk_context &ctx) const;

  // Run this node as a check. `ropts` carries the parent-child rendering flags;
  // a top-level check passes the defaults.
  virtual check_result run_check(const walk_context &ctx, const render_options &ropts) const;

  // The named child, or a null pointer. A leaf never has one.
  virtual node_ptr find(const std::string &child) const;

  // Whether this node's body IS the whole answer, rather than the value of a
  // `{"<name>": ...}` pair. True only for a plugin: its document is the
  // `{"returncode", "stdout"}` pair check_ncpa.py reads at the top level, and
  // wrapping it in the node's name would hide it from the plugin.
  virtual bool answers_unwrapped() const { return false; }

 private:
  std::string name_;
};

// A leaf: one or more values and a unit.
class leaf_node : public node {
 public:
  leaf_node(std::string name, values_type vals, std::string unit);

  // True when the JSON renders the single value on its own rather than as a
  // one-element array. NCPA's leaves differ here - `memory/virtual/total`
  // answers `[8589934592, "B"]` while `cpu/percent` answers `[[1.0, 2.0], "%"]`
  // - because its methods return whichever Python type was handy, and the XI
  // wizard reads the result. Defaults to true for a single value.
  leaf_node &set_scalar(bool scalar);
  // Marks a value that is already a per-second rate (everything the NSClient++
  // 1 Hz collector publishes). `delta` on such a node appends "/s" to the unit
  // and leaves the number alone instead of differencing two rates.
  leaf_node &set_rate(bool rate);

  boost::json::value walk_body(const walk_context &ctx) const override;
  check_result run_check(const walk_context &ctx, const render_options &ropts) const override;

  // The values after `unit`, `units`, `delta` and `aggregate` have been
  // applied - what both the walk and the check see. Exposed because a
  // RunnableParentNode reads its `total` child this way to decide whether its
  // children's perfdata carries thresholds.
  void resolve(const walk_context &ctx, values_type &out, std::string &unit_out) const;

 private:
  values_type values_;
  std::string unit_;
  bool scalar_ = true;
  bool rate_ = false;
};

// A node with children and no check of its own: walking it walks everything
// below, and checking it is UNKNOWN.
class parent_node : public node {
 public:
  explicit parent_node(std::string name) : node(std::move(name)) {}

  // Children keep insertion order, which is the order they appear in the JSON
  // and the order a RunnableParentNode renders them in.
  parent_node &add(const node_ptr &child);
  const std::vector<node_ptr> &children() const { return children_; }

  boost::json::value walk_body(const walk_context &ctx) const override;
  node_ptr find(const std::string &child) const override;

 private:
  std::vector<node_ptr> children_;
};

// A parent that is also checkable: one child is the primary value the check
// alerts on, and the others are folded into the line as "(Total: 8.00 GiB,
// ...)". memory/virtual, memory/swap, a disk mount point and an interface are
// all of this shape, and they are what `check_ncpa -M memory/virtual` hits.
class runnable_parent_node : public parent_node {
 public:
  runnable_parent_node(std::string name, std::string primary, std::string primary_unit);

  runnable_parent_node &set_custom_output(std::string custom_output);
  // Restrict which children take part in the check. Empty means all of them.
  // (They all still appear in a walk.)
  runnable_parent_node &set_include(std::vector<std::string> include);
  // Whether the primary child's own perfdata joins the line. NCPA sets this on
  // memory/virtual only; on every other node of this shape a percentage
  // primary's perfdata - the one carrying the thresholds - is dropped. See
  // https://github.com/NagiosEnterprises/ncpa/issues/783.
  runnable_parent_node &set_add_primary_to_perfdata(bool add);

  check_result run_check(const walk_context &ctx, const render_options &ropts) const override;

 private:
  std::string primary_;
  std::string primary_unit_;
  std::string custom_output_;
  std::vector<std::string> include_;
  bool add_primary_to_perfdata_ = false;
};

// The node a path resolves to when it names something that is not there. It is
// not an error in the HTTP sense: check_ncpa.py turns a non-2xx status into
// "UNKNOWN: An error occurred connecting to API", which tells an operator
// nothing, so NCPA answers 200 with an error body and this node renders it.
class missing_node : public node {
 public:
  missing_node(std::string failed_name, std::string node_type, std::string full_path);

  boost::json::value walk_body(const walk_context &ctx) const override;
  check_result run_check(const walk_context &ctx, const render_options &ropts) const override;

 private:
  std::string failed_name_;
  std::string node_type_;
  std::string full_path_;
};

// One service as the agent reports it. NCPA's services node is a flat
// name -> "running" | "stopped" map, so that is all a bridge needs.
struct service_entry {
  std::string name;
  // "running" or "stopped". Anything the platform cannot classify is reported
  // stopped, which is what makes a check on it alert rather than pass.
  std::string status;
};

// One process, with the four fields NCPA's process filters match on.
struct process_entry {
  // The executable's name, which is what NCPA calls a process's `name`.
  std::string name;
  // The executable with its path.
  std::string exe;
  std::string username;
  std::string cmd;
  long long pid = 0;
};

// How the `plugins` node reaches the agent. A registered query, an alias or an
// external script all look the same from here.
struct query_dispatcher {
  virtual ~query_dispatcher() = default;
  // Run `name` with `args` (already REST-style `key=value` tokens) and return
  // its Nagios return code and its "message|perfdata" text verbatim.
  virtual check_result run_query(const std::string &name, const std::vector<std::string> &args) = 0;
  // The queries `plugins/` exposes, for the bare `/api/plugins` listing.
  virtual std::vector<std::string> list_queries() = 0;
  // Whether `name` is exposed at all, per the `plugins` allow-list.
  virtual bool is_exposed(const std::string &name) = 0;
  // Whether path arguments are accepted (`allow arguments`).
  virtual bool allow_arguments() = 0;
  // Every service the agent can see, unfiltered: the NCPA filters are applied
  // here rather than by the agent, because their semantics (`match=search`,
  // `combiner=or`) are NCPA's and not the agent's.
  virtual std::vector<service_entry> list_services() = 0;
  virtual std::vector<process_entry> list_processes() = 0;
};

// How an NCPA filter compares a value: exact (the default), a case-insensitive
// substring (`match=search`) or a regular expression (`match=regex`).
bool ncpa_matches(const std::string &pattern, const std::string &candidate, const std::string &match_mode);

// `services`: a flat name -> status map, filtered by `service`, `status` and
// `match`. In check mode every service is compared with its expected state.
class services_node : public node {
 public:
  services_node(std::string name, query_dispatcher *dispatcher);

  boost::json::value walk_body(const walk_context &ctx) const override;
  check_result run_check(const walk_context &ctx, const render_options &ropts) const override;

 private:
  std::vector<service_entry> filtered(const walk_context &ctx, bool ignore_status) const;
  query_dispatcher *dispatcher_;
};

// `processes`: the matching processes as a list, filtered by `name`, `exe`,
// `username`, `cmd`, `match` and `combiner`. In check mode the value checked is
// the number of matches.
class processes_node : public node {
 public:
  processes_node(std::string name, query_dispatcher *dispatcher);

  boost::json::value walk_body(const walk_context &ctx) const override;
  check_result run_check(const walk_context &ctx, const render_options &ropts) const override;

 private:
  std::vector<process_entry> filtered(const walk_context &ctx) const;
  // "Process count for processes named nginx", the title NCPA puts in the
  // output line so the alert says what it was counting.
  static std::string label(const walk_context &ctx);
  query_dispatcher *dispatcher_;
};

// `plugins/<name>/<arg>/<arg>`: every registered check and external script,
// with its own output passed through unchanged. This is the node that makes
// every NSClient++ check reachable from Nagios without any mapping work.
class plugins_node : public node {
 public:
  plugins_node(std::string name, query_dispatcher *dispatcher);

  boost::json::value walk_body(const walk_context &ctx) const override;
  node_ptr find(const std::string &child) const override;

 private:
  query_dispatcher *dispatcher_;
};

// One named plugin plus the path segments that followed it.
class plugin_node : public node {
 public:
  plugin_node(std::string name, query_dispatcher *dispatcher);

  boost::json::value walk_body(const walk_context &ctx) const override;
  check_result run_check(const walk_context &ctx, const render_options &ropts) const override;
  node_ptr find(const std::string &child) const override;
  bool answers_unwrapped() const override { return true; }

  const std::vector<std::string> &arguments() const { return arguments_; }

 private:
  query_dispatcher *dispatcher_;
  std::vector<std::string> arguments_;
};

// Walk `path` down from `root`. Never returns null: a segment that names no
// child resolves to a missing_node, which renders NCPA's error body.
node_ptr resolve(const node_ptr &root, const std::vector<std::string> &path, const std::string &full_path);

}  // namespace ncpa
