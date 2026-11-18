// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_protocol.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace json = boost::json;

namespace icinga_agent {

std::vector<std::string> netstring_reader::feed(const char *data, const std::size_t size) {
  buffer_.append(data, size);
  std::vector<std::string> out;
  std::size_t pos = 0;
  while (true) {
    // Icinga writes a trailing ',' after every payload; tolerate a stray one.
    while (pos < buffer_.size() && buffer_[pos] == ',') ++pos;
    if (pos >= buffer_.size()) break;
    const std::size_t colon = buffer_.find(':', pos);
    if (colon == std::string::npos) {
      if (buffer_.size() - pos > 20) throw std::runtime_error("netstring: length prefix too long");
      break;
    }
    if (colon == pos || colon - pos > 20) throw std::runtime_error("netstring: invalid length prefix");
    std::size_t len = 0;
    for (std::size_t i = pos; i < colon; ++i) {
      const char c = buffer_[i];
      if (c < '0' || c > '9') throw std::runtime_error("netstring: invalid length prefix");
      len = len * 10 + static_cast<std::size_t>(c - '0');
      if (len > max_message_) throw std::runtime_error("netstring: message exceeds the size limit");
    }
    // payload + the closing ','
    if (buffer_.size() < colon + 1 + len + 1) break;
    if (buffer_[colon + 1 + len] != ',') throw std::runtime_error("netstring: missing terminator");
    out.emplace_back(buffer_.substr(colon + 1, len));
    pos = colon + 1 + len + 1;
  }
  buffer_.erase(0, pos);
  return out;
}

std::string frame(const std::string &json_text) { return std::to_string(json_text.size()) + ":" + json_text + ","; }

std::string make_message(const std::string &method, json::object params) {
  json::object msg;
  msg["jsonrpc"] = "2.0";
  msg["method"] = method;
  msg["params"] = std::move(params);
  return frame(json::serialize(msg));
}

std::string make_hello(const std::string &agent_version) {
  json::object p;
  // Bit 0 (ExecuteArbitraryCommand) is deliberately not set: we do not run
  // ad-hoc commands from the master's /v1/actions/execute-command. Bit 1 is
  // IfwApiCheckCommand - we understand the ifw-api argument convention.
  p["capabilities"] = 2;
  p["version"] = kAnnouncedVersion;
  p["agentversion"] = agent_version;
  return make_message("icinga::Hello", std::move(p));
}

std::string make_heartbeat(const int timeout_seconds) {
  json::object p;
  p["timeout"] = timeout_seconds;
  return make_message("event::Heartbeat", std::move(p));
}

std::string make_log_position(const double unix_time) {
  json::object p;
  p["log_position"] = unix_time;
  return make_message("log::SetLogPosition", std::move(p));
}

std::string make_certificate_request(const std::string &ticket) {
  json::object p;
  if (!ticket.empty()) p["ticket"] = ticket;
  return make_message("pki::RequestCertificate", std::move(p));
}

namespace {

const json::value *find(const json::object &obj, const char *key) {
  const auto it = obj.find(key);
  if (it == obj.end()) return nullptr;
  return &it->value();
}

std::string as_string(const json::value &v) {
  if (v.is_string()) return std::string(v.as_string());
  if (v.is_bool()) return v.as_bool() ? "true" : "false";
  if (v.is_int64()) return std::to_string(v.as_int64());
  if (v.is_uint64()) return std::to_string(v.as_uint64());
  if (v.is_double()) {
    const double d = v.as_double();
    if (std::floor(d) == d && std::fabs(d) < 1e15) return std::to_string(static_cast<long long>(d));
    std::ostringstream ss;
    ss << std::setprecision(15) << d;
    return ss.str();
  }
  if (v.is_null()) return "";
  return json::serialize(v);
}

std::string string_field(const json::object &obj, const char *key) {
  const json::value *v = find(obj, key);
  return v ? as_string(*v) : std::string();
}

bool is_empty(const json::value &v) {
  if (v.is_null()) return true;
  if (v.is_string()) return v.as_string().empty();
  if (v.is_array()) return v.as_array().empty();
  if (v.is_object()) return v.as_object().empty();
  return false;
}

bool is_truthy(const json::value &v) {
  if (v.is_bool()) return v.as_bool();
  if (v.is_int64()) return v.as_int64() != 0;
  if (v.is_uint64()) return v.as_uint64() != 0;
  if (v.is_double()) return v.as_double() != 0;
  if (v.is_string()) {
    const std::string s(v.as_string());
    return !(s.empty() || s == "0" || s == "false");
  }
  return !is_empty(v);
}

// A macro that the master did not send is "missing": Icinga drops the whole
// argument in that case, and so do we.
struct missing_macro {};

json::value expand(const std::string &text, const json::object &macros) {
  // Exactly one macro: keep its type.
  if (text.size() > 2 && text.front() == '$' && text.back() == '$' && text.find('$', 1) == text.size() - 1) {
    const std::string name = text.substr(1, text.size() - 2);
    const json::value *v = find(macros, name.c_str());
    if (!v) throw missing_macro();
    return *v;
  }
  std::string out;
  std::size_t pos = 0;
  while (pos < text.size()) {
    const std::size_t start = text.find('$', pos);
    if (start == std::string::npos) {
      out += text.substr(pos);
      break;
    }
    const std::size_t end = text.find('$', start + 1);
    if (end == std::string::npos) throw std::runtime_error("unterminated macro in '" + text + "'");
    out += text.substr(pos, start - pos);
    const std::string name = text.substr(start + 1, end - start - 1);
    if (name.empty()) {
      out += '$';  // `$$` is an escaped dollar
    } else {
      const json::value *v = find(macros, name.c_str());
      if (!v) throw missing_macro();
      if (v->is_array()) {
        std::string joined;
        for (const auto &e : v->as_array()) {
          if (!joined.empty()) joined += ",";
          joined += as_string(e);
        }
        out += joined;
      } else {
        out += as_string(*v);
      }
    }
    pos = end + 1;
  }
  return json::value(out);
}

json::value expand_value(const json::value &v, const json::object &macros) {
  if (v.is_string()) return expand(std::string(v.as_string()), macros);
  if (v.is_array()) {
    json::array out;
    for (const auto &e : v.as_array()) out.push_back(expand_value(e, macros));
    return out;
  }
  return v;
}

std::string strip_dashes(const std::string &key) {
  std::size_t i = 0;
  while (i < key.size() && key[i] == '-') ++i;
  return key.substr(i);
}

}  // namespace

json::value expand_macros(const std::string &text, const json::object &macros) {
  try {
    return expand(text, macros);
  } catch (const missing_macro &) {
    return nullptr;
  }
}

std::list<std::string> build_arguments(const json::object &arguments, const json::object &macros) {
  struct arg {
    long order;
    std::string key;
    std::list<std::string> tokens;
  };
  std::vector<arg> args;
  for (const auto &kv : arguments) {
    std::string key = strip_dashes(std::string(kv.key()));
    long order = 0;
    bool skip_key = false;
    bool repeat_key = true;
    const json::value *value = nullptr;
    json::value plain;
    if (kv.value().is_object()) {
      const json::object &def = kv.value().as_object();
      if (const json::value *k = find(def, "key")) key = strip_dashes(as_string(*k));
      if (const json::value *o = find(def, "order")) order = static_cast<long>(std::strtol(as_string(*o).c_str(), nullptr, 10));
      if (const json::value *s = find(def, "skip_key")) skip_key = is_truthy(*s);
      if (const json::value *r = find(def, "repeat_key")) repeat_key = is_truthy(*r);
      if (const json::value *set_if = find(def, "set_if")) {
        try {
          if (!is_truthy(expand_value(*set_if, macros))) continue;
        } catch (const missing_macro &) {
          continue;
        }
      }
      value = find(def, "value");
    } else {
      plain = kv.value();
      value = &plain;
    }
    arg a{order, key, {}};
    if (value && !value->is_null()) {
      json::value resolved;
      try {
        resolved = expand_value(*value, macros);
      } catch (const missing_macro &) {
        continue;
      }
      if (is_empty(resolved)) continue;
      std::vector<std::string> values;
      if (resolved.is_array()) {
        for (const auto &e : resolved.as_array()) values.push_back(as_string(e));
      } else {
        values.push_back(as_string(resolved));
      }
      if (skip_key || key.empty()) {
        for (const auto &v : values) a.tokens.push_back(v);
      } else if (repeat_key) {
        for (const auto &v : values) a.tokens.push_back(key + "=" + v);
      } else {
        std::string joined;
        for (const auto &v : values) joined += (joined.empty() ? "" : ",") + v;
        a.tokens.push_back(key + "=" + joined);
      }
    } else {
      if (key.empty()) continue;
      a.tokens.push_back(key);
    }
    args.push_back(std::move(a));
  }
  std::stable_sort(args.begin(), args.end(), [](const arg &l, const arg &r) {
    if (l.order != r.order) return l.order < r.order;
    return l.key < r.key;
  });
  std::list<std::string> out;
  for (const auto &a : args) out.insert(out.end(), a.tokens.begin(), a.tokens.end());
  return out;
}

query_request parse_execute_command(const json::value &params_value) {
  query_request req;
  if (!params_value.is_object()) {
    req.error = "ExecuteCommand without params";
    return req;
  }
  const json::object &params = params_value.as_object();
  req.host = string_field(params, "host");
  req.service = string_field(params, "service");
  req.command_type = string_field(params, "command_type");
  req.icinga_command = string_field(params, "command");
  if (const json::value *d = find(params, "deadline")) {
    if (d->is_number()) req.deadline = d->to_number<double>();
  }
  if (req.command_type != "check_command") {
    req.error = "Only check commands are supported, got '" + req.command_type + "'";
    return req;
  }
  json::object macros;
  if (const json::value *m = find(params, "macros")) {
    if (m->is_object()) macros = m->as_object();
  }
  const json::value *ifw_command = find(macros, "ifw_api_command");
  if (ifw_command && !is_empty(*ifw_command)) {
    req.query = as_string(*ifw_command);
    if (const json::value *a = find(macros, "ifw_api_arguments")) {
      if (a->is_object()) {
        try {
          req.arguments = build_arguments(a->as_object(), macros);
        } catch (const std::exception &e) {
          req.error = std::string("Failed to build arguments: ") + e.what();
        }
      }
    }
  } else {
    req.query = req.icinga_command;
  }
  if (req.query.empty()) req.error = "ExecuteCommand without a command";
  return req;
}

std::vector<std::string> split_perfdata(const std::string &perfdata) {
  std::vector<std::string> out;
  std::string cur;
  bool in_quotes = false;
  for (const char c : perfdata) {
    if (c == '\'') in_quotes = !in_quotes;
    if ((c == ' ' || c == '\n') && !in_quotes) {
      if (!cur.empty()) out.push_back(cur);
      cur.clear();
      continue;
    }
    cur += c;
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}

std::string make_check_result(const std::string &host, const std::string &service, const check_result &result) {
  json::object cr;
  cr["type"] = "CheckResult";
  cr["exit_status"] = result.exit_status;
  cr["state"] = result.exit_status;
  cr["active"] = true;
  cr["output"] = result.output;
  cr["command"] = result.command_line;
  cr["check_source"] = result.check_source;
  cr["execution_start"] = result.execution_start;
  cr["execution_end"] = result.execution_end;
  cr["schedule_start"] = result.execution_start;
  cr["schedule_end"] = result.execution_end;
  json::array perf;
  for (const auto &p : split_perfdata(result.perfdata)) perf.emplace_back(p);
  cr["performance_data"] = std::move(perf);
  json::object p;
  p["host"] = host;
  if (!service.empty()) p["service"] = service;
  p["cr"] = std::move(cr);
  return make_message("event::CheckResult", std::move(p));
}

}  // namespace icinga_agent
