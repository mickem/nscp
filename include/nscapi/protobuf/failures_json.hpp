// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/json.hpp>
#include <cstdint>
#include <nscapi/protobuf/failures.hpp>
#include <nscapi/protobuf/perf_json.hpp>
#include <string>

// One failed-check entry as JSON. Header-only and shared by the two places
// that send an entry off the host - the REST API (/api/v2/failures) and the
// fleet failure report - so a consumer reads both with one parser. Each adds
// what only it needs (REST a local `date` and an `age`, the report a send
// time).
namespace failures_json {

// The Nagios status number, the way the results endpoint reports `status`.
inline int status_number(const PB::Common::ResultCode code) {
  switch (code) {
    case PB::Common::ResultCode::OK:
      return 0;
    case PB::Common::ResultCode::WARNING:
      return 1;
    case PB::Common::ResultCode::CRITICAL:
      return 2;
    default:
      return 3;
  }
}

// The reference to the metrics snapshot an entry was recorded with: its
// state, and - when it has one - which snapshot and how old it was when the
// check failed. The snapshot itself is served on its own, so a listing never
// carries fifty of them.
inline boost::json::object context(const PB::Failures::FailedCheck &entry) {
  const PB::Failures::MetricsContext &context = entry.context();
  boost::json::object node;
  node["state"] = nscapi::failures::context_state_name(context.state());
  if (context.snapshot_id() > 0) {
    node["snapshot_id"] = static_cast<std::int64_t>(context.snapshot_id());
    node["captured_at"] = context.captured_at();
    node["age_at_failure"] = entry.timestamp() > context.captured_at() ? entry.timestamp() - context.captured_at() : 0;
    node["size"] = static_cast<std::int64_t>(context.size());
  }
  return node;
}

// The entry. `arguments` is only emitted when the store records them: an
// entry recorded while it was off carries none, and a client should be able
// to tell "none were given" (an empty list) from "not recorded" (no key).
inline boost::json::object entry(const PB::Failures::FailedCheck &entry, const bool arguments_recorded) {
  const PB::Commands::QueryResponseMessage::Response &r = entry.response();
  boost::json::object node;
  node["index"] = static_cast<std::int64_t>(entry.index());
  // Which problem this is - the command and what it was pointed at - stable
  // across restarts, where `index` is not.
  node["key"] = entry.key();
  node["timestamp"] = entry.timestamp();
  // Who asked: the module that dispatched the query and, when it knows, the
  // principal behind it (`WEBServer:admin`, `NRPEServer`, `Scheduler`).
  node["subject"] = entry.subject();
  // 0 for a check a client asked for; 1+ for one a check ran on its behalf
  // (check_multi, an alias), which is then also listed at depth 0 with the
  // combined result.
  node["depth"] = entry.depth();
  node["command"] = r.command();
  node["alias"] = r.alias();
  node["status"] = status_number(r.result());
  node["result"] = nscapi::failures::status_name(r.result());
  // The first line's text on its own, so a table does not have to dig into
  // `lines` for the one thing it shows per row.
  node["message"] = r.lines_size() > 0 ? r.lines(0).message() : "";
  boost::json::array lines;
  for (const PB::Commands::QueryResponseMessage::Response::Line &l : r.lines()) {
    boost::json::object line;
    line["message"] = l.message();
    line["perf"] = perf_json::to_json(l);
    lines.push_back(line);
  }
  node["lines"] = lines;
  if (arguments_recorded) {
    boost::json::array arguments;
    for (const std::string &a : r.arguments()) arguments.emplace_back(a);
    node["arguments"] = arguments;
  }
  node["context"] = context(entry);
  return node;
}

}  // namespace failures_json
