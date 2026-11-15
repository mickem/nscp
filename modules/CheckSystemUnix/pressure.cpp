// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "pressure.h"

#include <locale>
#include <nscapi/nscapi_metrics_helper.hpp>
#include <sstream>

namespace pressure {

namespace {
// `some avg10=0.12 avg60=0.05 avg300=0.01 total=123456`
bool parse_line(const std::string &line, std::string &kind, stall &out) {
  std::istringstream is(line);
  is.imbue(std::locale::classic());
  if (!(is >> kind)) return false;
  if (kind != "some" && kind != "full") return false;
  bool have_avg10 = false, have_avg60 = false, have_avg300 = false, have_total = false;
  std::string field;
  while (is >> field) {
    const std::string::size_type eq = field.find('=');
    if (eq == std::string::npos) continue;
    const std::string key = field.substr(0, eq);
    std::istringstream value(field.substr(eq + 1));
    value.imbue(std::locale::classic());
    if (key == "avg10") {
      have_avg10 = static_cast<bool>(value >> out.avg10);
    } else if (key == "avg60") {
      have_avg60 = static_cast<bool>(value >> out.avg60);
    } else if (key == "avg300") {
      have_avg300 = static_cast<bool>(value >> out.avg300);
    } else if (key == "total") {
      have_total = static_cast<bool>(value >> out.total_us);
    }
  }
  return have_avg10 && have_avg60 && have_avg300 && have_total;
}
}  // namespace

bool parse_psi(const std::string &content, resource &out) {
  std::istringstream lines(content);
  std::string line;
  while (std::getline(lines, line)) {
    std::string kind;
    stall value;
    if (!parse_line(line, kind, value)) continue;
    if (kind == "some") {
      out.has_some = true;
      out.some = value;
    } else {
      out.has_full = true;
      out.full = value;
    }
  }
  return out.has_some || out.has_full;
}

bool parse_vmstat_value(const std::string &content, const std::string &key, unsigned long long &out) {
  std::istringstream lines(content);
  std::string line;
  while (std::getline(lines, line)) {
    std::istringstream is(line);
    is.imbue(std::locale::classic());
    std::string name;
    if (!(is >> name) || name != key) continue;
    unsigned long long value = 0;
    if (!(is >> value)) return false;
    out = value;
    return true;
  }
  return false;
}

void build_pressure_metrics(PB::Metrics::MetricsBundle *system, const sample &value) {
  using nscapi::metrics::describe;
  using nscapi::metrics::for_instance;
  using nscapi::metrics::instance_scope;
  using nscapi::metrics::metric;
  if (value.resources.empty() && !value.has_major_faults) return;

  PB::Metrics::MetricsBundle *section = system->add_children();
  section->set_key("pressure");
  describe(section, "How much the system is waiting: pressure stall information and major page faults");

  // One family per figure, labelled by resource, so `cpu.some_avg10` and
  // `io.some_avg10` are two samples of system_pressure_some_avg10_percent.
  // The help is the same for every resource, as one family's must be.
  const auto add_stall = [](const instance_scope &r, const std::string &kind, const stall &s) {
    const std::string who = kind == "some" ? "at least one task" : "every non-idle task";
    r.metric(kind + "_avg10").help("Share of time " + who + " was stalled on the resource, over the last 10 seconds").unit("percent").gauge(s.avg10);
    r.metric(kind + "_avg60").help("Share of time " + who + " was stalled on the resource, over the last 60 seconds").unit("percent").gauge(s.avg60);
    r.metric(kind + "_avg300").help("Share of time " + who + " was stalled on the resource, over the last 300 seconds").unit("percent").gauge(s.avg300);
    // Only grows while the kernel runs, so a scraper may rate() it.
    r.metric(kind + "_stall_seconds")
        .help("Time " + who + " was stalled on the resource since boot")
        .unit("seconds")
        .counter(static_cast<double>(s.total_us) / 1000000.0);
  };
  for (const resource &res : value.resources) {
    const instance_scope r = for_instance(section, res.name, "resource");
    if (res.has_some) add_stall(r, "some", res.some);
    if (res.has_full) add_stall(r, "full", res.full);
  }
  if (value.has_major_faults) {
    metric(section, "major_faults").help("Major page faults since boot: pages that had to be read in from disk").counter(value.major_faults);
  }
}

}  // namespace pressure
