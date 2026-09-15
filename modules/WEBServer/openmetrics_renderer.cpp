// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "openmetrics_renderer.hpp"

#include <cmath>
#include <map>
#include <set>
#include <str/number_format.hpp>
#include <str/xtos.hpp>

namespace openmetrics {

namespace {

// What a producer said the metric is, by which member of the `value` oneof it
// set. Not the same word in the two expositions, and not the same family name
// either, which is why the renderer carries the type around rather than the
// word.
enum class metric_type { gauge, counter, unknown, info, summary, histogram };

// The suffix the spec gives the *sample* of a family of this type. A counter
// family `foo` has the sample `foo_total`; an info family `foo` has `foo_info`;
// a summary and a histogram spread over several, so theirs sit on the samples.
std::string sample_suffix(const metric_type type) {
  if (type == metric_type::counter) return "_total";
  if (type == metric_type::info) return "_info";
  return "";
}

const char *type_word(const metric_type type, const dialect d) {
  switch (type) {
    case metric_type::counter:
      return "counter";
    case metric_type::unknown:
      // The two formats spell the "we do not know" type differently, and the
      // wrong word is a parse error rather than a shrug.
      return d == dialect::openmetrics_1_0 ? "unknown" : "untyped";
    case metric_type::info:
      // `info` does not exist before OpenMetrics 1.0; the older format would
      // reject the whole body over it. A gauge that is always 1 is what every
      // exporter emitted before the type existed (`node_uname_info`), and it
      // carries exactly the same information.
      return d == dialect::openmetrics_1_0 ? "info" : "gauge";
    case metric_type::summary:
      return "summary";
    case metric_type::histogram:
      return "histogram";
    case metric_type::gauge:
      break;
  }
  return "gauge";
}

// `# HELP`, `# TYPE` and `# UNIT` name the family in OpenMetrics and the sample
// in the Prometheus text format. For a gauge those are the same string, which
// is why this only ever matters for counters and info families.
std::string metadata_suffix(const metric_type type, const dialect d) {
  if (d == dialect::openmetrics_1_0) return "";
  return sample_suffix(type);
}

struct sample {
  // Appended to the family name for this sample: `_total`, `_info`, `_sum`,
  // `_count`, `_bucket`, or nothing.
  std::string suffix;
  // Already rendered and escaped, `{a="1",b="2"}` or empty.
  std::string labels;
  std::string value;
};

typedef std::vector<std::pair<std::string, std::string> > label_list;

// One family, collected before anything is written: both expositions require
// the samples of a family to be contiguous under a single `# TYPE`, which the
// flat list of lines this replaces could not promise once two producers
// interleave. A family holds one sample per label set - one per CPU core, one
// per NIC - and `series` is what keeps a second metric from landing on a label
// set that is already there, which would be the same series twice.
struct family {
  std::string name;
  metric_type type = metric_type::gauge;
  std::string help;
  std::string unit;
  std::vector<sample> samples;
  std::set<std::string> series;
};

struct family_set {
  static constexpr size_t none = static_cast<size_t>(-1);

  std::vector<family> families;
  // Family name -> index into `families`, so a second instance of a family
  // appends a sample instead of starting a new one, and the emitted order
  // stays first-seen rather than alphabetical.
  std::map<std::string, size_t> index;
  // Every name this document will write, whether as a family or as a sample.
  // Sanitising is lossy, so two distinct keys can land on one name (`foo.bar`
  // and `foo bar`, or `mem.%` and `mem.percent`), and a counter family `foo`
  // reserves `foo_total` as well as `foo`, which a gauge of that name must not
  // then take.
  std::set<std::string> claimed;

  // Reserves every name a family of this shape would write. Returns false, and
  // leaves nothing behind, when any of them is taken.
  bool claim(const std::string &name, const metric_type type, const std::vector<std::string> &suffixes) {
    std::set<std::string> wanted;
    wanted.insert(name);
    wanted.insert(name + sample_suffix(type));
    for (const std::string &suffix : suffixes) wanted.insert(name + suffix);
    for (const std::string &candidate : wanted) {
      if (claimed.find(candidate) != claimed.end()) return false;
    }
    claimed.insert(wanted.begin(), wanted.end());
    return true;
  }

  // The index of the family under `name`, created on first use. `none` when
  // the name belongs to something else already - the caller reports that and
  // drops the metric, rather than emitting a second series of a name a strict
  // parser would then reject the whole body over. An index rather than a
  // pointer because opening the next family reallocates the vector.
  size_t open(const std::string &name, const metric_type type, const std::string &help, const std::string &unit, const std::vector<std::string> &suffixes) {
    const std::map<std::string, size_t>::const_iterator existing = index.find(name);
    if (existing != index.end()) {
      // Same name, different shape: one `# TYPE` (or one `# UNIT`) cannot
      // describe both, so the second metric has nowhere to go.
      if (families[existing->second].type != type || families[existing->second].unit != unit) return none;
      return existing->second;
    }
    if (!claim(name, type, suffixes)) return none;
    family added;
    added.name = name;
    added.type = type;
    added.help = help;
    added.unit = unit;
    index[name] = families.size();
    families.push_back(added);
    return families.size() - 1;
  }
};

// OpenMetrics requires the name of a family that declares a unit to end with
// that unit, so a producer only has to say `bytes` once and the suffix follows.
// A key that already ends in it (`system.mem.physical.%` sanitised to
// `..._percent`, a frequency in `..._mhz`) keeps the name it had.
std::string with_unit(const std::string &name, const std::string &unit) {
  if (unit.empty()) return name;
  const std::string suffix = "_" + unit;
  if (name.size() >= suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) return name;
  return name + suffix;
}

std::string render_labels(const label_list &labels) {
  if (labels.empty()) return "";
  std::string ret = "{";
  for (size_t i = 0; i < labels.size(); ++i) {
    if (i != 0) ret += ",";
    ret += labels[i].first + "=\"" + escape_label_value(labels[i].second) + "\"";
  }
  return ret + "}";
}

label_list dims_of(const PB::Metrics::Metric &metric) {
  label_list ret;
  for (const PB::Common::KeyValue &dim : metric.dims()) {
    if (dim.key().empty()) continue;
    ret.push_back(std::make_pair(sanitize_name(dim.key()), dim.value()));
  }
  return ret;
}

// The string metrics of one bundle, gathered while walking it: they have no
// numeric sample of their own, so they become labels of the bundle's `_info`
// family - the `node_uname_info` shape, one series carrying the strings that
// describe a thing rather than measure it. Strings sharing a label set share a
// series, so one NIC's MAC address and status end up on one line.
struct info_series {
  std::string dims_key;
  label_list labels;
  std::string help;
};

metric_type type_of(const PB::Metrics::Metric &metric) {
  if (metric.has_counter_value()) return metric_type::counter;
  if (metric.has_untyped_value()) return metric_type::unknown;
  if (metric.has_string_value()) return metric_type::info;
  if (metric.has_summary_value()) return metric_type::summary;
  if (metric.has_histogram_value()) return metric_type::histogram;
  return metric_type::gauge;
}

void add_numeric(family &target, const std::string &labels, const double value) {
  sample added;
  added.suffix = sample_suffix(target.type);
  added.labels = labels;
  added.value = render_value(value);
  target.samples.push_back(added);
}

void add_summary(family &target, const label_list &labels, const std::string &rendered, const PB::Metrics::Summary &summary) {
  for (const PB::Metrics::Quantile &quantile : summary.quantile()) {
    label_list with_quantile = labels;
    with_quantile.push_back(std::make_pair("quantile", render_value(quantile.quantile())));
    sample added;
    added.labels = render_labels(with_quantile);
    added.value = render_value(quantile.value());
    target.samples.push_back(added);
  }
  sample sum;
  sum.suffix = "_sum";
  sum.labels = rendered;
  sum.value = render_value(summary.sample_sum());
  target.samples.push_back(sum);
  sample count;
  count.suffix = "_count";
  count.labels = rendered;
  count.value = render_value(static_cast<double>(summary.sample_count()));
  target.samples.push_back(count);
}

void add_histogram(family &target, const label_list &labels, const std::string &rendered, const PB::Metrics::Histogram &histogram) {
  for (const PB::Metrics::Bucket &bucket : histogram.bucket()) {
    label_list with_le = labels;
    with_le.push_back(std::make_pair("le", render_value(bucket.upper_bound())));
    sample added;
    added.suffix = "_bucket";
    added.labels = render_labels(with_le);
    added.value = render_value(static_cast<double>(bucket.cumulative_count()));
    target.samples.push_back(added);
  }
  sample sum;
  sum.suffix = "_sum";
  sum.labels = rendered;
  sum.value = render_value(histogram.sample_sum());
  target.samples.push_back(sum);
  sample count;
  count.suffix = "_count";
  count.labels = rendered;
  count.value = render_value(static_cast<double>(histogram.sample_count()));
  target.samples.push_back(count);
}

// Which sample suffixes a family of this type will write, so the collision
// check can reserve all of them up front.
std::vector<std::string> suffixes_of(const metric_type type) {
  std::vector<std::string> ret;
  if (type == metric_type::summary || type == metric_type::histogram) {
    ret.push_back("_sum");
    ret.push_back("_count");
    if (type == metric_type::histogram) ret.push_back("_bucket");
  }
  return ret;
}

void report(std::vector<std::string> *problems, const std::string &trail, const std::string &key, const std::string &name, const std::string &why) {
  if (problems == nullptr) return;
  problems->push_back("Dropping metric '" + trail + "." + key + "' from the OpenMetrics exposition: it renders as '" + name + "' and " + why + ".");
}

// Folds one string metric into the bundle's info series that carries its label
// set, creating that series on first use.
void add_info(std::vector<info_series> &info, const label_list &dims, const std::string &label_name, const std::string &value, const std::string &help) {
  std::string dims_key;
  for (const std::pair<std::string, std::string> &dim : dims) dims_key += dim.first + "=" + dim.second + "\x1f";
  size_t at = 0;
  while (at < info.size() && info[at].dims_key != dims_key) ++at;
  if (at == info.size()) {
    info_series added;
    added.dims_key = dims_key;
    added.labels = dims;
    added.help = help;
    info.push_back(added);
  }
  if (info[at].help.empty()) info[at].help = help;
  info[at].labels.push_back(std::make_pair(label_name, value));
}

bool has_label(const label_list &labels, const std::string &name) {
  for (const std::pair<std::string, std::string> &label : labels) {
    if (label.first == name) return true;
  }
  return false;
}

const info_series *find_series(const std::vector<info_series> &info, const label_list &dims) {
  std::string dims_key;
  for (const std::pair<std::string, std::string> &dim : dims) dims_key += dim.first + "=" + dim.second + "\x1f";
  for (const info_series &series : info) {
    if (series.dims_key == dims_key) return &series;
  }
  return nullptr;
}

void collect(const PB::Metrics::MetricsBundle &bundle, const std::string &trail, family_set &out, std::vector<std::string> *problems) {
  for (const PB::Metrics::MetricsBundle &child : bundle.children()) {
    collect(child, trail + "_" + child.key(), out, problems);
  }

  // The bundle's strings, gathered in first-seen order and folded into one
  // `_info` family once the bundle is walked, so a bundle contributes at most
  // one of them however many strings it holds.
  std::vector<info_series> info;

  for (const PB::Metrics::Metric &metric : bundle.value()) {
    const metric_type type = type_of(metric);
    const label_list labels = dims_of(metric);
    // `alias` is the family name relative to the bundle when the key embeds an
    // instance that `dims` carries instead; an empty alias means the key is
    // the name, which is every metric that has no instance.
    const std::string leaf = metric.alias().empty() ? metric.key() : metric.alias();
    // A metric with no help of its own inherits the bundle's description,
    // which is how a section of near-identical metrics (one per core, one per
    // NIC) gets help text without repeating it on every metric.
    const std::string help = metric.desc().empty() ? bundle.desc() : metric.desc();

    if (type == metric_type::info) {
      const std::string label_name = sanitize_name(leaf);
      const info_series *existing = find_series(info, labels);
      if (existing != nullptr && has_label(existing->labels, label_name)) {
        report(problems, trail, metric.key(), label_name, "another string metric of this bundle already claimed that label");
        continue;
      }
      add_info(info, labels, label_name, metric.string_value().value(), help);
      continue;
    }

    // Sanitise the joined path in one pass rather than per segment: that is
    // what collapses a run spanning a separator (`disk.io.` + `C:` would leave
    // `disk_io__C_` if each part were cleaned on its own).
    const std::string name = with_unit(sanitize_name(trail + "_" + leaf), metric.unit());
    const size_t at = out.open(name, type, help, metric.unit(), suffixes_of(type));
    if (at == family_set::none) {
      report(problems, trail, metric.key(), name, "another metric already claimed that name");
      continue;
    }
    family &target = out.families[at];
    const std::string rendered = render_labels(labels);
    if (!target.series.insert(rendered).second) {
      report(problems, trail, metric.key(), name, "another metric already claimed that name");
      continue;
    }
    switch (type) {
      case metric_type::counter:
        add_numeric(target, rendered, metric.counter_value().value());
        break;
      case metric_type::unknown:
        add_numeric(target, rendered, metric.untyped_value().value());
        break;
      case metric_type::summary:
        add_summary(target, labels, rendered, metric.summary_value());
        break;
      case metric_type::histogram:
        add_histogram(target, labels, rendered, metric.histogram_value());
        break;
      default:
        add_numeric(target, rendered, metric.gauge_value().value());
        break;
    }
  }

  if (info.empty()) return;
  const std::string info_name = sanitize_name(trail);
  const size_t at = out.open(info_name, metric_type::info, info.front().help, "", suffixes_of(metric_type::info));
  if (at == family_set::none) {
    report(problems, trail, "<strings>", info_name, "another metric already claimed that name");
    return;
  }
  family &target = out.families[at];
  for (const info_series &series : info) {
    const std::string rendered = render_labels(series.labels);
    if (!target.series.insert(rendered).second) continue;
    sample added;
    added.suffix = "_info";
    added.labels = rendered;
    // An info series says what a thing is rather than how much of it there is;
    // the 1 is the whole value, the convention every exporter uses for
    // `..._info`.
    added.value = "1";
    target.samples.push_back(added);
  }
}

// The pre-renderer walk, kept verbatim rather than shared with collect(): the
// whole point of this path is that it stays frozen while the real renderer
// moves on. Children before values, keys pasted in untouched, `str::xtos` for
// the value - byte for byte what `build_metrics` used to append.
void collect_legacy(const PB::Metrics::MetricsBundle &bundle, const std::string &trail, std::string &body) {
  for (const PB::Metrics::MetricsBundle &child : bundle.children()) {
    collect_legacy(child, trail + "_" + child.key(), body);
  }
  for (const PB::Metrics::Metric &metric : bundle.value()) {
    if (!metric.has_gauge_value()) continue;
    body += trail + "_" + metric.key() + " " + str::xtos(metric.gauge_value().value()) + "\n";
  }
}

}  // namespace

std::string sanitize_name(const std::string &raw) {
  std::string expanded;
  expanded.reserve(raw.size());
  for (const char c : raw) {
    if (c == '%') {
      expanded += "percent";
    } else {
      expanded += c;
    }
  }
  std::string ret;
  ret.reserve(expanded.size());
  for (const char c : expanded) {
    const bool legal = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
    if (legal) {
      ret += c;
      continue;
    }
    // Every separator, whether it was already a `_` or is standing in for
    // something illegal, collapses into a single one. `a...b`, `a b` and
    // `a_-_b` all land on `a_b`, and - the case that actually turns up -
    // `disk.free.C:` joined to `_total` gives `disk_free_C_total` rather than
    // a stray `C__total`.
    if (!ret.empty() && ret[ret.size() - 1] == '_') continue;
    ret += '_';
  }
  // A name must start with a letter or an underscore, but OpenMetrics also
  // reserves every name that *begins* with an underscore, so the obvious `_`
  // prefix would trade one non-conformance for another. Borrow a letter
  // instead. Only reachable for a producer-chosen key - a PDH counter or a
  // Python script under the empty root bundle - since every built-in bundle
  // name starts with a letter.
  if (ret.empty()) return "metric";
  if ((ret[0] >= 'a' && ret[0] <= 'z') || (ret[0] >= 'A' && ret[0] <= 'Z')) return ret;
  const std::string tail = ret[0] == '_' ? ret.substr(1) : ret;
  return tail.empty() ? "metric" : "metric_" + tail;
}

std::string render_value(const double value) {
  if (std::isnan(value)) return "NaN";
  if (std::isinf(value)) return value > 0 ? "+Inf" : "-Inf";
  return str::render_shortest(value);
}

std::string escape_help(const std::string &raw) {
  std::string ret;
  ret.reserve(raw.size());
  for (const char c : raw) {
    if (c == '\\') {
      ret += "\\\\";
    } else if (c == '\n') {
      ret += "\\n";
    } else if (c == '\r') {
      // No escape exists for it and a bare one would end the line early.
      continue;
    } else {
      ret += c;
    }
  }
  return ret;
}

std::string escape_label_value(const std::string &raw) {
  std::string ret;
  ret.reserve(raw.size());
  for (const char c : raw) {
    if (c == '\\') {
      ret += "\\\\";
    } else if (c == '"') {
      ret += "\\\"";
    } else if (c == '\n') {
      ret += "\\n";
    } else if (c == '\r') {
      continue;
    } else {
      ret += c;
    }
  }
  return ret;
}

dialect dialect_for(const std::string &accept) {
  // A substring match is enough: `Accept` is a comma separated list with
  // optional q-values, and any client naming the OpenMetrics type at all is
  // telling us it can read the richer form.
  if (accept.find("application/openmetrics-text") != std::string::npos) return dialect::openmetrics_1_0;
  return dialect::prometheus_text_0_0_4;
}

std::string content_type_for(const std::string &accept) { return content_type_for(dialect_for(accept)); }

std::string content_type_for(const dialect dialect) {
  if (dialect == dialect::openmetrics_1_0) return "application/openmetrics-text; version=1.0.0; charset=utf-8";
  return "text/plain; version=0.0.4; charset=utf-8";
}

std::string render(const PB::Metrics::MetricsMessage &response, const dialect dialect, std::vector<std::string> *problems) {
  family_set collected;
  for (const PB::Metrics::MetricsMessage::Response &payload : response.payload()) {
    for (const PB::Metrics::MetricsBundle &bundle : payload.bundles()) {
      collect(bundle, bundle.key(), collected, problems);
    }
  }
  std::string body;
  for (const family &f : collected.families) {
    const std::string described = f.name + metadata_suffix(f.type, dialect);
    if (!f.help.empty()) body += "# HELP " + described + " " + escape_help(f.help) + "\n";
    body += "# TYPE " + described + " " + type_word(f.type, dialect) + "\n";
    // `# UNIT` is an OpenMetrics 1.0 line; the older parser reads it as a
    // comment, so it costs nothing to leave in both bodies.
    if (!f.unit.empty()) body += "# UNIT " + described + " " + f.unit + "\n";
    for (const sample &s : f.samples) {
      body += f.name + s.suffix + s.labels + " " + s.value + "\n";
    }
  }
  // Mandatory in OpenMetrics 1.0, and a parser that only knows the older
  // Prometheus text format reads it as a comment.
  body += "# EOF\n";
  return body;
}

std::string render_legacy(const PB::Metrics::MetricsMessage &response) {
  std::string body;
  for (const PB::Metrics::MetricsMessage::Response &payload : response.payload()) {
    for (const PB::Metrics::MetricsBundle &bundle : payload.bundles()) {
      collect_legacy(bundle, bundle.key(), body);
    }
  }
  return body;
}

}  // namespace openmetrics
