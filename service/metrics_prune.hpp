// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <cstddef>
#include <cstdio>
#include <map>
#include <nscapi/nscapi_metrics_helper.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <string>
#include <utility>
#include <vector>

namespace nsclient {
namespace core {

// Trimming a metrics snapshot before the failed-check log keeps it
// ([/settings/core/failed checks] `prune metrics`). A busy Windows host
// publishes a few hundred values, and most of what a failure report carries
// is noise: a dozen idle WAN miniports with every rate at 0, a `_Total` disk
// that is the only disk, network adapters with no MAC and no speed. Each rule
// is opt-in:
//
//   zeros       a numeric value (gauge, counter, untyped) that is exactly 0
//   empty       a string value that is empty
//   duplicates  an instance whose every value equals an earlier instance's in
//               the same section (`_Total` and the one disk it totals, eight
//               idle adapters); the first is kept and the rest are listed
//
// and, from `prune pattern`, a list of text to exclude: any metric whose flat
// key (`system.network.Hyper-V Virtual Ethernet Adapter.Speed`) contains one
// of them, ignoring the case of ASCII letters. That is how an operator drops
// the instances they know are noise on their hosts - a VirtualBox adapter,
// an unmounted volume - and anything else they would rather not ship.
// Exclusion runs first, so an excluded instance is never the one a duplicate
// is collapsed into.
//
// An instance is what a producer declared with `for_instance()`: metrics in
// one bundle carrying the same labels, keyed `<instance>.<alias>`. A metric
// without labels is never collapsed, because there is nothing to say which
// other one it would be a copy of. Duplicates are found on the values as
// collected, before zeros and empties are removed, so two instances only
// collapse when they were identical, not when they merely look alike after
// trimming.
//
// Only the copy the log keeps is pruned. `/api/v2/metrics` and the
// submitters were handed the full snapshot before this runs.
struct metrics_prune {
  struct rules {
    bool zeros = false;
    bool empty = false;
    bool duplicates = false;
    // Lower-cased (ASCII) text a key must not contain.
    std::vector<std::string> patterns;
    bool any() const { return zeros || empty || duplicates || !patterns.empty(); }
  };

  // What was taken out. `collapsed` maps each dropped instance (its full
  // flat-key prefix) to the instance it duplicated, so a reader can still tell
  // that `disk.io.C:` existed and read its values off `disk.io._Total`.
  struct summary {
    std::size_t zeros = 0;
    std::size_t empty = 0;
    std::size_t duplicates = 0;
    std::size_t excluded = 0;
    std::vector<std::pair<std::string, std::string>> collapsed;
    bool any() const { return zeros > 0 || empty > 0 || duplicates > 0 || excluded > 0; }
  };

  // A comma-separated list of zeros, empty and duplicates; empty, or `none`,
  // turns pruning off. The patterns in `out` are kept.
  static bool parse(const std::string &text, rules &out, std::string &error) {
    rules parsed;
    parsed.patterns = out.patterns;
    std::string::size_type start = 0;
    while (start <= text.size()) {
      std::string::size_type end = text.find(',', start);
      if (end == std::string::npos) end = text.size();
      std::string token = text.substr(start, end - start);
      boost::algorithm::trim(token);
      boost::algorithm::to_lower(token);
      start = end + 1;
      if (token.empty() || token == "none") continue;
      if (token == "zeros" || token == "zero")
        parsed.zeros = true;
      else if (token == "empty")
        parsed.empty = true;
      else if (token == "duplicates" || token == "duplicate")
        parsed.duplicates = true;
      else {
        error = "'" + token + "' is not a pruning rule (expected a list of zeros, empty, duplicates)";
        return false;
      }
    }
    out = parsed;
    return true;
  }

  // A comma-separated list of text to exclude (`prune pattern`). Blanks
  // around each entry are trimmed and empty entries skipped; anything else is
  // a pattern, so this cannot fail.
  static std::vector<std::string> parse_patterns(const std::string &text) {
    std::vector<std::string> out;
    std::string::size_type start = 0;
    while (start <= text.size()) {
      std::string::size_type end = text.find(',', start);
      if (end == std::string::npos) end = text.size();
      std::string token = text.substr(start, end - start);
      boost::algorithm::trim(token);
      start = end + 1;
      if (!token.empty()) out.push_back(ascii_lower(token));
    }
    return out;
  }

  // The canonical spelling, as parse reads it.
  static std::string format(const rules &value) {
    std::string out;
    if (value.zeros) out += "zeros";
    if (value.empty) out += (out.empty() ? "" : ",") + std::string("empty");
    if (value.duplicates) out += (out.empty() ? "" : ",") + std::string("duplicates");
    return out;
  }

  static summary apply(PB::Metrics::MetricsMessage &message, const rules &value) {
    summary result;
    if (!value.any()) return result;
    for (PB::Metrics::MetricsMessage::Response &payload : *message.mutable_payload()) {
      for (PB::Metrics::MetricsBundle &bundle : *payload.mutable_bundles()) {
        apply_bundle(bundle, bundle.key(), value, result);
      }
    }
    return result;
  }

 private:
  // Only A-Z: a locale-aware lower-casing would differ between hosts, and
  // the keys are UTF-8 that a byte-wise one would mangle.
  static std::string ascii_lower(std::string text) {
    for (char &c : text) {
      if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return text;
  }

  static bool is_excluded(const std::string &flat_key, const rules &value) {
    if (value.patterns.empty()) return false;
    const std::string key = ascii_lower(flat_key);
    for (const std::string &pattern : value.patterns) {
      if (key.find(pattern) != std::string::npos) return true;
    }
    return false;
  }

  // The prefix a for_instance() metric was keyed under, or empty when the
  // metric is not one.
  static std::string instance_prefix(const PB::Metrics::Metric &metric) {
    if (metric.dims_size() == 0 || metric.alias().empty()) return "";
    const std::string &key = metric.key();
    const std::string suffix = "." + metric.alias();
    if (key.size() <= suffix.size() || key.compare(key.size() - suffix.size(), suffix.size(), suffix) != 0) return "";
    return key.substr(0, key.size() - suffix.size());
  }

  // One value, exactly: the type, and the number at full precision or the
  // string as it is.
  static std::string value_signature(const PB::Metrics::Metric &metric) {
    double number = 0;
    if (nscapi::metrics::numeric_value(metric, number)) {
      char buffer[40];
      std::snprintf(buffer, sizeof(buffer), "%.17g", number);
      const char *type = metric.has_counter_value() ? "c" : metric.has_untyped_value() ? "u" : "g";
      return std::string(type) + buffer;
    }
    if (metric.has_string_value()) return "s" + metric.string_value().value();
    // A summary or histogram: compared by its serialised form.
    return "x" + metric.SerializeAsString();
  }

  static bool is_zero(const PB::Metrics::Metric &metric) {
    double number = 0;
    return nscapi::metrics::numeric_value(metric, number) && number == 0;
  }

  static bool is_empty(const PB::Metrics::Metric &metric) { return metric.has_string_value() && metric.string_value().value().empty(); }

  static void apply_bundle(PB::Metrics::MetricsBundle &bundle, const std::string &trail, const rules &value, summary &result) {
    for (PB::Metrics::MetricsBundle &child : *bundle.mutable_children()) {
      apply_bundle(child, trail + "." + child.key(), value, result);
    }

    // Excluded first, so what is left is all the duplicate check looks at.
    if (!value.patterns.empty()) {
      google::protobuf::RepeatedPtrField<PB::Metrics::Metric> remaining;
      for (PB::Metrics::Metric &metric : *bundle.mutable_value()) {
        if (is_excluded(trail + "." + metric.key(), value)) {
          ++result.excluded;
          continue;
        }
        remaining.Add()->Swap(&metric);
      }
      bundle.mutable_value()->Swap(&remaining);
    }

    // Instances that duplicate an earlier one, by prefix.
    std::map<std::string, bool> dropped;
    if (value.duplicates) {
      // Each instance's values, keyed by alias so the order a producer
      // published them in does not matter, in the order instances first
      // appear.
      std::vector<std::string> order;
      std::map<std::string, std::map<std::string, std::string>> values;
      for (const PB::Metrics::Metric &metric : bundle.value()) {
        const std::string prefix = instance_prefix(metric);
        if (prefix.empty()) continue;
        if (values.find(prefix) == values.end()) order.push_back(prefix);
        values[prefix][metric.alias()] = value_signature(metric);
      }
      std::map<std::map<std::string, std::string>, std::string> first;
      for (const std::string &prefix : order) {
        const std::pair<std::map<std::map<std::string, std::string>, std::string>::iterator, bool> seen = first.insert(std::make_pair(values[prefix], prefix));
        if (!seen.second) {
          dropped[prefix] = true;
          result.collapsed.push_back(std::make_pair(trail + "." + prefix, trail + "." + seen.first->second));
        }
      }
    }

    google::protobuf::RepeatedPtrField<PB::Metrics::Metric> kept;
    for (PB::Metrics::Metric &metric : *bundle.mutable_value()) {
      if (!dropped.empty() && dropped.find(instance_prefix(metric)) != dropped.end()) {
        ++result.duplicates;
        continue;
      }
      if (value.zeros && is_zero(metric)) {
        ++result.zeros;
        continue;
      }
      if (value.empty && is_empty(metric)) {
        ++result.empty;
        continue;
      }
      kept.Add()->Swap(&metric);
    }
    bundle.mutable_value()->Swap(&kept);
  }
};

}  // namespace core
}  // namespace nsclient
