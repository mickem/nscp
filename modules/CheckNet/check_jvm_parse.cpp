// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <boost/json.hpp>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdint>
#include <locale>
#include <sstream>

#include <str/saturate.hpp>

#include "check_jvm_internal.hpp"

namespace json = boost::json;

namespace check_net {
namespace check_jvm_internal {

long long jvm_status::gc_count() const {
  long long total = 0;
  // A collector that has never run, or one that cannot count, reports -1.
  for (const jvm_collector &c : collectors)
    if (c.count > 0) total += c.count;
  return total;
}

long long jvm_status::gc_time() const {
  long long total = 0;
  for (const jvm_collector &c : collectors)
    if (c.time_ms > 0) total += c.time_ms;
  return total;
}

std::string jvm_status::collector_names() const {
  std::string out;
  for (const jvm_collector &c : collectors) {
    if (!out.empty()) out += ", ";
    out += c.name;
  }
  return out;
}

double jvm_status::heap_usage() const {
  const long long limit = heap_max > 0 ? heap_max : heap_committed;
  if (limit <= 0) return 0.0;
  // One decimal: enough for a threshold, and it renders as 29.1 rather than
  // 29.1015625 in the message.
  return std::round(1000.0 * static_cast<double>(heap_used) / static_cast<double>(limit)) / 10.0;
}

namespace {

// The filter grammar's variable names: a letter, then letters, digits and
// underscores (a leading underscore does not parse).
bool is_identifier(const std::string &s) {
  if (s.empty()) return false;
  if (!std::isalpha(static_cast<unsigned char>(s[0]))) return false;
  for (const char c : s)
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
  return true;
}

// Words the filter grammar matches case-insensitively as operators; an alias
// spelled like one parses as the operator, not as the keyword.
bool is_grammar_word(const std::string &s) {
  static const char *const words[] = {"and", "or", "not", "in", "like", "regexp", "eq", "ne", "lt", "le", "gt", "ge", "str"};
  std::string lower;
  for (const char c : s) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  for (const char *w : words)
    if (lower == w) return true;
  // The grammar matches `not` without a word boundary, so `notice > 1` reads
  // as `not (ice > 1)`. `not_after` survives only because `_after` cannot
  // start a variable name; anything else after the `not` would be split off.
  if (lower.size() > 3 && lower.compare(0, 3, "not") == 0 && lower[3] != '_') return true;
  return false;
}

// Split on '/' that is not escaped as '!/'. Escapes are kept in the segments;
// unescape() removes them where the value is used as a plain string.
std::vector<std::string> split_escaped(const std::string &s) {
  std::vector<std::string> out(1);
  for (std::size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '!' && i + 1 < s.size()) {
      out.back() += s[i];
      out.back() += s[++i];
    } else if (s[i] == '/') {
      out.emplace_back();
    } else {
      out.back() += s[i];
    }
  }
  return out;
}

std::string unescape(const std::string &s) {
  std::string out;
  for (std::size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '!' && i + 1 < s.size()) ++i;
    out += s[i];
  }
  return out;
}

bool parse_number(const std::string &text, double &out) {
  // Not std::stod: that honours the process locale, and a decimal-comma
  // locale would stop at the dot.
  std::istringstream stream(text);
  stream.imbue(std::locale::classic());
  stream >> out;
  if (stream.fail()) return false;
  stream >> std::ws;
  return stream.eof();
}

// A JSON number as a long long; -1 (the JMX "unknown") for anything else.
long long as_ll(const json::value *v) {
  if (v == nullptr) return -1;
  if (v->is_int64()) return v->as_int64();
  if (v->is_uint64()) {
    const auto u = v->as_uint64();
    return u > static_cast<std::uint64_t>(LLONG_MAX) ? LLONG_MAX : static_cast<long long>(u);
  }
  // Both come from the remote agent: a NaN or anything past the long long
  // range (1e300, as a number or as the string "1e300") must not be cast raw.
  if (v->is_double()) return std::isnan(v->as_double()) ? -1 : str::to_int64_saturating(v->as_double());
  if (v->is_string()) {
    double d = 0;
    if (parse_number(std::string(v->as_string().c_str()), d) && !std::isnan(d)) return str::to_int64_saturating(d);
  }
  return -1;
}

long long field_ll(const json::object *o, const char *key) { return o == nullptr ? -1 : as_ll(o->if_contains(key)); }

// Like field_ll but for a value where -1 would be a lie (a count of threads
// that was simply not returned): missing reads as 0.
long long count_ll(const json::object *o, const char *key) {
  const long long v = field_ll(o, key);
  return v < 0 ? 0 : v;
}

std::string field_str(const json::object *o, const char *key) {
  if (o == nullptr) return "";
  const json::value *v = o->if_contains(key);
  if (v == nullptr || !v->is_string()) return "";
  return std::string(v->as_string().c_str());
}

const json::object *as_obj(const json::value *v) { return (v != nullptr && v->is_object()) ? &v->as_object() : nullptr; }

custom_value to_custom(const json::value &v) {
  custom_value out;
  if (v.is_bool()) {
    out.has_number = true;
    out.number = v.as_bool() ? 1.0 : 0.0;
    out.text = v.as_bool() ? "true" : "false";
  } else if (v.is_int64()) {
    out.has_number = true;
    out.number = static_cast<double>(v.as_int64());
    out.text = std::to_string(v.as_int64());
  } else if (v.is_uint64()) {
    out.has_number = true;
    out.number = static_cast<double>(v.as_uint64());
    out.text = std::to_string(v.as_uint64());
  } else if (v.is_double()) {
    out.has_number = true;
    out.number = v.as_double();
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << v.as_double();
    out.text = oss.str();
  } else if (v.is_string()) {
    out.text = std::string(v.as_string().c_str());
    out.has_number = parse_number(out.text, out.number);
  } else if (v.is_null()) {
    out.text = "null";
  } else {
    out.text = json::serialize(v);
  }
  // A value past the double range (1e400) parses as infinity: keep its text
  // but do not compare or graph it as a number.
  if (out.has_number && !std::isfinite(out.number)) out.has_number = false;
  return out;
}

json::object read_request(const std::string &mbean, const std::vector<std::string> &attributes) {
  json::object o;
  o["type"] = "read";
  o["mbean"] = mbean;
  json::array a;
  for (const std::string &attr : attributes) a.emplace_back(attr);
  o["attribute"] = std::move(a);
  return o;
}

// The error of one bulk-response entry, or an empty string when it succeeded.
std::string entry_error(const json::object &entry) {
  const long long status = field_ll(&entry, "status");
  if (status == 200) return "";
  std::string message = field_str(&entry, "error");
  if (message.empty()) message = "status " + std::to_string(status);
  const json::object *request = as_obj(entry.if_contains("request"));
  const std::string mbean = field_str(request, "mbean");
  return mbean.empty() ? message : mbean + ": " + message;
}

void note_error(jvm_status &out, const std::string &error) {
  if (out.error.empty() && !error.empty()) out.error = error;
}

}  // namespace

bool parse_read_spec(const std::string &spec, jvm_read &out, std::string &error) {
  const auto eq = spec.find('=');
  if (eq == std::string::npos) {
    error = "expected alias=mbean/attribute[/path]: " + spec;
    return false;
  }
  out.alias = spec.substr(0, eq);
  if (!is_identifier(out.alias)) {
    error = "alias must start with a letter and contain only letters, digits and underscores: " + out.alias;
    return false;
  }
  if (is_grammar_word(out.alias)) {
    error = "alias " + out.alias + " is, or starts like, a word of the filter language (and, or, not, in, like, ...)";
    return false;
  }
  const std::vector<std::string> parts = split_escaped(spec.substr(eq + 1));
  if (parts.size() < 2 || parts[0].empty() || parts[1].empty()) {
    error = "expected alias=mbean/attribute[/path] (escape a / in the MBean name as !/): " + spec;
    return false;
  }
  out.mbean = unescape(parts[0]);
  if (out.mbean.find(':') == std::string::npos) {
    error = "not an MBean object name (domain:key=value,...): " + out.mbean;
    return false;
  }
  out.attribute = unescape(parts[1]);
  out.path.clear();
  for (std::size_t i = 2; i < parts.size(); ++i) {
    if (!out.path.empty()) out.path += "/";
    out.path += parts[i];
  }
  return true;
}

std::string build_bulk_request(const std::vector<jvm_read> &custom) {
  json::array requests;
  requests.emplace_back(read_request("java.lang:type=Memory", {"HeapMemoryUsage", "NonHeapMemoryUsage"}));
  requests.emplace_back(read_request("java.lang:type=Threading", {"ThreadCount", "DaemonThreadCount", "PeakThreadCount"}));
  requests.emplace_back(read_request("java.lang:type=Runtime", {"Uptime", "VmName", "VmVendor", "VmVersion", "SpecVersion"}));
  requests.emplace_back(read_request("java.lang:type=ClassLoading", {"LoadedClassCount"}));
  requests.emplace_back(read_request("java.lang:type=GarbageCollector,name=*", {"CollectionCount", "CollectionTime"}));
  for (const jvm_read &r : custom) {
    json::object o;
    o["type"] = "read";
    o["mbean"] = r.mbean;
    o["attribute"] = r.attribute;
    if (!r.path.empty()) o["path"] = r.path;
    requests.emplace_back(std::move(o));
  }
  return json::serialize(requests);
}

std::string top_level_error(const std::string &body) {
  boost::system::error_code ec;
  const json::value root = json::parse(body, ec);
  if (ec || !root.is_object()) return "";
  const json::object &o = root.as_object();
  if (o.if_contains("error") == nullptr) return "";
  std::string message = field_str(&o, "error");
  return message.empty() ? "status " + std::to_string(field_ll(&o, "status")) : message;
}

bool parse_bulk_response(const std::string &body, const std::vector<jvm_read> &custom, jvm_status &out) {
  boost::system::error_code ec;
  const json::value root = json::parse(body, ec);
  if (ec || !root.is_array()) return false;
  const json::array &entries = root.as_array();
  if (entries.size() != fixed_read_count + custom.size()) return false;
  for (const json::value &e : entries)
    if (!e.is_object()) return false;

  // Jolokia answers a bulk request in request order.
  const json::object &memory = entries[0].as_object();
  const json::object &threading = entries[1].as_object();
  const json::object &runtime = entries[2].as_object();
  const json::object &classes = entries[3].as_object();
  const json::object &gc = entries[4].as_object();

  note_error(out, entry_error(memory));
  if (const json::object *value = as_obj(memory.if_contains("value"))) {
    const json::object *heap = as_obj(value->if_contains("HeapMemoryUsage"));
    out.heap_used = count_ll(heap, "used");
    out.heap_committed = count_ll(heap, "committed");
    out.heap_max = field_ll(heap, "max");
    const json::object *nonheap = as_obj(value->if_contains("NonHeapMemoryUsage"));
    out.nonheap_used = count_ll(nonheap, "used");
    out.nonheap_committed = count_ll(nonheap, "committed");
  }

  note_error(out, entry_error(threading));
  if (const json::object *value = as_obj(threading.if_contains("value"))) {
    out.threads = count_ll(value, "ThreadCount");
    out.daemon_threads = count_ll(value, "DaemonThreadCount");
    out.peak_threads = count_ll(value, "PeakThreadCount");
  }

  note_error(out, entry_error(runtime));
  if (const json::object *value = as_obj(runtime.if_contains("value"))) {
    out.uptime_ms = count_ll(value, "Uptime");
    out.vm_name = field_str(value, "VmName");
    out.vm_vendor = field_str(value, "VmVendor");
    out.vm_version = field_str(value, "VmVersion");
    out.spec_version = field_str(value, "SpecVersion");
  }

  note_error(out, entry_error(classes));
  if (const json::object *value = as_obj(classes.if_contains("value"))) out.loaded_classes = count_ll(value, "LoadedClassCount");

  // A wildcard read answers with an object keyed by each matching MBean's
  // canonical name ("java.lang:name=G1 Young Generation,type=GarbageCollector").
  note_error(out, entry_error(gc));
  if (const json::object *value = as_obj(gc.if_contains("value"))) {
    for (const auto &kv : *value) {
      const std::string mbean(kv.key());
      jvm_collector c;
      const auto name_pos = mbean.find("name=");
      if (name_pos == std::string::npos) {
        c.name = mbean;
      } else {
        const auto end = mbean.find(',', name_pos);
        c.name = mbean.substr(name_pos + 5, end == std::string::npos ? std::string::npos : end - name_pos - 5);
      }
      const json::object *attrs = as_obj(&kv.value());
      c.count = field_ll(attrs, "CollectionCount");
      c.time_ms = field_ll(attrs, "CollectionTime");
      out.collectors.push_back(c);
    }
  }

  for (std::size_t i = 0; i < custom.size(); ++i) {
    const json::object &entry = entries[fixed_read_count + i].as_object();
    const std::string err = entry_error(entry);
    if (!err.empty()) {
      note_error(out, custom[i].alias + ": " + err);
      continue;
    }
    const json::value *value = entry.if_contains("value");
    if (value != nullptr) out.custom[custom[i].alias] = to_custom(*value);
  }
  return true;
}

}  // namespace check_jvm_internal
}  // namespace check_net
