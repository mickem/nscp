// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only
#pragma once
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace check_nps {
struct event {
  int id = 0;
  std::string client, policy, reason;
};

inline event make_event(const std::string &provider, int id, const std::string &client_name, const std::string &client_ip,
                        const std::string &network_policy, const std::string &proxy_policy, const std::string &reason) {
  if (provider != "Microsoft-Windows-Security-Auditing") throw std::runtime_error("Unexpected NPS audit provider");
  if (id < 6272 || id > 6275) throw std::runtime_error("Unexpected NPS audit event ID");
  event out;
  out.id = id;
  // Missing grouping fields stay visible; they must not exclude the event.
  out.client = client_name;
  if (out.client.empty() || out.client == "-") out.client = client_ip;
  if (out.client.empty()) out.client = "unknown";
  out.policy = network_policy;
  if (out.policy.empty() || out.policy == "-") out.policy = proxy_policy;
  if (out.policy.empty()) out.policy = "unknown";
  out.reason = reason;
  if (out.reason.empty()) out.reason = out.id == 6272 ? "0" : "unknown";
  return out;
}

struct summary {
  std::string group = "all";
  long long accepted = 0, rejected = 0, discarded = 0, accounting_discards = 0;
  std::map<std::string, long long> reasons;
  long long decisions() const { return accepted + rejected; }
  long long requests() const { return decisions() + discarded; }
  double reject_pct() const { return decisions() ? 100.0 * rejected / decisions() : 0.0; }
  std::string top_reason() const {
    std::string result = "none";
    long long largest = 0;
    for (const auto &entry : reasons) {
      if (entry.second > largest) {
        result = entry.first;
        largest = entry.second;
      }
    }
    return result;
  }
};

inline std::vector<summary> summarize(const std::vector<event> &events, const std::string &group_by) {
  if (group_by != "all" && group_by != "client" && group_by != "policy" && group_by != "reason")
    throw std::runtime_error("group-by must be all, client, policy or reason");
  std::map<std::string, summary> groups;
  if (group_by == "all") groups["all"] = summary{};
  for (const auto &entry : events) {
    const auto key = group_by == "client" ? entry.client : group_by == "policy" ? entry.policy : group_by == "reason" ? entry.reason : "all";
    auto &out = groups[key];
    out.group = key;
    switch (entry.id) {
      case 6272:
        ++out.accepted;
        break;
      case 6273:
        ++out.rejected;
        ++out.reasons[entry.reason];
        break;
      case 6274:
        ++out.discarded;
        ++out.reasons[entry.reason];
        break;
      case 6275:
        ++out.accounting_discards;
        ++out.reasons[entry.reason];
        break;
      default:
        throw std::runtime_error("Unexpected NPS event in aggregation");
    }
  }
  std::vector<summary> out;
  for (const auto &entry : groups) out.push_back(entry.second);
  return out;
}

struct counter_value {
  std::string object, counter, instance;
  double value = 0;
  std::string label() const { return object + "_" + counter + (instance.empty() ? "" : "_" + instance); }
};

// Windows readers throw on unavailable/incomplete data. No invented zeroes.
void require_nps();
std::vector<event> read_events(int seconds, int max_events, bool accounting);
std::vector<counter_value> read_counters(const std::string &object, int sample_ms);
}  // namespace check_nps
