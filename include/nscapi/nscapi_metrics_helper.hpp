// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/dll_defines.hpp>

#include <nscapi/protobuf/metrics.hpp>
#include <string>
#include <utility>
#include <vector>

// How a module declares a metric.
//
// `add_metric` is the original shorthand: a key and a number, typed as a gauge,
// with no description and no unit. It still works and out-of-tree modules can
// keep calling it, but everything it produces is anonymous on the OpenMetrics
// endpoint - a name and a value, with nothing telling a scraper what the value
// means or what it is measured in.
//
// `metric()` is the builder that fills that in:
//
//   metric(mem, "physical.used").help("Physical memory in use").unit("bytes").gauge(used);
//   metric(bundle, "jobs").help("Scheduled jobs executed since start").counter(tasks);
//   metric(section, "uptime").help("Uptime, human readable").info(uptime_str);
//   describe(mem, "Memory as reported by the kernel");   // fallback help for the bundle
//
// A metric measured once per core, NIC, drive, sensor or process says so with
// `instance()` and `label()` instead of pasting the instance into the key:
//
//   metric(cpu, "idle").instance("core 0").label("core", "0").unit("percent").gauge(load.idle);
//
// A producer publishing a whole section for one instance says it once, with
// `for_instance()`, and each metric line is then only about the metric:
//
//   const auto disk = for_instance(section, name, "disk");
//   disk.metric("reads_per_sec").help("Read operations per second").gauge(reads);
//   disk.metric("queue_length").help("Requests queued on the disk").gauge(queued);
//
// `instance()` composes the key exactly as the concatenation did (`core
// 0.idle`), so the flat and nested JSON views, the web UI dashboard, Graphite,
// collectd and Python all see a byte-identical snapshot; `label()` records the
// dimension, and the two together tell the OpenMetrics renderer that this is
// one sample of a `system_cpu_idle` family rather than a family of its own.
// Without them a four-core host publishes four families named after the core,
// so `sum by (core)` has nothing to group on and the family names differ
// between two hosts with different core counts.
//
// Rebuilding the key *from* the labels was the alternative and cannot work
// while Windows spells a core `core 0` and Linux `core_0`: the difference would
// have to leak into the label value, which is the one place it must not be. The
// key stays authoritative for everyone who reads keys, and the labels are
// additive.
//
// A new metric should declare at least `help`, and `unit` whenever the value is
// measured in something (bytes, seconds, percent). The choice of terminal call
// is the metric's type, and it is a real choice: `counter` is for a value that
// only ever grows for the lifetime of the process (jobs run, errors seen), so a
// scraper may `rate()` it across a restart; `gauge` is for anything that can go
// down again. Getting it wrong is not cosmetic - a gauge read as a counter
// produces nonsense at every dip.
namespace nscapi {
namespace metrics {

// The no-metadata shorthand. Kept for out-of-tree modules and for the places
// where a key is all that is known (a PDH counter, a Python script's dict).
NSCAPI_EXPORT void add_metric(PB::Metrics::MetricsBundle *b, const std::string &key, long long value);
NSCAPI_EXPORT void add_metric(PB::Metrics::MetricsBundle *b, const std::string &key, unsigned long long value);
NSCAPI_EXPORT void add_metric(PB::Metrics::MetricsBundle *b, const std::string &key, std::string value);
NSCAPI_EXPORT void add_metric(PB::Metrics::MetricsBundle *b, const std::string &key, double value);

// Accumulates the metadata and appends the metric on the terminal call, so a
// half-built metric can never reach the bundle. Deliberately not reusable: one
// builder is one metric.
class metric_builder {
 public:
  metric_builder(PB::Metrics::MetricsBundle *bundle, std::string name) : bundle_(bundle), name_(std::move(name)) {}

  // One line, no trailing period, describing what the value is. Becomes
  // `# HELP`; a metric without one inherits its bundle's `describe()` text.
  metric_builder &help(std::string text) {
    help_ = std::move(text);
    return *this;
  }
  // What the value is measured in: `bytes`, `seconds`, `percent`, ... Becomes
  // `# UNIT`, and the family name is made to end with it, which is what
  // OpenMetrics requires of a family that declares a unit. Leave it off for a
  // plain count and for a rate (`bytes per second` is not a unit, and a name
  // ending in `_bytes` would lie about what the sample is).
  metric_builder &unit(std::string unit) {
    unit_ = std::move(unit);
    return *this;
  }

  // The thing this sample was measured on, as the key has always spelled it:
  // `core 0`, `Ethernet 1`, `C:`, `notepad.exe`. Prepended to the metric name
  // with a `.`, so the key is what concatenation produced before. An empty
  // instance is a no-op, for a producer whose instance is sometimes unnamed (a
  // single battery), whose key then has no prefix either.
  metric_builder &instance(const std::string &instance) {
    if (!instance.empty()) key_ = instance + "." + name_;
    return *this;
  }

  // The flat key, spelled out, for a producer that did not compose it as
  // `<instance>.<name>`. A PDH counter is the live case: it publishes
  // `pdh.<counter>.<instance>`, with the instance *last*, so the key cannot be
  // derived from the family name and the instance the way `instance()` does it.
  // `instance()` and `key()` set the same thing, so the last call wins.
  metric_builder &key(const std::string &key) {
    key_ = key;
    return *this;
  }

  // One dimension of this sample. Repeatable; insertion order is the order the
  // renderer emits the labels in, and a stable order is what stops a scraper
  // seeing a series rename between two scrapes. A label with an empty key or an
  // empty value is dropped: OpenMetrics treats `x=""` and an absent `x` as the
  // same series, so a sample carrying one would silently collide with a sample
  // that has it filled in - which lets a producer pass a field that is
  // sometimes missing (a NIC with no MAC, a zone with no label file) without
  // guarding every call.
  metric_builder &label(const std::string &key, const std::string &value) {
    if (key.empty() || value.empty()) return *this;
    labels_.push_back(std::make_pair(key, value));
    return *this;
  }

  // A value that can go up and down.
  template <typename T>
  void gauge(const T value) {
    add()->mutable_gauge_value()->set_value(static_cast<double>(value));
  }
  // A value that only grows while the process lives. Rendered with the `_total`
  // suffix the spec reserves for it.
  template <typename T>
  void counter(const T value) {
    add()->mutable_counter_value()->set_value(static_cast<double>(value));
  }
  // A number whose direction is genuinely unknown. Rare - prefer gauge.
  template <typename T>
  void untyped(const T value) {
    add()->mutable_untyped_value()->set_value(static_cast<double>(value));
  }
  // A string: an uptime, a MAC address, a power source. It has no numeric
  // sample, so the OpenMetrics renderer folds it into its bundle's `_info`
  // family as a label instead.
  void info(const std::string &value) { add()->mutable_string_value()->set_value(value); }

 private:
  PB::Metrics::Metric *add() {
    PB::Metrics::Metric *m = bundle_->add_value();
    // The key is what it has always been, instance and all: every consumer
    // other than the OpenMetrics renderer reads it and nothing else.
    m->set_key(key_.empty() ? name_ : key_);
    if (!help_.empty()) m->set_desc(help_);
    if (!unit_.empty()) m->set_unit(unit_);
    if (!labels_.empty()) {
      // `alias` is only meaningful next to `dims`: it names the family the
      // labelled samples share, and without labels to tell those samples apart
      // it would just be an invitation to render several of them as one series.
      // So the two are written together or not at all, and a renderer reading
      // `alias` never has to ask whether the dimensions came with it.
      m->set_alias(name_);
      for (const std::pair<std::string, std::string> &l : labels_) {
        PB::Common::KeyValue *dim = m->add_dims();
        dim->set_key(l.first);
        dim->set_value(l.second);
      }
    }
    return m;
  }

  PB::Metrics::MetricsBundle *bundle_;
  // The metric's name within its bundle - the OpenMetrics family name when the
  // metric carries labels, and the whole key when it does not.
  std::string name_;
  // Empty until a producer composes one; `name_` is the key when it does not.
  std::string key_;
  std::string help_;
  std::string unit_;
  std::vector<std::pair<std::string, std::string> > labels_;
};

inline metric_builder metric(PB::Metrics::MetricsBundle *bundle, std::string name) { return metric_builder(bundle, std::move(name)); }

// The `core` label value for one entry of a CPU-load map, whose keys are
// `total` and `core N` - spelled `core_N` on Linux, which normalises the space
// away before it builds the key, and `core 0` on Windows, which does not.
//
// That difference is a fact about the JSON keys dashboards already read, so it
// stays there; what it must not do is reach the label, where it would make
// `core="core 0"` and `core="core_0"` two different cores depending on which
// host reported them. The label is the bare `0`, on both.
//
// `total` is kept as a label value rather than dropped or split into its own
// family: it mirrors the JSON key, and a query can exclude it explicitly. The
// cost is that `sum without (core)` double-counts, which the documentation
// calls out.
inline std::string core_label(const std::string &cpu_key) {
  static const std::string prefix = "core";
  if (cpu_key.size() <= prefix.size() || cpu_key.compare(0, prefix.size(), prefix) != 0) return cpu_key;
  const char separator = cpu_key[prefix.size()];
  if (separator != ' ' && separator != '_') return cpu_key;
  const std::string core = cpu_key.substr(prefix.size() + 1);
  // `core_` with nothing after it is not a key anything produces, but reducing
  // it to an empty label would drop the label entirely - and a sample missing
  // the label that tells it apart from its siblings collides with them.
  return core.empty() ? cpu_key : core;
}

// One instance's worth of a section: the key prefix and the dimension that
// every metric of this loop iteration shares.
//
// The producers publish these in blocks - eleven disk-IO counters for one
// device, nine WMI fields for one adapter - and spelling
// `.instance(name).label("disk", name)` on each line makes ninety-odd call
// sites mostly punctuation, with the dimension to get wrong ninety times
// instead of once. Saying it once also means a producer whose label value is
// not its key prefix (a Windows CPU socket, a normalised core number) states
// that difference in one place.
class instance_scope {
 public:
  instance_scope(PB::Metrics::MetricsBundle *bundle, std::string instance, std::string label, std::string value)
      : bundle_(bundle), instance_(std::move(instance)), label_(std::move(label)), value_(std::move(value)) {}

  // One metric of this instance, ready for its help, unit and value.
  metric_builder metric(const std::string &name) const { return nscapi::metrics::metric(bundle_, name).instance(instance_).label(label_, value_); }

 private:
  PB::Metrics::MetricsBundle *bundle_;
  std::string instance_;
  std::string label_;
  std::string value_;
};

// Publish a section's metrics for one instance. `instance` is the key prefix,
// exactly as the concatenation spelled it, and `label` is the dimension name.
inline instance_scope for_instance(PB::Metrics::MetricsBundle *bundle, std::string instance, std::string label) {
  std::string value = instance;
  return instance_scope(bundle, std::move(instance), std::move(label), std::move(value));
}

// The same, where the label value is not the key prefix. Two producers need
// it: a CPU-load key is `core 0` where the label is the bare `0`, and a
// Windows processor's key is its model name where the dimension that actually
// tells two sockets apart is the device id.
inline instance_scope for_instance(PB::Metrics::MetricsBundle *bundle, std::string instance, std::string label, std::string value) {
  return instance_scope(bundle, std::move(instance), std::move(label), std::move(value));
}

// Help text for a whole bundle, used for every metric in it that declares none
// of its own. The cheap way to give a section of near-identical metrics (one
// per CPU core, one per NIC) a description without repeating it per metric.
inline void describe(PB::Metrics::MetricsBundle *bundle, const std::string &desc) { bundle->set_desc(desc); }

// The numeric sample of a metric, whatever type it was declared as, for the
// consumers that forward numbers and do not care about the distinction (the
// JSON views, Graphite, Elastic, the Python dict). Without it every one of them
// silently drops whatever a producer declares as a counter, which is a metric
// disappearing from a dashboard for no reason its owner could see.
// Returns false for a string metric and for the aggregate types (summary,
// histogram), which have no single value to hand over.
inline bool numeric_value(const PB::Metrics::Metric &metric, double &value) {
  if (metric.has_gauge_value()) {
    value = metric.gauge_value().value();
    return true;
  }
  if (metric.has_counter_value()) {
    value = metric.counter_value().value();
    return true;
  }
  if (metric.has_untyped_value()) {
    value = metric.untyped_value().value();
    return true;
  }
  return false;
}

}  // namespace metrics
}  // namespace nscapi
