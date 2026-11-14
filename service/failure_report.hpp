// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/json.hpp>
#include <cstdint>
#include <ctime>
#include <map>
#include <metrics/metrics_json.hpp>
#include <nscapi/protobuf/failures_json.hpp>
#include <string>
#include <utility>
#include <vector>

#include "failed_check_store.hpp"

namespace nsclient {
namespace core {

// The body of a failure-context report (POST /agent/v1/failure-context): one
// failed check with the metrics snapshot it was recorded with, plus a summary
// of every other failure since the previous report that was not sent on its
// own. The fleet sync sends at most one of these per interval (see
// onboarding::failure_context_pacer), so `suppressed` is what lets the server
// show a check that flapped thirty times in the hour without receiving thirty
// snapshots.
//
//   {
//     "sent_at": "<iso>",
//     "failure": { <the entry as /api/v2/failures renders it>, "date": "<iso>" },
//     "context": { "state", "snapshot_id", "captured_at": "<iso>", "age_seconds",
//                  "metrics": {...flat, as /api/v2/metrics...}, "metadata": {...},
//                  "pruned": { "rules", "zeros", "empty", "duplicates", "collapsed": {...} } },
//     "suppressed": { "since": "<iso>", "count", "evicted",
//                     "by_check": [ {"command", "status", "count", "first", "last"} ] }
//   }
//
// `metrics` and `metadata` are only there when the state is `captured`, and
// `pruned` only when `prune metrics` trimmed the snapshot.
// `suppressed.count` is the failures listed in `by_check`; `evicted` is the
// ones the log's cap dropped before they could be reported, which the server
// otherwise has no way to know about.
struct failure_report {
  // The failure to send, and the snapshot it refers to (null when it has
  // none any more).
  PB::Failures::FailedCheck failure;
  metrics_snapshot_ptr context;
  // The other failures since the last report, newest first.
  failed_check_store::entry_list suppressed;
  unsigned long long evicted = 0;
  // The start of the window: the previous report, or the oldest failure in it.
  std::int64_t since = 0;
  bool arguments_recorded = false;

  static std::string iso_time(const std::int64_t epoch) {
    if (epoch <= 0) return "";
    return boost::posix_time::to_iso_extended_string(boost::posix_time::from_time_t(static_cast<std::time_t>(epoch))) + "Z";
  }

  std::string render(const std::int64_t now) const {
    boost::json::object root;
    root["sent_at"] = iso_time(now);

    boost::json::object entry = failures_json::entry(failure, arguments_recorded);
    entry["date"] = iso_time(failure.timestamp());
    // The context is reported on its own, below, with the snapshot.
    entry.erase("context");
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
      context_node["captured_at"] = iso_time(reference.captured_at());
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
      node["first"] = iso_time(g.first);
      node["last"] = iso_time(g.last);
      by_check.push_back(node);
    }
    boost::json::object summary;
    summary["since"] = iso_time(since);
    summary["count"] = static_cast<std::int64_t>(suppressed.size());
    summary["evicted"] = static_cast<std::int64_t>(evicted);
    summary["by_check"] = by_check;
    root["suppressed"] = summary;
    return boost::json::serialize(root);
  }

  // Pick what to send from the failures since the last report (`fresh`,
  // newest first, not empty): the newest one whose snapshot is still held -
  // the freshest picture of the system in trouble - or, when none has one,
  // the newest. The rest go into `suppressed`.
  static failure_report from(const failed_check_store &store, const failed_check_store::entry_list &fresh) {
    failure_report report;
    std::size_t pick = 0;
    for (std::size_t i = 0; i < fresh.size(); ++i) {
      if (fresh[i].context().state() == PB::Failures::MetricsContext::CAPTURED) {
        pick = i;
        break;
      }
    }
    failed_check_store::entry_with_context chosen;
    if (store.get(fresh[pick].index(), chosen)) {
      report.failure = chosen.item;
      report.context = chosen.context;
    } else {
      report.failure = fresh[pick];
    }
    for (std::size_t i = 0; i < fresh.size(); ++i) {
      if (i != pick) report.suppressed.push_back(fresh[i]);
    }
    report.arguments_recorded = store.get_options().record_arguments;
    return report;
  }
};

}  // namespace core
}  // namespace nsclient
