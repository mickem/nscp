// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// Parses a metrics exposition - what an exporter serves on `/metrics` - into
// families and samples.
//
// It reads both dialects the WEBServer renderer writes, because those are the
// two every exporter in the wild serves:
//
//   * OpenMetrics 1.0 (`application/openmetrics-text; version=1.0.0`), where
//     the metadata lines name the family (`# TYPE foo counter`, sample
//     `foo_total`) and the body ends with `# EOF`;
//   * the Prometheus text format 0.0.4 (`text/plain; version=0.0.4`), where
//     the metadata lines name the sample (`# TYPE foo_total counter`), `# EOF`
//     does not exist and arbitrary `#` comments are allowed.
//
// The two are folded onto one model: a counter family is always named without
// its `_total`, and the `_bucket`, `_sum`, `_count` and `_created` samples of
// a histogram or a summary belong to that family rather than starting families
// of their own. That is the same shape the renderer starts from, which is what
// lets a scraped family be republished under the agent's own bundle.
//
// The body comes off the network, so the parser is a pure function of its
// input with every axis bounded: it never recurses, never backtracks, and stops
// at the first line it cannot read, reporting which one and why rather than
// guessing at what was meant. Families read before that line are kept, so a
// caller can choose between discarding the scrape and using what arrived.
//
// Exemplars are skipped. No validation is made of what a sample means - that a
// histogram's buckets are cumulative, that `le` is present - only of whether
// the line can be read.
namespace metrics {
namespace openmetrics {

// What a `# TYPE` line declared. `unknown` covers the Prometheus `untyped`,
// and a family that never had a `# TYPE` line at all.
enum class family_type { unknown, counter, gauge, histogram, gaugehistogram, summary, info, stateset };

// The word the OpenMetrics 1.0 specification uses for the type.
const char *type_name(family_type type);

// Label name/value pairs in the order the exporter served them. A vector rather
// than a map because that order is what an operator sees in the exposition,
// and a sample rarely carries more than a handful.
typedef std::vector<std::pair<std::string, std::string> > label_list;

struct sample {
  // The sample name exactly as served: `foo_total`, `foo_bucket`, `foo`.
  std::string name;
  label_list labels;
  // `NaN`, `+Inf` and `-Inf` are preserved.
  double value = 0;
  // As served, without conversion: OpenMetrics writes seconds, the Prometheus
  // text format milliseconds, and nothing in the body says which it is.
  std::optional<double> timestamp;
};

struct family {
  // The family name: for a counter without `_total`, for a histogram or a
  // summary without the sample suffix.
  std::string name;
  family_type type = family_type::unknown;
  // `# HELP`, unescaped.
  std::string help;
  // `# UNIT`.
  std::string unit;
  std::vector<sample> samples;
};

struct limits {
  // The most samples one body may carry. A body with more is cut off at the
  // first sample over the limit and reported as an error. 0 means no limit.
  std::size_t max_series = 0;
  // The longest line, in bytes, the parser will read. A line over it is
  // reported without being read. 0 means no limit.
  std::size_t max_line_bytes = 0;
};

struct result {
  std::vector<family> families;
  // Total samples across all families.
  std::size_t sample_count = 0;
  // Whether the body carried the `# EOF` terminator. Only OpenMetrics 1.0
  // writes it, so its absence is a sign of truncation only when the exporter
  // answered with that content type.
  bool saw_eof = false;
  // Empty when the whole body was read. Otherwise why reading stopped, and the
  // 1-based line it stopped on.
  std::string error;
  std::size_t error_line = 0;

  bool ok() const { return error.empty(); }
};

result parse(const std::string &body, const limits &bounds = limits());

// The value of the label `name` on `s`, or nothing when the sample does not
// carry it.
std::optional<std::string> find_label(const sample &s, const std::string &name);

}  // namespace openmetrics
}  // namespace metrics
