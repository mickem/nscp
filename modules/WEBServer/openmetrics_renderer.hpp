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
// What is deliberately *not* here yet: `# HELP`, `# UNIT`, counters and info
// families all need metadata that no producer sets today, and labels need the
// producers to say which part of a key is an instance. Those arrive with the
// metrics-metadata work, and bring with them the escaping helpers and the
// one-family-many-samples shape that only labels make reachable.
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
// because a different metric already claimed its family name (two keys can
// sanitise to the same name - `foo.bar` and `foo bar`). The caller logs them;
// the renderer keeps the first and never emits a duplicate family.
std::string render(const PB::Metrics::MetricsMessage &response, std::vector<std::string> *problems = nullptr);

// The exposition as it was emitted before the renderer existed: one
// `<bundle path>_<key> <value>` line per gauge, keys verbatim, values through
// `str::xtos`, strings skipped, no metadata. Kept behind the `openmetrics
// format = legacy` setting so a dashboard built on the old names has a release
// or two to migrate.
std::string render_legacy(const PB::Metrics::MetricsMessage &response);

}  // namespace openmetrics
