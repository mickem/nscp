// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/json.hpp>
#include <chrono>
#include <cstdint>
#include <map>
#include <metrics/metrics_json.hpp>
#include <nscapi/protobuf/diagnostics_json.hpp>
#include <nscapi/protobuf/failures_json.hpp>
#include <random>
#include <str/rfc3339.hpp>
#include <string>
#include <utility>
#include <vector>

#include "failed_check_store.hpp"

namespace nsclient {
namespace core {

// The body of a failure-context report (POST /agent/v1/failure-context): what
// the host looked like when a check failed. It is there to enrich an alert a
// monitoring system already holds, not to be one: the snapshots are the
// payload, and a server files them by host and time. The failed check they
// were recorded with, and the summary of every other failure since the
// previous report, say what the host was in trouble over - a label for the
// reader, not a feed of events to track. The fleet sync sends at most one of
// these per interval (see onboarding::failure_context_pacer), and none for
// failures that have no snapshot to send.
//
//   {
//     "version": 1,
//     "report_id": "<ulid>",
//     "sent_at": "<iso>", "sent_at_unix": <seconds>,
//     "arguments_mode": "none" | "safe" | "all",
//     "failure": { <the entry as /api/v2/failures renders it>, "date": "<iso>" },
//     "context": { "state", "snapshot_id", "captured_at": "<iso>", "captured_at_unix", "age_seconds",
//                  "metrics": {...flat, as /api/v2/metrics...}, "metadata": {...},
//                  "pruned": { "rules", "zeros", "empty", "duplicates", "collapsed": {...} } },
//     "diagnostics": { "state", "snapshot_id", "captured_at": "<iso>", "captured_at_unix", "age_seconds",
//                      "sections": { "<name>": {...as /api/v2/diagnostics/<name>...} } },
//     "suppressed": { "since": "<iso>", "since_unix", "count", "evicted",
//                     "by_check": [ {"command", "status", "count",
//                                    "first", "first_unix", "last", "last_unix"} ] }
//   }
//
// Every time is there twice: as an ISO string to show, and as unix seconds
// (`*_unix`, and `failure.timestamp`) to sort and join on - the time is what
// a server matches an alert to a snapshot by.
//
// `report_id` names the report, not the attempt: a report whose answer was
// lost is sent again under the same id, so a server that did receive it does
// not file the same snapshot twice. The repeat can have grown - it covers
// every failure since the last report that was acknowledged - so it replaces
// the earlier one.
//
// `metrics` and `metadata` are only there when the state is `captured`, and
// `pruned` only when `prune metrics` trimmed the snapshot; the same goes for
// `diagnostics.sections`. Diagnostics ride on the same opt-in as the metrics
// context (the server's `X-Failure-Context: accept`): a server that predates
// them ignores the key, and an agent that predates them sends none.
// `suppressed.count` is the failures listed in `by_check`; `evicted` is the
// ones the log's cap dropped before they could be reported, which the server
// otherwise has no way to know about.
struct failure_report {
  // The failure to send, and the snapshot it refers to (null when it has
  // none any more).
  PB::Failures::FailedCheck failure;
  metrics_snapshot_ptr context;
  // The diagnostics snapshot it refers to (null when it has none any more).
  diagnostics_snapshot_ptr diagnostics;
  // The other failures since the last report, newest first.
  failed_check_store::entry_list suppressed;
  unsigned long long evicted = 0;
  // The start of the window: the previous report, or the oldest failure in it.
  std::int64_t since = 0;
  bool arguments_recorded = false;
  // `record arguments` as it is spelled ("none", "safe", "all"), so the
  // server can tell a list cut down to the allow-list from a complete one.
  std::string arguments_mode = "none";
  // Names this report across its retries; see above.
  std::string report_id;

  // The format of the body. Raised when a field changes meaning or goes
  // away; a field that is only added does not raise it.
  static constexpr int wire_version = 1;

  // Whether there is a snapshot to send, metrics or diagnostics. A report
  // without one tells the server nothing a monitoring system has not already
  // said, so it is not sent.
  bool has_snapshot() const {
    return (context && failure.context().state() == PB::Failures::MetricsContext::CAPTURED) ||
           (diagnostics && failure.diagnostics().state() == PB::Failures::DiagnosticsContext::CAPTURED);
  }

  // A ULID: 48 bits of milliseconds since the epoch and 80 random bits, as
  // 26 characters of Crockford base32. Sorts by the time it was made.
  template <class Random>
  static std::string make_report_id(std::uint64_t milliseconds, Random &random) {
    static const char alphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
    std::string out(26, '0');
    for (int i = 9; i >= 0; --i) {
      out[static_cast<std::size_t>(i)] = alphabet[milliseconds & 31];
      milliseconds >>= 5;
    }
    std::uniform_int_distribution<int> digit(0, 31);
    for (std::size_t i = 10; i < 26; ++i) out[i] = alphabet[digit(random)];
    return out;
  }
  static std::string make_report_id() {
    std::random_device random;
    const std::uint64_t ms = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    return make_report_id(ms, random);
  }

  std::string render(const std::int64_t now) const {
    boost::json::object root;
    root["version"] = wire_version;
    if (!report_id.empty()) root["report_id"] = report_id;
    root["sent_at"] = str::format_rfc3339(now);
    root["sent_at_unix"] = now;
    root["arguments_mode"] = arguments_mode;

    boost::json::object entry = failures_json::entry(failure, arguments_recorded);
    entry["date"] = str::format_rfc3339(failure.timestamp());
    // The context and diagnostics are reported on their own, below, with the
    // snapshots.
    entry.erase("context");
    entry.erase("diagnostics");
    root["failure"] = entry;

    const PB::Failures::MetricsContext &reference = failure.context();
    boost::json::object context_node;
    // An entry listed as captured whose snapshot went before it was read
    // (the budget released it in between) is reported as what it now is.
    PB::Failures::MetricsContext::State state = reference.state();
    if (state == PB::Failures::MetricsContext::CAPTURED && !this->context) state = PB::Failures::MetricsContext::EVICTED;
    context_node["state"] = nscapi::failures::context_state_name(state);
    if (reference.snapshot_id() > 0) {
      context_node["snapshot_id"] = static_cast<std::int64_t>(reference.snapshot_id());
      context_node["captured_at"] = str::format_rfc3339(reference.captured_at());
      context_node["captured_at_unix"] = reference.captured_at();
      context_node["age_seconds"] = failure.timestamp() > reference.captured_at() ? failure.timestamp() - reference.captured_at() : 0;
    }
    if (state == PB::Failures::MetricsContext::CAPTURED) {
      boost::json::object metrics_list, metrics_meta;
      metrics_json::build(metrics_list, &metrics_meta, this->context->message);
      context_node["metrics"] = metrics_list;
      context_node["metadata"] = metrics_meta;
      const boost::json::value pruned = failures_json::pruned(reference);
      if (!pruned.is_null()) context_node["pruned"] = pruned;
    }
    root["context"] = context_node;

    const PB::Failures::DiagnosticsContext &diagnostics_ref = failure.diagnostics();
    boost::json::object diagnostics_node;
    PB::Failures::DiagnosticsContext::State diagnostics_state = diagnostics_ref.state();
    if (diagnostics_state == PB::Failures::DiagnosticsContext::CAPTURED && !this->diagnostics) diagnostics_state = PB::Failures::DiagnosticsContext::EVICTED;
    diagnostics_node["state"] = nscapi::failures::context_state_name(diagnostics_state);
    if (diagnostics_ref.snapshot_id() > 0) {
      diagnostics_node["snapshot_id"] = static_cast<std::int64_t>(diagnostics_ref.snapshot_id());
      diagnostics_node["captured_at"] = str::format_rfc3339(diagnostics_ref.captured_at());
      diagnostics_node["captured_at_unix"] = diagnostics_ref.captured_at();
      diagnostics_node["age_seconds"] = failure.timestamp() > diagnostics_ref.captured_at() ? failure.timestamp() - diagnostics_ref.captured_at() : 0;
    }
    if (diagnostics_state == PB::Failures::DiagnosticsContext::CAPTURED) {
      diagnostics_node["sections"] = diagnostics_json::sections(this->diagnostics->message);
    }
    root["diagnostics"] = diagnostics_node;

    // Grouped by command and status, in the order each group was first seen
    // going back from the newest, so the busiest recent problem leads.
    struct group {
      std::string command;
      int status = 0;
      std::size_t count = 0;
      std::int64_t first = 0;
      std::int64_t last = 0;
    };
    std::vector<group> groups;
    std::map<std::pair<std::string, int>, std::size_t> position;
    for (const PB::Failures::FailedCheck &other : suppressed) {
      const std::pair<std::string, int> key(other.response().command(), failures_json::status_number(other.response().result()));
      std::map<std::pair<std::string, int>, std::size_t>::const_iterator it = position.find(key);
      if (it == position.end()) {
        it = position.insert(std::make_pair(key, groups.size())).first;
        group g;
        g.command = key.first;
        g.status = key.second;
        g.first = other.timestamp();
        g.last = other.timestamp();
        groups.push_back(g);
      }
      group &g = groups[it->second];
      ++g.count;
      if (other.timestamp() < g.first) g.first = other.timestamp();
      if (other.timestamp() > g.last) g.last = other.timestamp();
    }
    boost::json::array by_check;
    for (const group &g : groups) {
      boost::json::object node;
      node["command"] = g.command;
      node["status"] = g.status;
      node["count"] = static_cast<std::int64_t>(g.count);
      node["first"] = str::format_rfc3339(g.first);
      node["first_unix"] = g.first;
      node["last"] = str::format_rfc3339(g.last);
      node["last_unix"] = g.last;
      by_check.push_back(node);
    }
    boost::json::object summary;
    summary["since"] = str::format_rfc3339(since);
    summary["since_unix"] = since;
    summary["count"] = static_cast<std::int64_t>(suppressed.size());
    summary["evicted"] = static_cast<std::int64_t>(evicted);
    summary["by_check"] = by_check;
    root["suppressed"] = summary;
    return boost::json::serialize(root);
  }

  // Pick what to send from the failures since the last report (`fresh`,
  // newest first, not empty): the newest one with a snapshot still held,
  // metrics or diagnostics - the freshest picture of the system in trouble -
  // or, when none has one, the newest. The rest go into `suppressed`.
  static failure_report from(const failed_check_store &store, const failed_check_store::entry_list &fresh) {
    failure_report report;
    std::size_t pick = 0;
    for (std::size_t i = 0; i < fresh.size(); ++i) {
      if (fresh[i].context().state() == PB::Failures::MetricsContext::CAPTURED ||
          fresh[i].diagnostics().state() == PB::Failures::DiagnosticsContext::CAPTURED) {
        pick = i;
        break;
      }
    }
    failed_check_store::entry_with_context chosen;
    if (store.get(fresh[pick].index(), chosen)) {
      report.failure = chosen.item;
      report.context = chosen.context;
      report.diagnostics = chosen.diagnostics;
    } else {
      report.failure = fresh[pick];
    }
    for (std::size_t i = 0; i < fresh.size(); ++i) {
      if (i != pick) report.suppressed.push_back(fresh[i]);
    }
    const failed_check_store::argument_mode mode = store.get_options().record_arguments;
    report.arguments_recorded = mode != failed_check_store::argument_mode::none;
    report.arguments_mode = failed_check_store::format_argument_mode(mode);
    return report;
  }
};

}  // namespace core
}  // namespace nsclient
