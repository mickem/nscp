// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/json.hpp>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <map>
#include <random>
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
//     "version": 1,
//     "report_id": "<ulid>",
//     "sent_at": "<iso>", "sent_at_unix": <seconds>,
//     "failure": { <the entry as /api/v2/failures renders it, `key` included>, "date": "<iso>" },
//     "context": { "state", "snapshot_id", "captured_at": "<iso>", "captured_at_unix", "age_seconds",
//                  "metrics": {...flat, as /api/v2/metrics...}, "metadata": {...} },
//     "suppressed": { "since": "<iso>", "since_unix", "count", "evicted",
//                     "by_check": [ {"key", "command", "status", "count",
//                                    "first", "first_unix", "last", "last_unix"} ] }
//   }
//
// `metrics` and `metadata` are only there when the state is `captured`. A
// snapshot that is held but left out because the server cannot take a body
// that large is reported as `omitted`.
// `suppressed.count` is the failures listed in `by_check`; `evicted` is the
// ones the log's cap dropped before they could be reported, which the server
// otherwise has no way to know about.
//
// Every time is there twice: as an ISO string to show, and as unix seconds
// (`*_unix`, and `failure.timestamp`) to sort and compute on.
//
// `report_id` names the report, not the attempt: a report whose answer was
// lost is sent again under the same id, so a server that did receive it can
// tell a repeat from a second occurrence. The repeat may have grown - it
// covers every failure since the last report that was acknowledged - so it
// replaces the earlier one rather than adding to it.
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
  // Names this report across its retries; see above.
  std::string report_id;
  // Cleared to send the report without its snapshot, for a server (or a
  // proxy in front of it) that cannot take a body that large.
  bool include_metrics = true;

  // The format of the body. Raised when a field changes meaning or goes
  // away; a field that is only added does not raise it.
  static constexpr int wire_version = 1;

  // Whether rendering would put a snapshot in the body.
  bool carries_metrics() const {
    return include_metrics && context && failure.context().state() == PB::Failures::MetricsContext::CAPTURED;
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

  static std::string iso_time(const std::int64_t epoch) {
    if (epoch <= 0) return "";
    return boost::posix_time::to_iso_extended_string(boost::posix_time::from_time_t(static_cast<std::time_t>(epoch))) + "Z";
  }

  std::string render(const std::int64_t now) const {
    boost::json::object root;
    root["version"] = wire_version;
    if (!report_id.empty()) root["report_id"] = report_id;
    root["sent_at"] = iso_time(now);
    root["sent_at_unix"] = now;

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
    // Held, and left out on purpose: not one of the log's own states, which
    // all say what the host has.
    const bool omitted = state == PB::Failures::MetricsContext::CAPTURED && !include_metrics;
    context_node["state"] = omitted ? "omitted" : nscapi::failures::context_state_name(state);
    if (reference.snapshot_id() > 0) {
      context_node["snapshot_id"] = static_cast<std::int64_t>(reference.snapshot_id());
      context_node["captured_at"] = iso_time(reference.captured_at());
      context_node["captured_at_unix"] = reference.captured_at();
      context_node["age_seconds"] = failure.timestamp() > reference.captured_at() ? failure.timestamp() - reference.captured_at() : 0;
    }
    if (state == PB::Failures::MetricsContext::CAPTURED && !omitted) {
      boost::json::object metrics_list, metrics_meta;
      metrics_json::build(metrics_list, &metrics_meta, this->context->message);
      context_node["metrics"] = metrics_list;
      context_node["metadata"] = metrics_meta;
    }
    root["context"] = context_node;

    // Grouped by problem (the entry's key: the command and what it was
    // pointed at) and status, in the order each group was first seen going
    // back from the newest, so the busiest recent problem leads. By key and
    // not by command, so the counts can be attributed: two full drives are
    // two groups, each with its own count.
    struct group {
      std::string key;
      std::string command;
      int status = 0;
      std::size_t count = 0;
      std::int64_t first = 0;
      std::int64_t last = 0;
    };
    std::vector<group> groups;
    std::map<std::pair<std::string, int>, std::size_t> position;
    for (const PB::Failures::FailedCheck &other : suppressed) {
      const std::pair<std::string, int> key(other.key(), failures_json::status_number(other.response().result()));
      std::map<std::pair<std::string, int>, std::size_t>::const_iterator it = position.find(key);
      if (it == position.end()) {
        it = position.insert(std::make_pair(key, groups.size())).first;
        group g;
        g.key = key.first;
        g.command = other.response().command();
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
      node["key"] = g.key;
      node["command"] = g.command;
      node["status"] = g.status;
      node["count"] = static_cast<std::int64_t>(g.count);
      node["first"] = iso_time(g.first);
      node["first_unix"] = g.first;
      node["last"] = iso_time(g.last);
      node["last_unix"] = g.last;
      by_check.push_back(node);
    }
    boost::json::object summary;
    summary["since"] = iso_time(since);
    summary["since_unix"] = since;
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
