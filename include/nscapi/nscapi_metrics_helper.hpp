// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/metrics.hpp>

#include <string>
#include <utility>
#include <vector>

namespace nscapi {
namespace metrics {

// How a producer publishes one metric.
//
// Historically this was `add_metric(bundle, "core 0.idle", 95)` and nothing
// else: the key carried both what the metric *is* (`idle`) and which thing it
// was measured on (`core 0`), because the only consumers - flat JSON, the web
// UI, Graphite, collectd, Python - read a dotted key and have no notion of a
// dimension. That makes a fine tree and a poor time series: every core, NIC,
// drive and process ends up as its own OpenMetrics family, so `sum by (core)`
// has nothing to sum over and a Grafana variable has no label to bind to.
//
// The builder lets a producer say both things at once:
//
//   metric(cpu, "idle").instance("core 0").label("core", "0").gauge(load.idle);
//
// `instance()` composes the key exactly as the concatenation did (`core
// 0.idle`), so every existing consumer sees a byte-identical snapshot, while
// `label()` records the dimension in `Metric::dims` and the bare metric name
// in `Metric::alias`. Only the OpenMetrics renderer reads those two, and it
// renders the call above as one sample of the `system_cpu_idle` family
// carrying `core="0"` - alongside `core="1"`, `core="total"` and whatever else
// the loop produced.
//
// Rebuilding the key *from* the labels was the alternative, and it cannot work
// while Windows spells a core `core 0` and Linux spells it `core_0`: the
// inconsistency would have to leak into the label value, which is the one
// place it must not be. So the key stays authoritative for everyone who reads
// keys, and the labels are additive.
//
// The `add_metric()` overloads below are unchanged and stay the shorthand for
// a metric with no instance and no dimensions - out-of-tree modules keep
// compiling, and a metric that genuinely has no dimension does not need a
// builder to say so.
class metric_builder {
 public:
  metric_builder(PB::Metrics::MetricsBundle *bundle, std::string name);

  // The thing this sample was measured on, as the key has always spelled it:
  // `core 0`, `Ethernet 1`, `C:`, `notepad.exe`. Prepended to the metric name
  // with a `.`, so the resulting key is what concatenation produced before.
  // An empty instance is a no-op, for a producer whose instance is sometimes
  // unnamed (a single battery) and whose key then has no prefix either.
  metric_builder &instance(const std::string &instance);

  // The flat key, spelled out, for the producers that did not compose it as
  // `<instance>.<name>`. A PDH counter is the live case: it publishes
  // `pdh.<counter>.<instance>`, with the instance *last*, so the key cannot be
  // derived from the family name and the instance the way `instance()` does
  // it. Whatever the key is, it stays exactly what it was - that is the point
  // of letting a producer say it rather than inferring it.
  //
  // `instance()` and `key()` both set the same thing, so the last call wins.
  metric_builder &key(const std::string &key);

  // One dimension of this sample. Repeatable; insertion order is the order the
  // renderer emits the labels in. A label with an empty key or an empty value
  // is dropped rather than emitted: an empty label value is indistinguishable
  // from an absent label in OpenMetrics, and a sample carrying one would
  // silently collide with the same sample without it.
  metric_builder &label(const std::string &key, const std::string &value);

  // Terminators. Each writes the value and returns nothing, so a builder
  // expression always ends in exactly one of them.
  void gauge(double value);
  void gauge(long long value);
  void gauge(unsigned long long value);
  // A metric whose value is text - an uptime, a MAC address, a link state.
  // Named for where it is headed rather than for what it does today: the
  // renderer still skips string metrics, because giving them a home means
  // folding a bundle's strings into one `<bundle>_info{...} 1` family, which
  // is metadata work. The labels recorded here are what that family will be
  // keyed on, so setting them now costs nothing and saves a second sweep.
  void info(const std::string &value);

 private:
  PB::Metrics::MetricsBundle *bundle_;
  std::string name_;
  // Empty until a producer composes one; the name is the key when it does not.
  std::string key_;
  // Collected rather than written straight through: the metric is not added to
  // the bundle until a terminator runs, so an abandoned builder leaves no
  // half-filled `Metric` behind.
  std::vector<std::pair<std::string, std::string> > labels_;

  PB::Metrics::Metric *emit() const;
};

// Start building a metric named `name` in `bundle`.
metric_builder metric(PB::Metrics::MetricsBundle *bundle, const std::string &name);

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
std::string core_label(const std::string &cpu_key);

void add_metric(PB::Metrics::MetricsBundle *b, const std::string &key, long long value);
void add_metric(PB::Metrics::MetricsBundle *b, const std::string &key, unsigned long long value);
void add_metric(PB::Metrics::MetricsBundle *b, const std::string &key, std::string value);
void add_metric(PB::Metrics::MetricsBundle *b, const std::string &key, double value);

}  // namespace metrics
}  // namespace nscapi
