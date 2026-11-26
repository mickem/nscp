// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <map>
#include <string>
#include <vector>

namespace check_net {
namespace check_jvm_internal {

// One user-requested MBean attribute (`attribute=alias=mbean/attribute[/path]`).
struct jvm_read {
  std::string alias;
  std::string mbean;
  std::string attribute;
  // Jolokia inner path into a composite/tabular value ("used" for a
  // MemoryUsage), already in Jolokia's slash notation with escapes intact.
  std::string path;
};

// The value read for a jvm_read. A JSON number or boolean has `has_number`
// set; a numeric string is read as a number too (some MBeans expose counters
// as strings). Objects and arrays are kept as their serialized JSON in `text`.
struct custom_value {
  bool has_number = false;
  double number = 0.0;
  std::string text;
};

// One garbage collector (java.lang:type=GarbageCollector,name=*).
struct jvm_collector {
  std::string name;
  long long count = 0;
  long long time_ms = 0;
};

// The values a single bulk Jolokia request yields. Fields default to the
// "unknown" value the JVM itself uses where one exists (heap_max is -1 when
// the JVM has no defined maximum, exactly like MemoryUsage.getMax()).
struct jvm_status {
  long long heap_used = 0;
  long long heap_committed = 0;
  long long heap_max = -1;
  long long nonheap_used = 0;
  long long nonheap_committed = 0;
  long long threads = 0;
  long long daemon_threads = 0;
  long long peak_threads = 0;
  long long loaded_classes = 0;
  long long uptime_ms = 0;
  std::string vm_name;
  std::string vm_vendor;
  std::string vm_version;
  std::string spec_version;
  std::vector<jvm_collector> collectors;
  std::map<std::string, custom_value> custom;
  // The first sub-request Jolokia answered with an error, rendered as
  // "<mbean>: <message>"; empty when every read succeeded.
  std::string error;
  // The first of the platform reads (memory, threading, runtime, class
  // loading, garbage collectors) that failed, in the same form; empty when
  // they all succeeded. Without them the check has no JVM state to report.
  std::string platform_error;

  long long gc_count() const;
  long long gc_time() const;
  std::string collector_names() const;
  // Heap in use as a percentage of the maximum heap, or of the committed
  // heap when the JVM has no maximum (-Xmx unset on some collectors). 0 when
  // neither is known. Rounded to one decimal.
  double heap_usage() const;
};

// Parse an `attribute=` option value: `alias=mbean/attribute[/inner/path]`.
// Slashes inside the MBean name (a Tomcat context path, `context=/app`) are
// written `!/` and a literal `!` as `!!`, which is Jolokia's own escape for
// its GET URLs, so a path copied from a Jolokia URL works unchanged. The alias
// must be a filter-keyword-shaped identifier. Returns false and sets `error`
// on a malformed spec.
bool parse_read_spec(const std::string &spec, jvm_read &out, std::string &error);

// The JSON array POSTed to the agent: the fixed platform reads (memory,
// threading, runtime, class loading, garbage collectors) followed by one read
// per custom attribute, in order.
std::string build_bulk_request(const std::vector<jvm_read> &custom);

// Number of entries build_bulk_request emits before the custom reads.
static const std::size_t fixed_read_count = 5;

// Parse the agent's bulk response. Returns false when the body is not a
// Jolokia bulk response for the request we sent (not JSON, not an array, the
// wrong number of entries); per-read failures are not a parse failure, they
// land in `out.error` with the values of the other reads intact.
bool parse_bulk_response(const std::string &body, const std::vector<jvm_read> &custom, jvm_status &out);

// The message of a top-level Jolokia error object ({"status":403,"error":...})
// or an empty string when the body is not one. Jolokia answers some failures
// (an access-policy denial, a malformed request) with HTTP 200 and such a body.
std::string top_level_error(const std::string &body);

}  // namespace check_jvm_internal
}  // namespace check_net
