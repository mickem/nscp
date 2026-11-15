// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "diagnostics_systemd.h"

#include <boost/json.hpp>
#include <cerrno>
#include <cstdlib>
#include <sstream>

namespace systemd_diagnostics {

namespace {

// A decimal integer, the whole of `text` (no locale, no trailing junk).
bool to_int(const std::string &text, std::int64_t &out) {
  if (text.empty()) return false;
  errno = 0;
  char *end = nullptr;
  const long long v = std::strtoll(text.c_str(), &end, 10);
  if (errno != 0 || end == nullptr || *end != '\0') return false;
  out = static_cast<std::int64_t>(v);
  return true;
}

std::string get(const std::map<std::string, std::string> &m, const std::string &key) {
  const auto it = m.find(key);
  return it == m.end() ? std::string() : it->second;
}

std::string without_service(const std::string &unit) {
  static const std::string suffix = ".service";
  if (unit.size() > suffix.size() && unit.compare(unit.size() - suffix.size(), suffix.size(), suffix) == 0) return unit.substr(0, unit.size() - suffix.size());
  return unit;
}

std::string trim(const std::string &s) {
  const std::size_t b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return "";
  const std::size_t e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

std::string field(const boost::json::object &o, const char *name) {
  const boost::json::value *v = o.if_contains(name);
  if (v == nullptr || !v->is_string()) return "";
  return std::string(v->get_string().c_str(), v->get_string().size());
}

std::string level_of(const std::string &priority) {
  if (priority == "0") return "emerg";
  if (priority == "1") return "alert";
  if (priority == "2") return "crit";
  if (priority == "3") return "err";
  return priority;
}

}  // namespace

std::vector<unit_listing> parse_list_units(const std::string &output) {
  std::vector<unit_listing> units;
  std::istringstream lines(output);
  std::string line;
  while (std::getline(lines, line)) {
    std::istringstream row(line);
    unit_listing u;
    if (!(row >> u.id >> u.load >> u.active >> u.sub)) continue;
    units.push_back(u);
  }
  return units;
}

bool is_candidate(const unit_listing &unit) {
  if (unit.load == "not-found") return false;
  return unit.active == "failed" || unit.active == "activating" || unit.sub == "auto-restart" || (unit.active == "active" && unit.sub == "running");
}

bool is_running(const unit_listing &unit) { return unit.load != "not-found" && unit.active == "active" && unit.sub == "running"; }

std::vector<std::string> select_units(const std::vector<unit_listing> &units, const bool sweep, const std::set<std::string> &restarted) {
  std::vector<std::string> picked;
  for (const unit_listing &u : units) {
    if (!is_candidate(u)) continue;
    if (is_running(u) && !sweep && restarted.count(u.id) == 0) continue;
    picked.push_back(u.id);
  }
  return picked;
}

std::string show_properties() {
  return "Id,LoadState,ActiveState,SubState,Result,NRestarts,ExecMainCode,ExecMainStatus,ExecMainStartTimestampMonotonic,MainPID,ExecStart,User";
}

std::vector<std::map<std::string, std::string>> parse_show(const std::string &output) {
  std::vector<std::map<std::string, std::string>> blocks;
  std::map<std::string, std::string> current;
  std::istringstream lines(output);
  std::string line;
  while (std::getline(lines, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) {
      if (!current.empty()) blocks.push_back(current);
      current.clear();
      continue;
    }
    const std::size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    current[line.substr(0, eq)] = line.substr(eq + 1);
  }
  if (!current.empty()) blocks.push_back(current);
  return blocks;
}

std::string exec_path(const std::string &exec_start) {
  const std::size_t at = exec_start.find("path=");
  if (at == std::string::npos) return "";
  const std::size_t begin = at + 5;
  const std::size_t end = exec_start.find(" ;", begin);
  return trim(exec_start.substr(begin, end == std::string::npos ? std::string::npos : end - begin));
}

bool problem_row(const std::map<std::string, std::string> &unit, const std::int64_t now, const std::int64_t monotonic_now_us,
                 service_tables::service_row &out) {
  if (get(unit, "LoadState") == "not-found") return false;
  const std::string id = get(unit, "Id");
  if (id.empty()) return false;
  const std::string active = get(unit, "ActiveState");
  const std::string sub = get(unit, "SubState");
  const std::string result = get(unit, "Result");
  std::int64_t restarts = 0;
  const bool has_restarts = to_int(get(unit, "NRestarts"), restarts);

  const bool failed = active == "failed";
  const bool restarting = sub == "auto-restart";
  const bool restarted = has_restarts && restarts > 0;
  const bool unsuccessful = !result.empty() && result != "success";
  if (!failed && !restarting && !restarted && !unsuccessful) return false;

  out = service_tables::service_row();
  out.name = without_service(id);
  out.state = sub.empty() || sub == active ? active : active + " (" + sub + ")";
  // ExecMainCode is the main process's si_code: 0 while it has not exited,
  // CLD_EXITED (1) with ExecMainStatus its exit status, CLD_KILLED (2) or
  // CLD_DUMPED (3) with the signal.
  std::int64_t code = 0, status = 0;
  if (to_int(get(unit, "ExecMainCode"), code) && code != 0 && to_int(get(unit, "ExecMainStatus"), status)) {
    out.has_exit_code = true;
    out.exit_code = status;
  }
  out.binary_path = exec_path(get(unit, "ExecStart"));
  const std::string user = get(unit, "User");
  out.account = user.empty() ? "root" : user;
  std::int64_t pid = 0, started_us = 0;
  if (to_int(get(unit, "MainPID"), pid) && pid > 0 && to_int(get(unit, "ExecMainStartTimestampMonotonic"), started_us) && started_us > 0 &&
      started_us <= monotonic_now_us) {
    out.started = now - (monotonic_now_us - started_us) / 1000000;
  }
  if (has_restarts) {
    out.has_restart_count = true;
    out.restart_count = restarts;
  }
  return true;
}

std::vector<std::string> journal_args(const feed which, const std::string &cursor, const std::int64_t since, const std::size_t limit) {
  std::vector<std::string> args = {"--no-pager", "--quiet", "--output=json", "--lines=" + std::to_string(limit)};
  if (cursor.empty())
    args.push_back("--since=@" + std::to_string(since));
  else
    args.push_back("--after-cursor=" + cursor);
  if (which == feed::errors) {
    args.push_back("--priority=0..3");
  } else {
    // Matches on different fields must all hold; on the same field, any one.
    args.push_back("_PID=1");
    for (const char *id : {message_unit_failed, message_unit_failure_result, message_unit_process_exit, message_unit_restart})
      args.push_back(std::string("MESSAGE_ID=") + id);
  }
  return args;
}

journal_read parse_journal(const feed which, const std::string &output) {
  journal_read out;
  std::istringstream lines(output);
  std::string line;
  while (std::getline(lines, line)) {
    if (line.empty()) continue;
    boost::system::error_code ec;
    const boost::json::value value = boost::json::parse(line, ec);
    if (ec || !value.is_object()) continue;
    const boost::json::object &o = value.get_object();
    const std::string cursor = field(o, "__CURSOR");
    if (!cursor.empty()) out.cursor = cursor;

    event_tables::event_row row;
    row.key = cursor;
    std::int64_t usec = 0;
    if (to_int(field(o, "__REALTIME_TIMESTAMP"), usec)) row.time = usec / 1000000;
    row.message = event_tables::message(field(o, "MESSAGE"));

    if (which == feed::service_events) {
      const std::string unit = field(o, "UNIT");
      if (unit.empty()) continue;
      // A main process that exited cleanly is no event.
      if (field(o, "MESSAGE_ID") == message_unit_process_exit && field(o, "EXIT_CODE") == "exited" && field(o, "EXIT_STATUS") == "0") continue;
      row.service = without_service(unit);
      row.log = "journal";
      row.source = "systemd";
    } else {
      const std::string transport = field(o, "_TRANSPORT");
      const std::string unit = field(o, "_SYSTEMD_UNIT");
      row.log = !unit.empty() ? unit : transport == "kernel" ? "kernel" : "journal";
      row.source = field(o, "SYSLOG_IDENTIFIER");
      if (row.source.empty()) row.source = field(o, "_COMM");
      if (row.source.empty() && transport == "kernel") row.source = "kernel";
      row.level = level_of(field(o, "PRIORITY"));
    }
    out.rows.push_back(row);
  }
  return out;
}

}  // namespace systemd_diagnostics
