// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "openmetrics_renderer.hpp"

#include <cmath>
#include <map>
#include <str/number_format.hpp>
#include <str/xtos.hpp>

namespace openmetrics {

namespace {

// One family, collected before anything is written: OpenMetrics requires the
// samples of a family to be contiguous under a single `# TYPE`, which the flat
// list of lines this replaces could not promise once two producers interleave.
// One sample per family today; labels are what will let a family hold several.
struct family {
  std::string name;
  std::string type;
  std::string value;
};

struct family_set {
  std::vector<family> families;
  // name -> index into `families`, so lookup is cheap and the emitted order
  // stays first-seen rather than alphabetical.
  std::map<std::string, size_t> index;

  // Adds one sample, or returns why it could not be added. Sanitising is
  // lossy, so two distinct keys can land on one name (`foo.bar` and `foo bar`,
  // or `mem.%` and `mem.percent`); when they do, the first metric of the
  // snapshot keeps the name and the rest are dropped rather than emitted as a
  // second sample of the same series - a duplicate a strict parser rejects for
  // the whole body, not just for that metric.
  std::string add_sample(const std::string &name, const std::string &type, const std::string &value) {
    if (index.find(name) != index.end()) return "another metric already claimed that name";
    family added;
    added.name = name;
    added.type = type;
    added.value = value;
    index[name] = families.size();
    families.push_back(added);
    return "";
  }
};

void collect(const PB::Metrics::MetricsBundle &bundle, const std::string &trail, family_set &out, std::vector<std::string> *problems) {
  for (const PB::Metrics::MetricsBundle &child : bundle.children()) {
    collect(child, trail + "_" + child.key(), out, problems);
  }
  for (const PB::Metrics::Metric &metric : bundle.value()) {
    // Strings carry no numeric sample, so there is nothing to expose yet; the
    // `info` family that gives them a home needs the metadata work.
    if (!metric.has_gauge_value()) continue;
    // Sanitise the joined path in one pass rather than per segment: that is
    // what collapses a run spanning a separator (`disk.io.` + `C:` would leave
    // `disk_io__C_` if each part were cleaned on its own).
    const std::string name = sanitize_name(trail + "_" + metric.key());
    const std::string problem = out.add_sample(name, "gauge", render_value(metric.gauge_value().value()));
    if (!problem.empty() && problems != nullptr) {
      problems->push_back("Dropping metric '" + trail + "." + metric.key() + "' from the OpenMetrics exposition: it renders as '" + name + "' and " + problem +
                          ".");
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
    body += f.name + " " + f.value + "\n";
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
