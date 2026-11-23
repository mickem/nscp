// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_translate.hpp"

#include <cstdlib>

#include "icinga_agent_arguments.hpp"
#include "icinga_agent_json.hpp"
#include "icinga_agent_macros.hpp"
#include "icinga_agent_thresholds.hpp"

namespace json = boost::json;

namespace icinga_agent {

namespace {

// Reading the macros goes through json_util and macro_text(): the master
// sends a macro only when the service (or the command's own defaults) gave it
// a value, so "not there" and "empty" are the same thing.
using json_util::has_value;
using json_util::text_list;
using json_util::truthy_field;

plugin_threshold threshold_of(const json::object &macros, const char *name) {
  try {
    return parse_plugin_threshold(macro_text(macros, name));
  } catch (const threshold_error &e) {
    throw translation_error(std::string(name) + ": " + e.what());
  }
}

// A value that goes inside quotes in a filter expression.
std::string quoted(const std::string &value, const char *name) {
  if (value.find('\'') != std::string::npos) throw translation_error(std::string(name) + " cannot contain a quote");
  return "'" + value + "'";
}

// ---------------------------------------------------------------------------
// The two sides of a check, built the same way for every plugin that takes a
// threshold in the plugins' common format.
// ---------------------------------------------------------------------------
struct bound {
  bool inverted = false;      // the plugin alerts on the low side (free space)
  std::string suffix;         // unit of a value that is not a percentage
  bool keep_percent = false;  // the keyword compares against a percentage of its total
  std::string keyword;        // set: write an expression on this keyword, not -w / -c
  bool ignore_negation = false;
};

// One side. Without a threshold the plugin never alerts on that side, so the
// query's own default must not apply either.
void add_side(std::list<std::string> &arguments, const char *short_key, const char *long_key, const json::object &macros, const char *macro,
              const bound &b) {
  if (!has_value(macros, macro)) {
    arguments.push_back(std::string(short_key) + "=none");
    return;
  }
  plugin_threshold t = threshold_of(macros, macro);
  if (b.ignore_negation) t.negated = false;
  if (b.keyword.empty()) {
    arguments.push_back(std::string(short_key) + "=" + to_range(t, b.inverted, b.suffix, b.keep_percent));
  } else {
    arguments.push_back(std::string(long_key) + "=" + to_expression(t, b.inverted, b.keyword, b.suffix, b.keep_percent));
  }
}

void add_sides(std::list<std::string> &arguments, const json::object &macros, const char *warn, const char *crit, const bound &b) {
  add_side(arguments, "w", "warning", macros, warn, b);
  add_side(arguments, "c", "critical", macros, crit, b);
}

// B, kB, MB, GB, TB (any case) as the size suffix of a filter value. The
// plugins count in 1024s, as the filters do.
std::string size_suffix(const json::object &macros, const char *name, const char *fallback) {
  std::string unit = has_value(macros, name) ? macro_text(macros, name) : fallback;
  for (char &c : unit) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
  if (unit == "b") return "b";
  if (unit == "kb") return "k";
  if (unit == "mb") return "m";
  if (unit == "gb") return "g";
  if (unit == "tb") return "t";
  throw translation_error(std::string(name) + ": unknown unit '" + unit + "' (B, kB, MB, GB or TB)");
}

// Free space by default; with the plugin's -U the same thresholds are read
// against the space in use, the plain way round.
bound space_bound(const json::object &macros, const char *unit_macro, const char *unit_fallback, const char *used_macro) {
  bound b;
  b.suffix = size_suffix(macros, unit_macro, unit_fallback);
  b.keep_percent = true;
  if (truthy_field(macros, used_macro)) {
    b.keyword = "used";
  } else {
    b.inverted = true;
  }
  return b;
}

// ---------------------------------------------------------------------------
// The Windows plugins (itl/command-plugins-windows.conf).
// ---------------------------------------------------------------------------

// check_disk.exe: free space per volume. Without -u the plugin counts bytes;
// the ITL sets "mb".
translated_command disk_windows(const json::object &macros) {
  translated_command out;
  out.query = "check_drivesize";
  const std::vector<std::string> paths = text_list(macros, "disk_win_path");
  for (const std::string &path : paths) out.arguments.push_back("drive=" + path);
  // No path: every fixed and remote volume, which is what the plugin walks.
  if (paths.empty()) out.arguments.push_back("filter=type in ('fixed', 'remote')");
  for (const std::string &drive : text_list(macros, "disk_win_exclude")) out.arguments.push_back("exclude=" + drive);
  add_sides(out.arguments, macros, "disk_win_warn", "disk_win_crit", space_bound(macros, "disk_win_unit", "b", "disk_win_show_used"));
  return out;
}

// check_memory.exe: free physical memory. With -U the plugin forces the plain
// reading, whatever the threshold's `!` said.
translated_command memory_windows(const json::object &macros) {
  translated_command out;
  out.query = "check_memory";
  out.arguments.push_back("type=physical");
  bound b = space_bound(macros, "memory_win_unit", "mb", "memory_win_show_used");
  b.ignore_negation = !b.keyword.empty();
  add_sides(out.arguments, macros, "memory_win_warn", "memory_win_crit", b);
  return out;
}

// check_swap.exe: the page files added up.
translated_command swap_windows(const json::object &macros) {
  translated_command out;
  out.query = "check_pagefile";
  out.arguments.push_back("filter=name = 'total'");
  bound b = space_bound(macros, "swap_win_unit", "mb", "swap_win_show_used");
  b.ignore_negation = !b.keyword.empty();
  add_sides(out.arguments, macros, "swap_win_warn", "swap_win_crit", b);
  return out;
}

// check_load.exe: CPU load in percent. It reads the first of a comma
// separated list and ignores the rest.
translated_command load_windows(const json::object &macros) {
  translated_command out;
  out.query = "check_cpu";
  json::object first;
  for (const char *name : {"load_win_warn", "load_win_crit"}) {
    if (!has_value(macros, name)) continue;
    const std::string value = macro_text(macros, name);
    first[name] = value.substr(0, value.find(','));
  }
  add_sides(out.arguments, first, "load_win_warn", "load_win_crit", bound());
  return out;
}

// check_procs.exe: the number of processes, optionally of one user. Only the
// number: the query's own short form also alerts on a process that is not
// running, and by default lists every process it looked at.
translated_command procs_windows(const json::object &macros) {
  translated_command out;
  out.query = "check_process";
  out.arguments.push_back("process=*");
  if (has_value(macros, "procs_win_user")) {
    out.arguments.push_back("resolve-owner=true");
    out.arguments.push_back("filter=username = " + quoted(macro_text(macros, "procs_win_user"), "procs_win_user"));
  }
  bound b;
  b.keyword = "count";
  add_sides(out.arguments, macros, "procs_win_warn", "procs_win_crit", b);
  out.arguments.push_back("top-syntax=${status}: ${count} processes");
  return out;
}

// check_users.exe: the number of logged-on users.
translated_command users_windows(const json::object &macros) {
  translated_command out;
  out.query = "check_users";
  add_sides(out.arguments, macros, "users_win_warn", "users_win_crit", bound());
  return out;
}

// check_uptime.exe: alerts on an uptime above the threshold, in -u units.
translated_command uptime_windows(const json::object &macros) {
  translated_command out;
  out.query = "check_uptime";
  std::string unit = has_value(macros, "uptime_win_unit") ? macro_text(macros, "uptime_win_unit") : "s";
  for (char &c : unit) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
  if (unit != "s" && unit != "m" && unit != "h") {
    throw translation_error(unit == "ms" ? "uptime_win_unit: milliseconds are not supported, use s, m or h"
                                         : "uptime_win_unit: unknown unit '" + unit + "' (s, m or h)");
  }
  bound b;
  b.suffix = unit;
  add_sides(out.arguments, macros, "uptime_win_warn", "uptime_win_crit", b);
  return out;
}

// check_network.exe: bytes per second, in and out.
translated_command network_windows(const json::object &macros) {
  translated_command out;
  out.query = "check_network";
  add_sides(out.arguments, macros, "network_win_warn", "network_win_crit", bound());
  return out;
}

// check_perfmon.exe: one performance counter.
translated_command perfmon_windows(const json::object &macros) {
  translated_command out;
  out.query = "check_pdh";
  if (!has_value(macros, "perfmon_win_counter")) throw translation_error("perfmon_win_counter is required");
  out.arguments.push_back("counter=" + macro_text(macros, "perfmon_win_counter"));
  if (has_value(macros, "perfmon_win_type")) {
    const std::string type = macro_text(macros, "perfmon_win_type");
    out.arguments.push_back(std::string("type=") + (type == "int64" ? "large" : type));
  }
  add_sides(out.arguments, macros, "perfmon_win_warn", "perfmon_win_crit", bound());
  return out;
}

// check_ping.exe: "-w rta,loss%". The ITL builds each side from two macros
// and Icinga drops the argument unless both are set.
std::string ping_side(const json::object &macros, const char *rta, const char *loss) {
  if (!has_value(macros, rta) || !has_value(macros, loss)) return "none";
  json::object as_percent;
  as_percent[loss] = macro_text(macros, loss) + "%";
  const plugin_threshold l = threshold_of(as_percent, loss);
  return to_range(threshold_of(macros, rta), false, "", false) + "," + to_range(l, false, "", true);
}

translated_command ping_windows(const json::object &macros) {
  translated_command out;
  out.query = "check_ping";
  if (!has_value(macros, "ping_win_address")) throw translation_error("ping_win_address is required");
  out.arguments.push_back("host=" + macro_text(macros, "ping_win_address"));
  if (has_value(macros, "ping_win_packets")) out.arguments.push_back("count=" + macro_text(macros, "ping_win_packets"));
  if (has_value(macros, "ping_win_timeout")) out.arguments.push_back("timeout=" + macro_text(macros, "ping_win_timeout"));
  out.arguments.push_back("w=" + ping_side(macros, "ping_win_wrta", "ping_win_wpl"));
  out.arguments.push_back("c=" + ping_side(macros, "ping_win_crta", "ping_win_cpl"));
  return out;
}

// check_service.exe: one service, critical (or with -w, warning) unless it
// is running.
translated_command service_windows(const json::object &macros) {
  translated_command out;
  out.query = "check_service";
  if (!has_value(macros, "service_win_service")) throw translation_error("service_win_service is required");
  if (truthy_field(macros, "service_win_description")) {
    throw translation_error("service_win_description (matching a service by its description) has no counterpart; name the service");
  }
  out.arguments.push_back("service=" + macro_text(macros, "service_win_service"));
  const bool warn = truthy_field(macros, "service_win_warn");
  out.arguments.push_back(std::string("warning=") + (warn ? "state != 'running'" : "none"));
  out.arguments.push_back(std::string("critical=") + (warn ? "none" : "state != 'running'"));
  return out;
}

long update_count(const json::object &macros, const char *name) {
  const std::string value = macro_text(macros, name);
  char *end = nullptr;
  const long n = std::strtol(value.c_str(), &end, 10);
  if (value.empty() || *end != '\0') throw translation_error(std::string(name) + ": '" + value + "' is not a number of updates");
  return n;
}

// check_update.exe: "-w N" warns at N updates or more and 0 means no bound;
// the critical bound defaults to one more than the warning; an update that
// needs a reboot is critical unless told otherwise.
translated_command update_windows(const json::object &macros) {
  translated_command out;
  out.query = "check_os_updates";
  const long warn = has_value(macros, "update_win_warn") ? update_count(macros, "update_win_warn") : 0;
  const long crit = has_value(macros, "update_win_crit") ? update_count(macros, "update_win_crit") : (has_value(macros, "update_win_warn") ? warn + 1 : 0);
  out.arguments.push_back(warn != 0 ? "warning=updates >= " + std::to_string(warn) : "warning=none");
  std::string critical;
  if (!truthy_field(macros, "ignore_reboot")) critical = "reboot_required > 0";
  if (crit != 0) critical += (critical.empty() ? "" : " or ") + ("updates >= " + std::to_string(crit));
  out.arguments.push_back("critical=" + (critical.empty() ? "none" : critical));
  return out;
}

// ---------------------------------------------------------------------------
// nscp-local (itl/command-nscp-local.conf): the ITL started `nscp client` for
// each check, with the query's own arguments. They are the ITL's argument
// definitions with the dashes off, and go through the same expansion the
// ifw-api commands use. Module, log level and --boot belonged to the one-shot
// client and mean nothing to a running agent.
// ---------------------------------------------------------------------------
const char *const nscp_local_common = R"json({
  "(arguments)": {"value": "$nscp_arguments$", "skip_key": true},
  "show-all": {"set_if": "$nscp_showall$"}
})json";

translated_command nscp_local(const json::object &macros, const char *fallback_query, const char *specific) {
  translated_command out;
  out.query = has_value(macros, "nscp_query") ? macro_text(macros, "nscp_query") : fallback_query;
  if (out.query.empty()) throw translation_error("nscp_query is required");
  json::object definitions = json::parse(nscp_local_common).as_object();
  if (specific != nullptr) {
    const json::value parsed = json::parse(specific);
    for (const auto &entry : parsed.as_object()) definitions[entry.key()] = entry.value();
  }
  out.arguments = build_arguments(definitions, macros);
  return out;
}

const char *const nscp_local_cpu = R"json({
  "time": {"value": "$nscp_cpu_time$"},
  "warning": {"value": "load>$nscp_cpu_warning$"},
  "critical": {"value": "load>$nscp_cpu_critical$"},
  "(specific)": {"value": "$nscp_cpu_arguments$", "skip_key": true}
})json";
const char *const nscp_local_memory = R"json({
  "type=committed": {"set_if": "$nscp_memory_committed$"},
  "type=physical": {"set_if": "$nscp_memory_physical$"},
  "warning": {"value": "$nscp_memory_op$ $nscp_memory_warning$"},
  "critical": {"value": "$nscp_memory_op$ $nscp_memory_critical$"},
  "(specific)": {"value": "$nscp_memory_arguments$", "skip_key": true}
})json";
const char *const nscp_local_service = R"json({
  "service": {"value": "$nscp_service_name$"},
  "ok": {"value": "$nscp_service_otype$='$nscp_service_ok$'"},
  "warning": {"value": "$nscp_service_wtype$='$nscp_service_warning$'"},
  "critical": {"value": "$nscp_service_ctype$='$nscp_service_critical$'"},
  "(specific)": {"value": "$nscp_service_arguments$", "skip_key": true}
})json";
const char *const nscp_local_disk = R"json({
  "drive": {"value": "$nscp_disk_drive$"},
  "exclude": {"value": "$nscp_disk_exclude$"},
  "warning": {"value": "$nscp_disk_op$ $nscp_disk_warning$"},
  "critical": {"value": "$nscp_disk_op$ $nscp_disk_critical$"},
  "(specific)": {"value": "$nscp_disk_arguments$", "skip_key": true}
})json";
const char *const nscp_local_counter = R"json({
  "counter": {"value": "$nscp_counter_name$"},
  "warning": {"value": "value $nscp_counter_op$ $nscp_counter_warning$"},
  "critical": {"value": "value $nscp_counter_op$ $nscp_counter_critical$"},
  "perf-syntax": {"value": "$nscp_counter_perfsyntax$"},
  "(specific)": {"value": "$nscp_counter_arguments$", "skip_key": true}
})json";
const char *const nscp_local_tasksched = R"json({
  "filter": {"value": "title='$nscp_tasksched_name$'", "set_if": "$nscp_tasksched_name$"},
  "folder": {"value": "$nscp_tasksched_folder$"},
  "hidden": {"set_if": "$nscp_tasksched_hidden$"},
  "recursive": {"value": "$nscp_tasksched_recursive$"},
  "warning": {"value": "$nscp_tasksched_warning$"},
  "critical": {"value": "$nscp_tasksched_critical$"},
  "empty-state": {"value": "$nscp_tasksched_emptystate$"},
  "perf-syntax": {"value": "$nscp_tasksched_perfsyntax$"},
  "detail-syntax": {"value": "$nscp_tasksched_detailsyntax$"},
  "(specific)": {"value": "$nscp_tasksched_arguments$", "skip_key": true}
})json";

typedef translated_command (*translator)(const json::object &);

const std::map<std::string, translator> &builtins() {
  static const std::map<std::string, translator> table = {
      {"disk-windows", disk_windows},
      {"load-windows", load_windows},
      {"memory-windows", memory_windows},
      {"network-windows", network_windows},
      {"perfmon-windows", perfmon_windows},
      {"ping-windows", ping_windows},
      {"ping4-windows", ping_windows},
      {"ping6-windows", ping_windows},
      {"procs-windows", procs_windows},
      {"service-windows", service_windows},
      {"swap-windows", swap_windows},
      {"update-windows", update_windows},
      {"uptime-windows", uptime_windows},
      {"users-windows", users_windows},
      {"nscp-local", [](const json::object &m) { return nscp_local(m, "", nullptr); }},
      {"nscp-local-cpu", [](const json::object &m) { return nscp_local(m, "check_cpu", nscp_local_cpu); }},
      {"nscp-local-memory", [](const json::object &m) { return nscp_local(m, "check_memory", nscp_local_memory); }},
      {"nscp-local-os-version", [](const json::object &m) { return nscp_local(m, "check_os_version", nullptr); }},
      {"nscp-local-pagefile", [](const json::object &m) { return nscp_local(m, "check_pagefile", nullptr); }},
      {"nscp-local-process", [](const json::object &m) { return nscp_local(m, "check_process", nullptr); }},
      {"nscp-local-service", [](const json::object &m) { return nscp_local(m, "check_service", nscp_local_service); }},
      {"nscp-local-uptime", [](const json::object &m) { return nscp_local(m, "check_uptime", nullptr); }},
      {"nscp-local-version", [](const json::object &m) { return nscp_local(m, "check_version", nullptr); }},
      {"nscp-local-disk", [](const json::object &m) { return nscp_local(m, "check_drivesize", nscp_local_disk); }},
      {"nscp-local-counter", [](const json::object &m) { return nscp_local(m, "check_pdh", nscp_local_counter); }},
      {"nscp-local-tasksched", [](const json::object &m) { return nscp_local(m, "check_tasksched", nscp_local_tasksched); }},
  };
  return table;
}

// ---------------------------------------------------------------------------
// Aliases from the configuration.
// ---------------------------------------------------------------------------
translated_command expand_alias(const std::string &name, const command_alias &alias, const json::object &macros) {
  translated_command out;
  const boost::optional<json::value> query = expand_macros(alias.query, macros);
  if (!query || json_util::is_empty(query.value())) {
    throw translation_error("the query of the alias '" + name + "' uses a macro the master did not send");
  }
  out.query = macro_value_text(query.value());
  out.fixed_arguments = alias.query.find('$') == std::string::npos;
  for (const std::string &argument : alias.arguments) {
    if (argument.find('$') != std::string::npos) out.fixed_arguments = false;
    // As Icinga does: an argument whose macro has no value is left out.
    const boost::optional<json::value> value = expand_macros(argument, macros);
    if (!value || json_util::is_empty(value.value())) continue;
    // An argument that is one list macro is one argument per element.
    if (value.value().is_array()) {
      for (const json::value &element : value.value().get_array()) out.arguments.push_back(json_util::to_text(element));
    } else {
      out.arguments.push_back(json_util::to_text(value.value()));
    }
  }
  return out;
}

}  // namespace

boost::optional<translated_command> translate_command(const translation_config &config, const std::string &command, const json::object &macros) {
  const auto alias = config.aliases.find(command);
  if (alias != config.aliases.end()) return expand_alias(command, alias->second, macros);
  if (!config.builtin) return boost::none;
  const auto builtin = builtins().find(command);
  if (builtin == builtins().end()) return boost::none;
  return builtin->second(macros);
}

std::vector<std::string> builtin_translations() {
  std::vector<std::string> names;
  for (const auto &entry : builtins()) names.push_back(entry.first);
  return names;
}

}  // namespace icinga_agent
