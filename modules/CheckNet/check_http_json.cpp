// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_http_json.hpp"

#include <cctype>
#include <sstream>

#include <boost/json.hpp>

namespace json = boost::json;

namespace check_net {
namespace check_http_json {

std::vector<std::string> split_path(const std::string &path) {
  std::vector<std::string> segs;
  std::string cur;
  bool in_quote = false;
  for (const char c : path) {
    if (in_quote) {
      if (c == '\'')
        in_quote = false;
      else
        cur.push_back(c);
    } else if (c == '\'') {
      in_quote = true;
    } else if (c == '.') {
      segs.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  segs.push_back(cur);
  return segs;
}

namespace {

bool is_index(const std::string &s) {
  if (s.empty()) return false;
  for (const char c : s)
    if (!std::isdigit(static_cast<unsigned char>(c))) return false;
  return true;
}

// Walk `root` down `segs`, returning the value found or nullptr.
const json::value *walk(const json::value &root, const std::vector<std::string> &segs) {
  const json::value *v = &root;
  for (const std::string &seg : segs) {
    if (v == nullptr) return nullptr;
    if (v->is_object()) {
      v = v->as_object().if_contains(seg);
    } else if (v->is_array() && is_index(seg)) {
      const json::array &arr = v->as_array();
      const std::size_t idx = std::stoull(seg);
      if (idx >= arr.size()) return nullptr;
      v = &arr[idx];
    } else {
      return nullptr;
    }
  }
  return v;
}

}  // namespace

bool extract(const std::string &body, const std::vector<std::pair<std::string, std::string>> &alias_paths, extraction &out) {
  boost::system::error_code ec;
  const json::value root = json::parse(body, ec);
  if (ec) return false;

  for (const auto &ap : alias_paths) {
    const std::string &alias = ap.first;
    const json::value *v = walk(root, split_path(ap.second));
    if (v == nullptr || v->is_null()) continue;

    if (v->is_bool()) {
      out.numbers[alias] = v->as_bool() ? 1.0 : 0.0;
      out.strings[alias] = v->as_bool() ? "true" : "false";
    } else if (v->is_int64()) {
      out.numbers[alias] = static_cast<double>(v->as_int64());
      out.strings[alias] = std::to_string(v->as_int64());
    } else if (v->is_uint64()) {
      out.numbers[alias] = static_cast<double>(v->as_uint64());
      out.strings[alias] = std::to_string(v->as_uint64());
    } else if (v->is_double()) {
      out.numbers[alias] = v->as_double();
      // Render doubles in a human-friendly form (0.25) rather than Boost.JSON's
      // compact "2.5E-1".
      std::ostringstream oss;
      oss << v->as_double();
      out.strings[alias] = oss.str();
    } else if (v->is_string()) {
      out.strings[alias] = std::string(v->as_string().c_str());
    } else {
      // array / object: expose the serialized JSON for substring/regex matching.
      out.strings[alias] = json::serialize(*v);
    }
  }
  return true;
}

}  // namespace check_http_json
}  // namespace check_net
