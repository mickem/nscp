// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "openmetrics_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <str/number_format.hpp>
#include <str/xtos.hpp>
#include <utility>

namespace openmetrics {

namespace {

// One sample: the labels that tell it apart from its siblings, already
// sanitised and escaped, plus the rendered value.
struct sample {
  typedef std::vector<std::pair<std::string, std::string> > label_list;
  label_list labels;
  std::string value;

  // `k="v"` joined with commas, in the order the producer added the labels -
  // which is stable because the builder keeps insertion order, and stability is
  // what stops a scraper seeing a series rename between two scrapes.
  std::string render_labels() const {
    if (labels.empty()) return "";
    std::string ret = "{";
    for (label_list::const_iterator it = labels.begin(); it != labels.end(); ++it) {
      if (it != labels.begin()) ret += ",";
      ret += it->first + "=\"" + escape_label_value(it->second) + "\"";
    }
    return ret + "}";
  }

  // What makes this sample the same series as another: the label set, order
  // ignored, since `{a="1",b="2"}` and `{b="2",a="1"}` are one series to a
  // scraper even though the two strings differ.
  std::string identity() const {
    std::vector<std::string> parts;
    parts.reserve(labels.size());
    for (const label_list::value_type &l : labels) parts.push_back(l.first + "=" + l.second);
    std::sort(parts.begin(), parts.end());
    std::string ret;
    // A separator no label name or value can contain, so `a=b,c` and `a=b` +
    // `c` cannot be confused for one another.
    for (const std::string &part : parts) ret += part + "\x1f";
    return ret;
  }
};

// One family, collected before anything is written: OpenMetrics requires the
// samples of a family to be contiguous under a single `# TYPE`, which the flat
// list of lines this replaces could not promise once two producers interleave.
// A family holds every sample that shares its name - one per core, per NIC or
// per drive once the producers label them, and exactly one for a metric with
// no dimensions.
struct family {
  std::string name;
  std::string type;
  std::vector<sample> samples;
  // The identities already in `samples`, so the duplicate check does not walk
  // the list for every sample of a 64-core machine.
  std::set<std::string> series;
};

struct family_set {
  std::vector<family> families;
  // name -> index into `families`, so lookup is cheap and the emitted order
  // stays first-seen rather than alphabetical.
  std::map<std::string, size_t> index;

  // Adds one sample, or returns why it could not be added. Two ways that can
  // fail, and both produce a document a strict parser rejects outright rather
  // than a single bad metric:
  //
  //   * the name is taken by a family of a different type. One type per family
  //     is the rule; everything is a gauge today, so this only becomes
  //     reachable when counters land, and it is cheaper to be right now than
  //     to remember then.
  //   * the exact series is already there. Sanitising is lossy, so two distinct
  //     keys can land on one name (`foo.bar` and `foo bar`, or `mem.%` and
  //     `mem.percent`); with labels in play it is also what a producer emitting
  //     the same instance twice would produce. Either way the first one of the
  //     snapshot keeps the series and the rest are dropped.
  std::string add_sample(const std::string &name, const std::string &type, const sample &s) {
    const std::map<std::string, size_t>::iterator it = index.find(name);
    if (it == index.end()) {
      family added;
      added.name = name;
      added.type = type;
      added.samples.push_back(s);
      added.series.insert(s.identity());
      index[name] = families.size();
      families.push_back(added);
      return "";
    }
    family &existing = families[it->second];
    if (existing.type != type) return "another metric already claimed that name as a " + existing.type;
    if (!existing.series.insert(s.identity()).second) {
      return s.labels.empty() ? "another metric already claimed that name" : "another metric already claimed that name with the same labels";
    }
    existing.samples.push_back(s);
    return "";
  }
};

// Read the dimensions a producer attached, cleaning the names and dropping
// what cannot be emitted. A label name repeated within one sample is the
// interesting case: it is invalid in the exposition, and it happens without
// anybody writing it twice, because two raw names can sanitise to the same one.
sample::label_list read_labels(const PB::Metrics::Metric &metric) {
  sample::label_list labels;
  std::set<std::string> seen;
  for (const PB::Common::KeyValue &dim : metric.dims()) {
    // An empty value is the same series as no label at all, so emitting it
    // would make two samples collide that the producer meant to keep apart.
    if (dim.value().empty()) continue;
    const std::string name = sanitize_label_name(dim.key());
    if (!seen.insert(name).second) continue;
    labels.push_back(std::make_pair(name, dim.value()));
  }
  return labels;
}

void collect(const PB::Metrics::MetricsBundle &bundle, const std::string &trail, family_set &out, std::vector<std::string> *problems) {
  for (const PB::Metrics::MetricsBundle &child : bundle.children()) {
    collect(child, trail + "_" + child.key(), out, problems);
  }
  for (const PB::Metrics::Metric &metric : bundle.value()) {
    // Strings carry no numeric sample, so there is nothing to expose yet; the
    // `info` family that gives them a home needs the metadata work.
    if (!metric.has_gauge_value()) continue;

    sample s;
    s.labels = read_labels(metric);
    s.value = render_value(metric.gauge_value().value());

    // `alias` is the family name with the instance lifted out of it, and the
    // builder only writes it alongside the dimensions that tell the instances
    // apart. Without both, the key is the whole name, exactly as before - a
    // producer that has not been swept and an out-of-tree module both land
    // here.
    const std::string leaf = s.labels.empty() || metric.alias().empty() ? metric.key() : metric.alias();
    // Sanitise the joined path in one pass rather than per segment: that is
    // what collapses a run spanning a separator (`disk.io.` + `C:` would leave
    // `disk_io__C_` if each part were cleaned on its own).
    const std::string name = sanitize_name(trail + "_" + leaf);
    const std::string problem = out.add_sample(name, "gauge", s);
    if (!problem.empty() && problems != nullptr) {
      problems->push_back("Dropping metric '" + trail + "." + metric.key() + "' from the OpenMetrics exposition: it renders as '" + name +
                          s.render_labels() + "' and " + problem + ".");
    }
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

namespace {
// The shared body of both name mappings. `fallback` is the letter-carrying
// prefix borrowed by a name that would otherwise start with a digit, and the
// whole name when nothing survives the cleaning.
std::string clean_name(const std::string &raw, const std::string &fallback) {
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
  if (ret.empty()) return fallback;
  if ((ret[0] >= 'a' && ret[0] <= 'z') || (ret[0] >= 'A' && ret[0] <= 'Z')) return ret;
  const std::string tail = ret[0] == '_' ? ret.substr(1) : ret;
  return tail.empty() ? fallback : fallback + "_" + tail;
}
}  // namespace

std::string sanitize_name(const std::string &raw) { return clean_name(raw, "metric"); }

std::string sanitize_label_name(const std::string &raw) { return clean_name(raw, "label"); }

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
    } else {
      ret += c;
    }
  }
  return ret;
}

std::string render_value(const double value) {
  if (std::isnan(value)) return "NaN";
  if (std::isinf(value)) return value > 0 ? "+Inf" : "-Inf";
  return str::render_shortest(value);
}

std::string content_type_for(const std::string &accept) {
  // A substring match is enough: `Accept` is a comma separated list with
  // optional q-values, and any client naming the OpenMetrics type at all is
  // telling us it can read the richer form.
  if (accept.find("application/openmetrics-text") != std::string::npos) {
    return "application/openmetrics-text; version=1.0.0; charset=utf-8";
  }
  return "text/plain; version=0.0.4; charset=utf-8";
}

std::string render(const PB::Metrics::MetricsMessage &response, std::vector<std::string> *problems) {
  family_set collected;
  for (const PB::Metrics::MetricsMessage::Response &payload : response.payload()) {
    for (const PB::Metrics::MetricsBundle &bundle : payload.bundles()) {
      collect(bundle, bundle.key(), collected, problems);
    }
  }
  std::string body;
  for (const family &f : collected.families) {
    body += "# TYPE " + f.name + " " + f.type + "\n";
    // Contiguous, which is the whole reason the families are collected before
    // anything is written: a scraper reading a sample of one family after
    // another family started has no `# TYPE` to apply to it.
    for (const sample &s : f.samples) body += f.name + s.render_labels() + " " + s.value + "\n";
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
