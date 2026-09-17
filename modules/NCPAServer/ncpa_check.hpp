// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/unordered/unordered_map.hpp>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

// The check half of the NCPA bridge: Nagios range syntax, `units` scaling,
// `aggregate`, `delta` and the exact stdout / perfdata text the NCPA agent
// produces. It is deliberately free of any NSClient++ dispatch so it can be
// unit tested without a core, the way check_nt_commands.cpp is.
//
// Every rule here is a transcription of NCPA's agent/listener/nodes.py
// (RunnableNode.get_nagios_return / is_within_range / adjust_scale /
// get_aggregated_values / deltaize_values). check_ncpa.py hands the agent's
// `stdout` to Nagios verbatim and Nagios XI parses the perfdata out of it, so
// a difference in spacing or in a threshold field is a difference a user sees.
// Where NCPA's own behaviour is odd, the oddity is reproduced and commented
// rather than corrected.
namespace ncpa {

// One value of a node. NCPA leaves carry either numbers or strings, and it
// formats an int ("%d") differently from a float ("%0.2f") - so which of the
// two a value is has to survive all the way to the rendering.
struct value {
  bool is_string = false;
  // Rendered with "%d" rather than "%0.2f", mirroring Python's
  // isinstance(x, int) test.
  bool is_integer = false;
  double number = 0;
  std::string text;

  static value from_double(double v);
  static value from_int(long long v);
  static value from_string(std::string s);

  // "%d" / "%0.2f" / the string itself.
  std::string format() const;
};

typedef std::vector<value> values_type;

// The query-string parameters check_ncpa.py sends, after parsing. `warning`,
// `critical`, `unit`, `units`, `aggregate`, `title` and `perfdata_label` are
// taken verbatim; `delta` is true unless the token is the literal "False"
// check_ncpa sends when the switch is off.
struct request_options {
  std::string warning;
  std::string critical;
  // The `units` scaling prefix: k, Ki, M, Mi, G, Gi, T, Ti (case-insensitive).
  std::string units;
  // The `unit` override, which replaces the node's own unit outright.
  std::string unit;
  // max / min / sum / avg; anything else leaves the values alone.
  std::string aggregate;
  std::string title;
  std::string perfdata_label;
  bool delta = false;
};

struct check_result {
  int returncode = 0;
  std::string stdout_text;
  // The perfdata on its own. A RunnableParentNode collects the perfdata of its
  // children and appends it to the parent's line, so the renderer hands it back
  // separately instead of only inside `stdout_text`.
  std::string perfdata;
};

// How one leaf is rendered. A leaf checked on its own takes the defaults; a
// leaf rendered as part of a RunnableParentNode (memory/virtual, a disk mount
// point, an interface) takes the rest.
struct render_options {
  bool use_perfdata = true;
  bool use_prefix = true;
  // The parent's primary child: its line carries a "{extra_data}" placeholder
  // the parent fills with its siblings' output.
  bool primary = false;
  // Non-zero marks this leaf as a secondary child of a percentage-primary
  // parent, whose perfdata carries no thresholds (they only describe the
  // primary value).
  double primary_total = 0;
  // A secondary child renders "Name: value" instead of "Name was value".
  bool secondary_data = false;
  std::string custom_output;
  bool capitalize_title = true;
  // The node's own name, which drives the two output special cases NCPA
  // hard-codes (`uptime` and an interface's `status`).
  std::string node_name;
};

// True when `v` falls in the alerting part of the Nagios range `range`.
// Throws std::runtime_error when the range is not one of the six shapes NCPA
// accepts, which is what turns a bad `-w` into UNKNOWN rather than a silent OK.
bool is_within_range(const std::string &range, double v);

// Apply the `units` prefix. NCPA only rescales a node whose unit is "b" or "B";
// anything else keeps its value, so `-u G` on a percentage is a no-op. `unit`
// is rewritten in place when a factor was applied ("B" -> "GiB").
void adjust_scale(values_type &vals, const std::string &units, std::string &unit);

// max / min / sum / avg collapse the list to one value; anything else (NCPA's
// default is the string "None") leaves it untouched.
void aggregate_values(values_type &vals, const std::string &mode);

// Python's str.capitalize(): first character upper, every other character
// lower. NCPA titles go through it, so "used_percent" is reported as
// "Used_percent" and a camel-cased name is flattened.
std::string capitalize(const std::string &s);

// "1 day 3 hours 4 minutes 12 seconds", the text NCPA substitutes for the raw
// number when the node is named `uptime`.
std::string elapsed_time(double seconds);

// Render one leaf: the returncode, the Nagios line and its perfdata.
check_result render_check(const values_type &vals, const std::string &unit, const request_options &opts, const render_options &ropts);

// Previous samples for `delta`, keyed the way NCPA keys its pickle files: the
// API accessor, the node name and the calling address, so two pollers watching
// the same counter do not consume each other's samples.
//
// NCPA persists these in temp files; this keeps them in memory, which is enough
// for a polling interval and loses nothing but the first sample after a
// restart. The first observation of a key reports 0, exactly as NCPA does when
// it finds no pickle.
class delta_store {
 public:
  // Replace `vals` with the per-second rate since the previous call for `key`.
  // Returns false (and zeroes the values) the first time a key is seen.
  bool deltaize(const std::string &key, values_type &vals);
  // Same, with an explicit clock so the arithmetic can be asserted in a test
  // without waiting a second out.
  bool deltaize_at(const std::string &key, values_type &vals, std::time_t now);
  void clear();

 private:
  struct entry {
    values_type values;
    std::time_t seen = 0;
  };
  boost::unordered_map<std::string, entry> entries_;
  std::mutex mutex_;
};

}  // namespace ncpa
