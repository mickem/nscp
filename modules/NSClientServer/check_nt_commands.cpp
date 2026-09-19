// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_nt_commands.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <nscapi/protobuf/functions_perfdata.hpp>
#include <str/utils.hpp>
#include <str/xtos.hpp>

namespace check_nt_commands {

namespace {
// Resolve a single token from the `allow` setting - either the keyword
// "any"/"all", a group name, or an individual check_nt command name - into the
// request code(s) it permits. Unrecognised tokens are collected in `unknown`.
void add_allow_token(const std::string &raw, std::set<int> &out, std::set<std::string> &unknown) {
  std::string t = boost::algorithm::to_lower_copy(boost::algorithm::trim_copy(raw));
  if (t.empty()) return;
  if (t == "any" || t == "all") {
    for (int c = REQ_CLIENTVERSION; c <= REQ_INSTANCES; ++c) out.insert(c);
  } else if (t == "metrics") {  // harmless aggregate system metrics
    out.insert(REQ_CPULOAD);
    out.insert(REQ_UPTIME);
    out.insert(REQ_USEDDISKSPACE);
    out.insert(REQ_MEMUSE);
  } else if (t == "info") {
    out.insert(REQ_CLIENTVERSION);
  } else if (t == "service") {
    out.insert(REQ_SERVICESTATE);
  } else if (t == "process") {
    out.insert(REQ_PROCSTATE);
  } else if (t == "counters") {  // arbitrary PDH counter read
    out.insert(REQ_COUNTER);
    out.insert(REQ_INSTANCES);
  } else if (t == "files") {  // arbitrary file existence/age
    out.insert(REQ_FILEAGE);
  } else if (t == "clientversion") {
    out.insert(REQ_CLIENTVERSION);
  } else if (t == "cpuload") {
    out.insert(REQ_CPULOAD);
  } else if (t == "uptime") {
    out.insert(REQ_UPTIME);
  } else if (t == "useddiskspace") {
    out.insert(REQ_USEDDISKSPACE);
  } else if (t == "servicestate") {
    out.insert(REQ_SERVICESTATE);
  } else if (t == "procstate") {
    out.insert(REQ_PROCSTATE);
  } else if (t == "memuse") {
    out.insert(REQ_MEMUSE);
  } else if (t == "counter") {
    out.insert(REQ_COUNTER);
  } else if (t == "fileage") {
    out.insert(REQ_FILEAGE);
  } else if (t == "instances") {
    out.insert(REQ_INSTANCES);
  } else {
    unknown.insert(t);
  }
}

void split_to_list(std::list<std::string> &list, const std::string &str, const std::string &key) {
  for (const std::string &s : str::utils::split_lst(str, std::string("&"))) {
    list.push_back(key + "=" + s);
  }
}

std::string extract_perf_value(const PB::Common::PerformanceData &perf) { return nscapi::protobuf::functions::extract_perf_value_as_string(perf); }
std::string extract_perf_total(const PB::Common::PerformanceData &perf) { return nscapi::protobuf::functions::extract_perf_maximum_as_string(perf); }
long long extract_perf_value_i(const PB::Common::PerformanceData &perf) { return nscapi::protobuf::functions::extract_perf_value_as_int(perf); }
}  // namespace

std::set<int> parse_allowed_commands(const std::string &spec, std::set<std::string> &unknown) {
  std::set<int> out;
  for (const std::string &tok : str::utils::split_lst(spec, std::string(","))) {
    add_allow_token(tok, out, unknown);
  }
  return out;
}

bool map_request(int code, const std::string &raw_args, mapped_command &out) {
  switch (code) {
    case REQ_CPULOAD:
      out.command = "check_cpu";
      for (const std::string &s : str::utils::split_lst(raw_args, std::string("&"))) {
        out.arguments.push_back("time=" + s + "m");
      }
      return true;
    case REQ_UPTIME:
      out.command = "check_uptime";
      out.arguments.push_back("warn=uptime<0");
      return true;
    case REQ_USEDDISKSPACE:
      out.command = "check_drivesize";
      split_to_list(out.arguments, raw_args, "drive");
      out.arguments.push_back("warn=free<0");
      out.arguments.push_back("crit=free<0");
      out.arguments.push_back("filter=type='fixed' and mounted = 1");
      out.arguments.push_back("perf-config=used(unit:B)free(unit:B)");
      return true;
    case REQ_SERVICESTATE:
      out.command = "check_service";
      split_to_list(out.arguments, raw_args, "service");
      if (out.arguments.size() > 0 && *out.arguments.begin() == "service=ShowFail") out.arguments.erase(out.arguments.begin());
      if (out.arguments.size() > 0 && *out.arguments.begin() == "service=ShowAll") {
        out.arguments.erase(out.arguments.begin());
        out.arguments.push_back("top-syntax=${list}");
      }
      out.arguments.push_back("detail-syntax=${name}: ${legacy_state}");
      out.arguments.push_back("empty-syntax=OK: All services are in their appropriate state.");
      out.arguments.push_back("filter=none");
      out.arguments.push_back("crit=not state = 'running'");
      return true;
    case REQ_PROCSTATE:
      out.command = "check_process";
      split_to_list(out.arguments, raw_args, "process");
      if (out.arguments.size() > 0 && *out.arguments.begin() == "process=ShowFail") out.arguments.erase(out.arguments.begin());
      if (out.arguments.size() > 0 && *out.arguments.begin() == "process=ShowAll") {
        out.arguments.erase(out.arguments.begin());
        out.arguments.push_back("top-syntax=${list}");
      }
      out.arguments.push_back("detail-syntax=${exe}: ${legacy_state}");
      out.arguments.push_back("empty-syntax=OK: All processes are running.");
      return true;
    case REQ_MEMUSE:
      out.command = "check_memory";
      out.arguments.push_back("warn=used<0");
      out.arguments.push_back("crit=used<0");
      out.arguments.push_back("filter=none");
      out.arguments.push_back("type=committed");
      out.arguments.push_back("perf-config=used(unit:B)free(unit:B)");
      return true;
    case REQ_COUNTER:
      out.command = "check_pdh";
      out.arguments.push_back("counter=" + raw_args);
      return true;
    case REQ_FILEAGE:
      // check_single_file, not check_files: FILEAGE answers with one number,
      // and format_response below reads perf(0) to get it. Mapped onto
      // check_files, a directory argument produced one performance value per
      // file and the client was handed whichever the walk emitted first - not
      // the oldest, not the newest, just arbitrary - while the message listed
      // every file beneath it and its mtime, so an authenticated caller could
      // enumerate C:\Users or /home one request at a time. Capping the depth
      // narrowed the listing but left the number just as arbitrary.
      //
      // A check that stats exactly one path cannot do either: there is only
      // ever one candidate, so the age is defined and there is nothing to
      // enumerate. A directory argument now fails instead of answering.
      out.command = "check_single_file";
      out.arguments.push_back("file=" + raw_args);
      // age is only emitted as performance data when a threshold names it,
      // and format_response needs perf(0) to be the age. age is never
      // negative, so this arms the perf counter without ever firing. It also
      // keeps check_single_file out of its no-thresholds UNKNOWN default.
      out.arguments.push_back("crit=age<0");
      out.arguments.push_back("detail-syntax=${file} ${written}");
      out.arguments.push_back("top-syntax=${list}");
      return true;
    default:
      // REQ_CLIENTVERSION and REQ_INSTANCES are answered inline by the
      // server; anything else is an unknown code.
      return false;
  }
}

std::string format_response(int code, const std::string &command, const PB::Commands::QueryResponseMessage::Response &payload) {
  if (payload.lines_size() != 1) {
    return "ERROR: Invalid number of lines returned from command: " + command + ", " + str::xtos(payload.lines_size());
  }
  const PB::Commands::QueryResponseMessage::Response::Line &line = payload.lines(0);

  switch (code) {
    case REQ_CPULOAD:  // Return the first performance data value
    case REQ_UPTIME:
    case REQ_COUNTER:
      if (line.perf_size() < 1) return "ERROR: No performance data from command: " + command;
      return extract_perf_value(line.perf(0));

    case REQ_MEMUSE:
      if (line.perf_size() < 1) return "ERROR: No performance data from command: " + command;
      return extract_perf_total(line.perf(0)) + "&" + extract_perf_value(line.perf(0));
    case REQ_USEDDISKSPACE:
      if (line.perf_size() < 1) return "ERROR: No performance data from command: " + command;
      return extract_perf_value(line.perf(0)) + "&" + extract_perf_total(line.perf(0));
    case REQ_FILEAGE:
      if (line.perf_size() < 1) return "ERROR: No performance data from command: " + command;
      return str::xtos_non_sci(extract_perf_value_i(line.perf(0)) / 60) + "&" + line.message();

    case REQ_SERVICESTATE:  // Some check_nt commands return the return code (coded as a string)
    case REQ_PROCSTATE:
      return str::xtos(payload.result()) + "& " + line.message();

    default:
      return "ERROR: Unknown command " + command;
  }
}

}  // namespace check_nt_commands
