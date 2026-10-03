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
// It reads the two formats every exporter in the wild serves, and which one a
// body is in is the caller's to say: it is the `Content-Type` the exporter
// answered with, and the two formats disagree on enough that guessing gets
// real bodies wrong.
//
//   * OpenMetrics 1.0 (`application/openmetrics-text; version=1.0.0`). The
//     metadata lines name the family and the samples carry a suffix: a counter
//     `foo` has the samples `foo_total` and `foo_created`, an info family `foo`
//     the sample `foo_info`. The body ends with `# EOF`, and timestamps are
//     seconds.
//   * The Prometheus text format 0.0.4 (`text/plain; version=0.0.4`). The
//     metadata lines name the sample: `# TYPE foo_total counter` declares a
//     family `foo_total` whose sample is `foo_total`. Any `#` line that is not
//     `# HELP` or `# TYPE` is a comment - `# EOF` included - and timestamps are
//     integer milliseconds. A gauge `foo` and a counter `foo_total` are two
//     unrelated families, which client_golang serves by default
//     (`go_memstats_alloc_bytes` beside `go_memstats_alloc_bytes_total`).
//
// Family names are kept exactly as the body declares them; nothing is renamed.
// Both formats keep a family's lines together - metadata first, then its
// samples - so a sample belongs to the family being read, or starts a new one
// of unknown type named after it. A sample of a family that already gave way to
// another, or metadata for a name already declared or sampled, is an error.
//
// One exception to that last rule: client_golang writes a counter `X_total` in
// OpenMetrics as the family `X` (the suffix is stripped from its metadata
// lines), so a Go exporter serves the counter `X` beside any gauge, histogram
// or summary of the same name. Both are kept, under the name `X`. In
// OpenMetrics a family is therefore identified by its name and its type. A
// counter is the only family that may share its name, with exactly one other
// family that declared its type, and only when its own block declares its
// type and carries a sample of its own; the lines after a repeated name are
// read ahead to decide which it is. When they are not such a block, the error
// is the line that repeated the name; a body that ends before they decide
// reads as truncated. Where both own `X_created` (a counter and a histogram or
// summary), only one of them may carry it.
//
// The samples of a histogram or a summary (`_bucket`, `_sum`, `_count`,
// `quantile`) belong to their family rather than starting families of their
// own. `# UNIT` is read in both formats: the Prometheus text format predates it,
// but the agent's own exposition carries it there too.
//
// The body comes off the network. Parsing is one pass that never recurses or
// backtracks, and stops at the first line it cannot read, reporting that line
// and why rather than guessing at what was meant. Families read before it are
// kept, so a caller can choose between discarding the scrape and using what
// arrived; a family whose own metadata line failed, with no samples yet, is
// not. Time is linear in the body for the lines of a family; each family that
// starts costs a few lookups in an ordered index of the names seen so far,
// logarithmic in their number. No hash table is involved whose worst case an
// exporter could choose names to reach.
//
// Memory is the text of the body - a family name is held twice, by its family
// and by the index - plus a fixed overhead for each family, sample and label,
// which `limits` bounds. Measured with libstdc++ on x86-64: a family costs
// about 350 bytes (with its first sample), each further sample about 100, a
// label 64 beyond any text too long for the short-string buffer. With no
// limits, that is about eleven times the body for one of label-heavy lines,
// thirty for one of single-sample families, and fifty when their names are a
// few characters long. The default limits hold the overhead to about 250 MB
// whatever the body.
//
// Exemplars are skipped. What a sample means is not checked - that a
// histogram's buckets are cumulative, that `le` is present - only whether the
// line can be read and which family it belongs to.
namespace metrics {
namespace openmetrics {

enum class format {
  // `application/openmetrics-text; version=1.0.0`.
  openmetrics_1_0,
  // `text/plain; version=0.0.4`, and what a body with no recognisable type is.
  prometheus_text_0_0_4
};

// The format of a body served with this `Content-Type`. Anything that does not
// name OpenMetrics is read as the Prometheus text format, which is what an
// exporter that does not negotiate serves.
format format_for_content_type(const std::string &content_type);

// What a `# TYPE` line declared. `unknown` is OpenMetrics' `unknown` and the
// Prometheus text format's `untyped`, and a family with no `# TYPE` line.
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
  // As served, without conversion: seconds in OpenMetrics, milliseconds in
  // the Prometheus text format. Always finite.
  std::optional<double> timestamp;
};

struct family {
  // The family name as declared: in OpenMetrics without the sample suffix
  // (`foo` for the counter sampled as `foo_total`), in the Prometheus text
  // format the name its samples carry (`foo_total`). With `type`, unique
  // within a result; see above for the one name two families may share.
  std::string name;
  family_type type = family_type::unknown;
  // `# HELP`, unescaped.
  std::string help;
  // `# UNIT`.
  std::string unit;
  std::vector<sample> samples;
};

// Every limit stops the parse at the first line over it and reports an error;
// 0 means no limit. The defaults are far above what any exporter serves -
// node_exporter is about a thousand series, a large kube-state-metrics a few
// hundred thousand - and together hold what a parse can keep to about 250 MB
// beyond the text of the body. A caller with a better idea of its targets
// sets its own.
struct limits {
  // The most samples one body may carry.
  std::size_t max_series = 500000;
  // The most families one body may declare or sample, which is what bounds a
  // body of nothing but metadata lines, or of single-sample families.
  std::size_t max_families = 50000;
  // The most labels one sample may carry.
  std::size_t max_labels_per_sample = 256;
  // The most labels the whole body may carry, across every sample. Each one
  // costs a fixed overhead beyond its text, so this is what bounds a body of
  // short, label-heavy lines.
  std::size_t max_labels = 2500000;
  // The longest line, in bytes, the parser will read. A line over it is
  // reported without being read.
  std::size_t max_line_bytes = 1024 * 1024;
};

struct result {
  // In the order they appear in the body. Every sample name belongs to exactly
  // one family, and no two families share both name and type.
  std::vector<family> families;
  // Total samples across all families.
  std::size_t sample_count = 0;
  // Whether an OpenMetrics body carried its `# EOF` terminator. A body that
  // reads cleanly without it was cut off at a line boundary. Always false for
  // the Prometheus text format, which has no terminator.
  bool saw_eof = false;
  // Empty when the whole body was read. Otherwise why reading stopped, and the
  // 1-based line it stopped on.
  std::string error;
  std::size_t error_line = 0;

  bool ok() const { return error.empty(); }
};

result parse(const std::string &body, format body_format, const limits &bounds = limits());

// The value of the label `name` on `s`, or nothing when the sample does not
// carry it.
std::optional<std::string> find_label(const sample &s, const std::string &name);

}  // namespace openmetrics
}  // namespace metrics
