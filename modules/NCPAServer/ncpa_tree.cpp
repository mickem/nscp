// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "ncpa_tree.hpp"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/regex.hpp>

namespace json = boost::json;

namespace ncpa {

namespace {

// Whether `aggregate` names one of the four collapsing modes. An aggregated
// leaf always renders as a list, even though it is down to one value, because
// NCPA's get_aggregated_values hands back `[max(values)]`.
bool aggregates(const std::string &mode) { return mode == "max" || mode == "min" || mode == "sum" || mode == "avg"; }

json::value to_json(const value &v) {
  if (v.is_string) return json::value(v.text);
  if (v.is_integer) return json::value(static_cast<std::int64_t>(v.number));
  return json::value(v.number);
}

json::value values_to_json(const values_type &vals, const bool as_array) {
  if (!as_array && vals.size() == 1) return to_json(vals[0]);
  json::array out;
  out.reserve(vals.size());
  for (const value &v : vals) out.push_back(to_json(v));
  return out;
}

}  // namespace

std::vector<std::string> split_accessor(const std::string &accessor) {
  std::vector<std::string> out;
  std::string current;
  char quote = 0;
  for (const char c : accessor) {
    if (quote != 0) {
      current += c;
      if (c == quote) quote = 0;
      continue;
    }
    if (c == '"' || c == '\'') {
      quote = c;
      current += c;
      continue;
    }
    if (c == '/') {
      if (!current.empty()) out.push_back(current);
      current.clear();
      continue;
    }
    current += c;
  }
  if (!current.empty()) out.push_back(current);
  return out;
}

std::string encode_mountpoint(const std::string &mountpoint) {
  std::string out;
  bool in_run = false;
  for (const char c : mountpoint) {
    if (c == '/' || c == '\\') {
      if (!in_run) out += '|';
      in_run = true;
      continue;
    }
    in_run = false;
    out += c;
  }
  return out;
}

json::object node::walk(const walk_context &ctx) const {
  json::object out;
  out[name()] = walk_body(ctx);
  return out;
}

check_result node::run_check(const walk_context & /*ctx*/, const render_options & /*ropts*/) const {
  check_result result;
  result.returncode = 3;
  result.stdout_text = "UNKNOWN: Unable to run check on node without check method. Requested '" + name() + "' node.";
  return result;
}

node_ptr node::find(const std::string & /*child*/) const { return {}; }

leaf_node::leaf_node(std::string name, values_type vals, std::string unit) : node(std::move(name)), values_(std::move(vals)), unit_(std::move(unit)) {}

leaf_node &leaf_node::set_scalar(const bool scalar) {
  scalar_ = scalar;
  return *this;
}

leaf_node &leaf_node::set_rate(const bool rate) {
  rate_ = rate;
  return *this;
}

void leaf_node::resolve(const walk_context &ctx, values_type &out, std::string &unit_out) const {
  out = values_;
  // An explicit `unit` replaces the node's own, which also takes it out of the
  // set adjust_scale will rescale - so `-U` disables `-u`, as in NCPA.
  unit_out = ctx.opts.unit.empty() ? unit_ : ctx.opts.unit;

  adjust_scale(out, ctx.opts.units, unit_out);

  if (ctx.opts.delta) {
    unit_out += "/s";
    // A value the 1 Hz collector already publishes as a per-second rate needs
    // no differencing: two consecutive rates differenced would report ~0, which
    // is exactly the silent wrong answer `delta` is meant to avoid. The unit
    // still gains the "/s" NCPA appends, because the value really is a rate.
    if (!rate_) {
      if (ctx.deltas != nullptr) {
        ctx.deltas->deltaize(ctx.accessor + "." + name() + ctx.remote_addr, out);
      } else {
        for (value &v : out) v = value::from_int(0);
      }
    }
  }

  aggregate_values(out, ctx.opts.aggregate);
}

json::value leaf_node::walk_body(const walk_context &ctx) const {
  values_type vals;
  std::string unit;
  resolve(ctx, vals, unit);

  const bool as_array = !scalar_ || vals.size() != 1 || aggregates(ctx.opts.aggregate);
  const json::value rendered = values_to_json(vals, as_array);
  if (unit.empty()) return rendered;

  json::array out;
  out.push_back(rendered);
  out.push_back(json::value(unit));
  return out;
}

check_result leaf_node::run_check(const walk_context &ctx, const render_options &ropts) const {
  values_type vals;
  std::string unit;
  resolve(ctx, vals, unit);

  render_options r = ropts;
  if (r.node_name.empty()) r.node_name = name();
  return render_check(vals, unit, ctx.opts, r);
}

parent_node &parent_node::add(const node_ptr &child) {
  if (child) children_.push_back(child);
  return *this;
}

json::value parent_node::walk_body(const walk_context &ctx) const {
  json::object out;
  for (const node_ptr &child : children_) {
    // One unreadable child must not take the whole subtree down with it: NCPA
    // substitutes an error string for that key and keeps walking, and a
    // dashboard polling /api depends on the rest still arriving.
    try {
      out[child->name()] = child->walk_body(ctx);
    } catch (const std::exception &e) {
      out[child->name()] = json::value(std::string("Error retrieving child: ") + e.what());
    }
  }
  return out;
}

node_ptr parent_node::find(const std::string &child) const {
  for (const node_ptr &c : children_) {
    if (c->name() == child) return c;
  }
  return {};
}

runnable_parent_node::runnable_parent_node(std::string name, std::string primary, std::string primary_unit)
    : parent_node(std::move(name)), primary_(std::move(primary)), primary_unit_(std::move(primary_unit)) {}

runnable_parent_node &runnable_parent_node::set_custom_output(std::string custom_output) {
  custom_output_ = std::move(custom_output);
  return *this;
}

runnable_parent_node &runnable_parent_node::set_include(std::vector<std::string> include) {
  include_ = std::move(include);
  return *this;
}

runnable_parent_node &runnable_parent_node::set_add_primary_to_perfdata(const bool add) {
  add_primary_to_perfdata_ = add;
  return *this;
}

check_result runnable_parent_node::run_check(const walk_context &ctx, const render_options &ropts) const {
  // A percentage primary describes a share of `total`, so the siblings' own
  // perfdata must not repeat the thresholds - a non-zero total is the flag
  // that suppresses them.
  double total = 0;
  if (primary_unit_ == "%") {
    if (const node_ptr total_node = find("total")) {
      if (const auto *leaf = dynamic_cast<const leaf_node *>(total_node.get())) {
        values_type vals;
        std::string unit;
        leaf->resolve(ctx, vals, unit);
        if (!vals.empty() && !vals[0].is_string) total = vals[0].number;
      }
    }
  }

  check_result primary_info;
  bool found_primary = false;
  std::vector<std::string> secondary_results;
  std::vector<std::string> secondary_perfdata;

  for (const node_ptr &child : children()) {
    if (!include_.empty() && std::find(include_.begin(), include_.end(), child->name()) == include_.end()) continue;

    if (child->name() == primary_) {
      render_options r;
      r.use_prefix = true;
      r.use_perfdata = false;
      r.primary = true;
      r.secondary_data = false;
      r.custom_output = custom_output_;
      r.node_name = child->name();
      primary_info = child->run_check(ctx, r);
      found_primary = true;
      if (add_primary_to_perfdata_ && !primary_info.perfdata.empty()) secondary_perfdata.push_back(primary_info.perfdata);
      continue;
    }

    render_options r;
    r.use_prefix = false;
    r.use_perfdata = false;
    r.primary = false;
    r.primary_total = total;
    r.secondary_data = true;
    r.node_name = child->name();
    const check_result res = child->run_check(ctx, r);
    if (!res.stdout_text.empty()) secondary_results.push_back(res.stdout_text);
    if (!res.perfdata.empty()) secondary_perfdata.push_back(res.perfdata);
  }

  if (!found_primary) {
    check_result result;
    result.returncode = 3;
    result.stdout_text = "UNKNOWN: Unable to run check on node without check method. Requested '" + name() + "' node.";
    return result;
  }

  const std::string secondary_stdout = "(" + boost::algorithm::join(secondary_results, ", ") + ")";
  boost::replace_all(primary_info.stdout_text, "{extra_data}", secondary_stdout);

  if (!secondary_perfdata.empty()) {
    std::string extra;
    // With a percentage primary the primary's perfdata only reaches the line
    // when the node opted in above - which NCPA does for memory/virtual and
    // nowhere else. The result is that a swap or disk check's thresholds do
    // not appear in its perfdata; reproduced deliberately, because a graph
    // built against the real agent expects exactly these labels.
    if (primary_unit_ != "%") extra += primary_info.perfdata + " ";
    extra += boost::algorithm::join(secondary_perfdata, " ");
    primary_info.stdout_text += " | " + extra;
  }
  primary_info.perfdata.clear();
  return primary_info;
}

missing_node::missing_node(std::string failed_name, std::string node_type, std::string full_path)
    : node("error"), failed_name_(std::move(failed_name)), node_type_(std::move(node_type)), full_path_(std::move(full_path)) {}

json::value missing_node::walk_body(const walk_context & /*ctx*/) const {
  json::object error;
  error["path"] = full_path_;
  error["code"] = 100;
  error["message"] = "The " + node_type_ + " requested does not exist.";
  error[node_type_] = failed_name_;
  return error;
}

check_result missing_node::run_check(const walk_context & /*ctx*/, const render_options & /*ropts*/) const {
  check_result result;
  result.returncode = 3;
  std::string message = "UNKNOWN: The " + node_type_ + " (" + failed_name_ + ") requested does not exist.";
  // A pipe in the message would be read as the start of perfdata by every
  // Nagios frontend, and a missing mount point's name is full of them.
  boost::replace_all(message, "|", "/");
  result.stdout_text = message;
  return result;
}

plugins_node::plugins_node(std::string name, query_dispatcher *dispatcher) : node(std::move(name)), dispatcher_(dispatcher) {}

json::value plugins_node::walk_body(const walk_context & /*ctx*/) const {
  json::array out;
  if (dispatcher_ != nullptr) {
    for (const std::string &name : dispatcher_->list_queries()) out.push_back(json::value(name));
  }
  return out;
}

node_ptr plugins_node::find(const std::string &child) const {
  if (dispatcher_ == nullptr || !dispatcher_->is_exposed(child)) return {};
  return std::make_shared<plugin_node>(child, dispatcher_);
}

plugin_node::plugin_node(std::string name, query_dispatcher *dispatcher) : node(std::move(name)), dispatcher_(dispatcher) {}

json::value plugin_node::walk_body(const walk_context &ctx) const {
  // NCPA runs a plugin the same way whether or not `check=1` was sent: there
  // is nothing to "list" about a plugin, so a walk answers the check result.
  const check_result result = run_check(ctx, render_options());
  json::object out;
  out["returncode"] = result.returncode;
  out["stdout"] = result.stdout_text;
  return out;
}

check_result plugin_node::run_check(const walk_context & /*ctx*/, const render_options & /*ropts*/) const {
  if (dispatcher_ == nullptr) {
    check_result result;
    result.returncode = 3;
    result.stdout_text = "UNKNOWN: No plugin dispatcher configured.";
    return result;
  }
  if (!arguments_.empty() && !dispatcher_->allow_arguments()) {
    check_result result;
    result.returncode = 3;
    result.stdout_text = "UNKNOWN: Arguments are not allowed. Set 'allow arguments = true' under /settings/NCPA/server to enable them.";
    return result;
  }
  return dispatcher_->run_query(name(), arguments_);
}

node_ptr plugin_node::find(const std::string &child) const {
  // Every path segment after the plugin name is one of its arguments, so the
  // node grows a copy of itself rather than resolving a child.
  const auto next = std::make_shared<plugin_node>(name(), dispatcher_);
  next->arguments_ = arguments_;
  next->arguments_.push_back(child);
  return next;
}

void filter_args::add(const std::string &key, const std::string &value) { values.emplace_back(key, value); }

std::vector<std::string> filter_args::all(const std::string &key) const {
  std::vector<std::string> out;
  for (const auto &entry : values) {
    if (entry.first == key && !entry.second.empty()) out.push_back(entry.second);
  }
  return out;
}

std::string filter_args::first(const std::string &key, const std::string &fallback) const {
  for (const auto &entry : values) {
    if (entry.first == key) return entry.second;
  }
  return fallback;
}

bool filter_args::has(const std::string &key) const {
  for (const auto &entry : values) {
    if (entry.first == key) return true;
  }
  return false;
}

bool ncpa_matches(const std::string &pattern, const std::string &candidate, const std::string &match_mode) {
  if (match_mode == "search") return boost::icontains(candidate, pattern);
  if (match_mode == "regex") {
    try {
      // re.search, not re.match: an unanchored pattern matches anywhere.
      return boost::regex_search(candidate, boost::regex(pattern));
    } catch (const std::exception &) {
      // A pattern that does not compile matches nothing, rather than taking
      // the whole check down with an exception.
      return false;
    }
  }
  return boost::iequals(candidate, pattern);
}

services_node::services_node(std::string name, query_dispatcher *dispatcher) : node(std::move(name)), dispatcher_(dispatcher) {}

std::vector<service_entry> services_node::filtered(const walk_context &ctx, const bool ignore_status) const {
  std::vector<service_entry> all;
  if (dispatcher_ != nullptr) all = dispatcher_->list_services();

  const std::vector<std::string> wanted = ctx.extras.all("service");
  const std::vector<std::string> statuses = ignore_status ? std::vector<std::string>() : ctx.extras.all("status");
  if (wanted.empty() && statuses.empty()) return all;

  // NCPA unions the two filters rather than intersecting them: a request
  // naming both a service and a status answers everything matching either.
  std::vector<service_entry> accepted;
  const std::string match_mode = ctx.extras.first("match");
  for (const service_entry &entry : all) {
    bool keep = false;
    for (const std::string &name : wanted) {
      if (ncpa_matches(name, entry.name, match_mode)) keep = true;
    }
    for (const std::string &status : statuses) {
      if (entry.status == status) keep = true;
    }
    if (keep) accepted.push_back(entry);
  }
  return accepted;
}

json::value services_node::walk_body(const walk_context &ctx) const {
  json::object out;
  for (const service_entry &entry : filtered(ctx, false)) out[entry.name] = entry.status;
  return out;
}

check_result services_node::run_check(const walk_context &ctx, const render_options & /*ropts*/) const {
  check_result result;

  // `status` is the EXPECTED state in check mode, not a filter - so the listing
  // must not be narrowed by it, or a stopped service would be filtered out of
  // the very check meant to catch it.
  std::vector<std::string> expected = ctx.extras.all("status");
  if (expected.empty()) expected.emplace_back("running");

  const std::string match_mode = ctx.extras.first("match");
  std::vector<std::string> requested = ctx.extras.all("service");
  // Only an exact-match request can report "could not be found": a substring or
  // a regular expression names no particular service.
  if (match_mode == "search" || match_mode == "regex") requested.clear();

  const std::vector<service_entry> services = filtered(ctx, true);
  if (services.empty()) {
    result.returncode = 3;
    result.stdout_text = "UNKNOWN: No services found for service names: " + boost::algorithm::join(ctx.extras.all("service"), ", ");
    return result;
  }

  // Services whose state is wrong are reported first, so a truncated line still
  // shows the problem.
  std::vector<std::string> problems;
  std::vector<std::string> fine;
  for (const service_entry &entry : services) {
    const bool ok = std::find(expected.begin(), expected.end(), entry.status) != expected.end();
    std::string line = entry.name + " is " + entry.status;
    if (ok) {
      fine.push_back(line);
    } else {
      line += " (should be " + boost::algorithm::join(expected, "") + ")";
      problems.push_back(line);
    }
    const auto found = std::find(requested.begin(), requested.end(), entry.name);
    if (found != requested.end()) requested.erase(found);
  }

  int returncode = problems.empty() ? 0 : 2;
  std::vector<std::string> lines = problems;
  lines.insert(lines.end(), fine.begin(), fine.end());
  // A name that matched nothing is UNKNOWN rather than CRITICAL: the agent
  // cannot say whether a service it has never heard of is down or misspelled.
  for (const std::string &missing : requested) {
    lines.push_back(missing + " could not be found");
    returncode = 3;
  }

  const char *prefix = returncode == 0 ? "OK" : (returncode == 2 ? "CRITICAL" : "UNKNOWN");
  result.returncode = returncode;
  result.stdout_text = std::string(prefix) + ": " + boost::algorithm::join(lines, ", ");
  return result;
}

processes_node::processes_node(std::string name, query_dispatcher *dispatcher) : node(std::move(name)), dispatcher_(dispatcher) {}

std::vector<process_entry> processes_node::filtered(const walk_context &ctx) const {
  std::vector<process_entry> all;
  if (dispatcher_ != nullptr) all = dispatcher_->list_processes();

  const std::vector<std::string> names = ctx.extras.all("name");
  const std::vector<std::string> exes = ctx.extras.all("exe");
  const std::vector<std::string> usernames = ctx.extras.all("username");
  const std::vector<std::string> cmds = ctx.extras.all("cmd");
  if (names.empty() && exes.empty() && usernames.empty() && cmds.empty()) return all;

  const std::string match_mode = ctx.extras.first("match");
  // `and` (the default) requires every term to match; `or` requires one.
  const bool require_all = !boost::iequals(ctx.extras.first("combiner", "and"), "or");

  std::vector<process_entry> accepted;
  for (const process_entry &entry : all) {
    bool all_matched = true;
    bool any_matched = false;
    const auto compare = [&](const std::vector<std::string> &patterns, const std::string &candidate) {
      for (const std::string &pattern : patterns) {
        if (ncpa_matches(pattern, candidate, match_mode)) {
          any_matched = true;
        } else {
          all_matched = false;
        }
      }
    };
    compare(names, entry.name);
    compare(exes, entry.exe);
    compare(usernames, entry.username);
    compare(cmds, entry.cmd);
    if (require_all ? all_matched : any_matched) accepted.push_back(entry);
  }
  return accepted;
}

json::value processes_node::walk_body(const walk_context &ctx) const {
  json::array out;
  for (const process_entry &entry : filtered(ctx)) {
    json::object process;
    // Every field is a one-element list, the shape NCPA's standard_form()
    // produces and the XI wizard reads.
    process["name"] = json::array{entry.name};
    process["exe"] = json::array{entry.exe};
    process["username"] = json::array{entry.username};
    process["cmd"] = json::array{entry.cmd};
    process["pid"] = json::array{entry.pid};
    out.push_back(process);
  }
  return out;
}

std::string processes_node::label(const walk_context &ctx) {
  const std::vector<std::string> exes = ctx.extras.all("exe");
  const std::vector<std::string> names = ctx.extras.all("name");
  const std::string combiner = boost::iequals(ctx.extras.first("combiner", "and"), "or") ? "or" : "and";

  std::string title = "Process count";
  if (exes.empty() && names.empty()) return title;
  title += " for";
  if (!exes.empty()) {
    title += " exes named " + boost::algorithm::join(exes, ",");
    if (!names.empty()) title += " " + combiner;
  }
  if (!names.empty()) title += " processes named " + boost::algorithm::join(names, ",");
  return title;
}

check_result processes_node::run_check(const walk_context &ctx, const render_options &ropts) const {
  const std::vector<process_entry> matches = filtered(ctx);

  // NCPA checks the NUMBER of matching processes, with a fixed perfdata label
  // so a graph keeps the same series whatever the filter names.
  walk_context check_ctx = ctx;
  if (check_ctx.opts.title.empty()) check_ctx.opts.title = label(ctx);
  if (check_ctx.opts.perfdata_label.empty()) check_ctx.opts.perfdata_label = "process_count";
  // The count is not a byte value and not a rate, so `units` and `delta` have
  // nothing to do here - and applying them would rescale a count.
  check_ctx.opts.units.clear();
  check_ctx.opts.delta = false;

  render_options r = ropts;
  r.node_name = name();
  // NCPA passes the title through verbatim here rather than capitalizing it,
  // because it has already built it as a sentence.
  r.capitalize_title = false;

  check_result result = render_check(values_type{value::from_int(static_cast<long long>(matches.size()))}, "", check_ctx.opts, r);

  if (matches.empty()) return result;

  // The matched processes as long output, below the summary line - the same
  // table NCPA appends, minus the per-process CPU and memory shares, which the
  // NSClient++ collector does not publish per process.
  std::string extra = "\nProcesses Matched\nPID: Name: Username: Exe\n-----------------------------------\n";
  for (const process_entry &entry : matches) {
    extra += std::to_string(entry.pid) + ": " + entry.name + ": " + entry.username + ": " + entry.exe + "\n";
  }
  // Perfdata has to stay last: every Nagios frontend reads the text as
  // "MESSAGE|PERFDATA" and would swallow the table into the graph otherwise.
  const std::size_t pipe = result.stdout_text.rfind('|');
  if (pipe == std::string::npos) {
    result.stdout_text += extra;
  } else {
    result.stdout_text = result.stdout_text.substr(0, pipe) + extra + "|" + result.stdout_text.substr(pipe + 1);
  }
  return result;
}

node_ptr resolve(const node_ptr &root, const std::vector<std::string> &path, const std::string &full_path) {
  node_ptr current = root;
  for (const std::string &segment : path) {
    const node_ptr next = current->find(segment);
    if (!next) {
      // "plugin" rather than "node" when the miss was under `plugins/`, which
      // is the distinction NCPA's error body makes.
      const std::string type = std::dynamic_pointer_cast<plugins_node>(current) ? "plugin" : "node";
      return std::make_shared<missing_node>(segment, type, full_path);
    }
    current = next;
  }
  return current;
}

}  // namespace ncpa
