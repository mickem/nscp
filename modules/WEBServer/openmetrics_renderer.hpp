// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/metrics.hpp>
#include <string>
#include <vector>

// Renders the latched metrics snapshot as a metrics exposition.
//
// This is a pure function of the protobuf message: no settings, no logging, no
// clock. It used to be three lines inside `WEBServer.cpp::build_metrics`,
// appending `<bundle>_<key> <value>` per gauge, which produced a body no
// strict parser accepts - names carrying `.`, `%`, spaces and colons, no
// `# TYPE`, no `# EOF`, and six-significant-digit values (a 16 GB memory
// reading went out as `1.6554e+10`).
//
// Everything a scraper is told about a metric beyond its value comes from the
// producer: `Metric.desc` is the `# HELP` text (falling back to the bundle's
// `desc`), `Metric.unit` the `# UNIT`, `Metric.dims` the labels, and which
// member of the `value` oneof is set is the type. A producer that declares
// none of them still renders, as an anonymous gauge - which is what every
// out-of-tree module does, and what every module in this repository did before
// the metadata sweep.
namespace openmetrics {

// Which of the two expositions to render. They are the same document except
// where the two specifications disagree about naming, which is exactly the
// metadata lines of a counter and of an info family: OpenMetrics names the
// family (`# TYPE foo counter`, sample `foo_total`), the Prometheus text
// format names the sample (`# TYPE foo_total counter`). A body rendered for
// one and served to the other loses the type of every counter, so the endpoint
// renders both and serves whichever matches the `Content-Type` it answers with.
enum class dialect {
  // `application/openmetrics-text; version=1.0.0`. What Prometheus asks for.
  openmetrics_1_0,
  // `text/plain; version=0.0.4`. The older Prometheus text format, and what
  // everything that does not negotiate gets.
  prometheus_text_0_0_4
};

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
//
// Used for label names too, which share the grammar.
std::string sanitize_name(const std::string &raw);

// Render one sample value. Finite values go through `str::render_shortest`
// (integers stay integers, no six-digit truncation); the three non-finite
// values have their own spelling in OpenMetrics.
std::string render_value(double value);

// `# HELP` text: only a backslash and a line feed have to be escaped, and a
// carriage return has no spelling at all, so it is dropped.
std::string escape_help(const std::string &raw);

// A label value, which is a quoted string: backslash, double quote and line
// feed. Live cases are a Windows device path (`\Device\HarddiskVolume1`) and
// an adapter description with a quote in it.
std::string escape_label_value(const std::string &raw);

// Which exposition the request's `Accept` header asks for, and the
// `Content-Type` to answer it with. Kept together so the body served and the
// type declared can never disagree.
dialect dialect_for(const std::string &accept);
std::string content_type_for(const std::string &accept);
std::string content_type_for(dialect dialect);

// Render the whole snapshot. Samples of one family are emitted contiguously
// under a single `# TYPE`, families in the order they were first seen, and the
// body ends with the `# EOF` that OpenMetrics 1.0 requires (and that the older
// format reads as a comment).
//
// `problems` collects one human-readable line per metric that was dropped
// because a different metric already claimed its family name (two keys can
// sanitise to the same name - `foo.bar` and `foo bar`). The caller logs them;
// the renderer keeps the first and never emits a duplicate family.
std::string render(const PB::Metrics::MetricsMessage &response, dialect dialect, std::vector<std::string> *problems = nullptr);

// The exposition as it was emitted before the renderer existed: one
// `<bundle path>_<key> <value>` line per gauge, keys verbatim, values through
// `str::xtos`, strings skipped, no metadata. Kept behind the `openmetrics
// format = legacy` setting so a dashboard built on the old names has a release
// or two to migrate.
std::string render_legacy(const PB::Metrics::MetricsMessage &response);

}  // namespace openmetrics
