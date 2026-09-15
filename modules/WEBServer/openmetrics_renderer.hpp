// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/metrics.hpp>
#include <string>
#include <vector>

// Renders the latched metrics snapshot as an OpenMetrics text exposition.
//
// This is a pure function of the protobuf message: no settings, no logging, no
// clock. It used to be three lines inside `WEBServer.cpp::build_metrics`,
// appending `<bundle>_<key> <value>` per gauge, which produced a body no
// strict parser accepts - names carrying `.`, `%`, spaces and colons, no
// `# TYPE`, no `# EOF`, and six-significant-digit values (a 16 GB memory
// reading went out as `1.6554e+10`).
//
// A metric that a producer built through `nscapi::metrics::metric()` with an
// instance and one or more labels arrives carrying `Metric::alias` (the family
// name, with the instance taken back out of it) and `Metric::dims` (the
// dimensions). Those samples group: one `# TYPE` for `system_cpu_idle` and one
// sample per core under it, rather than a family per core. A metric with no
// dims renders from its key exactly as before, so a producer that has not been
// swept, or an out-of-tree module, is unaffected.
//
// What is deliberately *not* here yet: `# HELP`, `# UNIT`, counters and info
// families all need metadata that no producer sets today. Those arrive with
// the metrics-metadata work.
//
// One consequence of typing everything as a gauge: a key that already ends in
// `total` or `count` (`system.network.eth0.total`, `system.os_updates.count`)
// produces a gauge family whose name carries a suffix OpenMetrics reserves for
// counters and summaries. Prometheus reads it, and `promtool check metrics`
// warns about it; typing those metrics as counters is what fixes it, and that
// is the metadata work rather than something to paper over with a rename the
// same work would undo.
namespace openmetrics {

// Map one protobuf key or bundle key onto the OpenMetrics name grammar
// (`[a-zA-Z_][a-zA-Z0-9_]*` - colons are reserved for recording rules and are
// not emitted by an exporter):
//
//   * `%` becomes `percent`, so `system.mem.physical.%` reads
//     `system_mem_physical_percent` - which is the name the documentation has
//     always shown, and the code never produced.
//   * anything outside `[a-zA-Z0-9_]` becomes `_`, and a run of them collapses
//     to a single `_`, which is exactly the `metric_relabel_configs` rewrite
//     the Prometheus scenario page tells operators to write by hand today.
//   * a name that would not start with a letter borrows a `metric_` prefix: a
//     name has to start with a letter or an underscore, and OpenMetrics
//     separately reserves every name *beginning* with an underscore, so `_`
//     would only trade one non-conformance for another.
std::string sanitize_name(const std::string &raw);

// The same mapping for a label name, which shares the metric-name grammar.
// Producers use fixed, already-legal names; what needs cleaning is the
// operator-defined end - a PDH counter's instance dimension, or the `labels`
// dict a Python script returns. The borrowed prefix is `label_` rather than
// `metric_` so a name that needed rescuing says what it is.
std::string sanitize_label_name(const std::string &raw);

// Escape a label value for the exposition. Only three characters have to be
// spelled differently (`\\`, `\"` and a newline as `\n`) and all three turn up
// in real values: a Windows volume reads `\Device\HarddiskVolume1`, and a WMI
// adapter description can carry a quote.
std::string escape_label_value(const std::string &raw);

// Render one sample value. Finite values go through `str::render_shortest`
// (integers stay integers, no six-digit truncation); the three non-finite
// values have their own spelling in OpenMetrics.
std::string render_value(double value);

// The `Content-Type` to answer a scrape carrying this `Accept` header with.
// The body is the same either way - the Prometheus text parser treats `# UNIT`
// and `# EOF` as ordinary comments - so this is purely about letting a client
// that asked for OpenMetrics 1.0 see that it got it.
std::string content_type_for(const std::string &accept);

// Render the whole snapshot. Samples of one family are emitted contiguously
// under a single `# TYPE`, families in the order they were first seen, and the
// body ends with the `# EOF` that OpenMetrics 1.0 requires.
//
// `problems` collects one human-readable line per metric that was dropped
// because a different metric already claimed its series - either the family
// name, since two keys can sanitise to the same one (`foo.bar` and `foo bar`),
// or the family name together with an identical label set. The caller logs
// them; the renderer keeps the first and never emits a duplicate series.
std::string render(const PB::Metrics::MetricsMessage &response, std::vector<std::string> *problems = nullptr);

// The exposition as it was emitted before the renderer existed: one
// `<bundle path>_<key> <value>` line per gauge, keys verbatim, values through
// `str::xtos`, strings skipped, no metadata. Kept behind the `openmetrics
// format = legacy` setting so a dashboard built on the old names has a release
// or two to migrate.
std::string render_legacy(const PB::Metrics::MetricsMessage &response);

}  // namespace openmetrics
