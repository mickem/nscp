// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <nscapi/nscapi_facts_helper.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sstream>
#include <string>
#include <vector>

namespace nscapi {
namespace facts {
namespace detail {

// The document under construction. An object keeps its children in insertion
// order: the core serialises canonically (keys sorted) before hashing, so the
// order here is only what a human reads in a log line, and insertion order is
// the order the producer wrote - which is the readable one.
struct node {
  enum kind { object_kind, array_kind, scalar_kind };

  kind type = object_kind;
  std::vector<std::pair<std::string, std::unique_ptr<node> > > fields;  // object
  std::vector<std::unique_ptr<node> > entries;                          // array
  std::string scalar;                                                   // already-serialised JSON scalar

  node *find(const std::string &key) {
    for (auto &field : fields) {
      if (field.first == key) return field.second.get();
    }
    return nullptr;
  }

  // Find or create a child of the given kind. A key written twice with
  // different kinds is a producer bug; the later write wins rather than
  // producing a half-object, and the core's validation is what catches the
  // shape that results.
  node *child(const std::string &key, const kind type) {
    node *existing = find(key);
    if (existing != nullptr) {
      if (existing->type == type) return existing;
      existing->type = type;
      existing->fields.clear();
      existing->entries.clear();
      existing->scalar.clear();
      return existing;
    }
    std::unique_ptr<node> created(new node());
    created->type = type;
    node *raw = created.get();
    fields.emplace_back(key, std::move(created));
    return raw;
  }

  void set_scalar(const std::string &key, const std::string &serialised) {
    node *target = child(key, scalar_kind);
    target->scalar = serialised;
  }

  node *append() {
    std::unique_ptr<node> created(new node());
    node *raw = created.get();
    entries.push_back(std::move(created));
    return raw;
  }

  void write(std::string &out) const {
    switch (type) {
      case object_kind: {
        out += '{';
        bool first = true;
        for (const auto &field : fields) {
          if (!first) out += ',';
          first = false;
          out += quote(field.first);
          out += ':';
          field.second->write(out);
        }
        out += '}';
        return;
      }
      case array_kind: {
        out += '[';
        bool first = true;
        for (const auto &entry : entries) {
          if (!first) out += ',';
          first = false;
          entry->write(out);
        }
        out += ']';
        return;
      }
      default:
        out += scalar.empty() ? "null" : scalar;
        return;
    }
  }
};

}  // namespace detail

namespace {

// Render a double the way a JSON writer has to: never with an exponent-free
// integer spelling that would read back as an integer on the other side, and
// never as `inf`/`nan`, which is not JSON at all.
std::string number_to_string(const double number) {
  if (number != number || number > 1e308 || number < -1e308) return "0";
  char buffer[40];
  std::snprintf(buffer, sizeof(buffer), "%.17g", number);
  std::string text(buffer);
  // %.17g is round-trip safe but noisy (0.10000000000000001). Try the shorter
  // spellings and keep the first that reads back identically.
  for (int precision = 1; precision < 17; ++precision) {
    std::snprintf(buffer, sizeof(buffer), "%.*g", precision, number);
    if (std::strtod(buffer, nullptr) == number) {
      text = buffer;
      break;
    }
  }
  return text;
}

std::string format_time(const std::time_t when, const char *format) {
  if (when <= 0) return std::string();
  std::tm parts;
#ifdef _WIN32
  if (gmtime_s(&parts, &when) != 0) return std::string();
#else
  if (gmtime_r(&when, &parts) == nullptr) return std::string();
#endif
  char buffer[64];
  const std::size_t written = std::strftime(buffer, sizeof(buffer), format, &parts);
  if (written == 0) return std::string();
  return std::string(buffer, written);
}

}  // namespace

std::string quote(const std::string &text) {
  std::string out;
  out.reserve(text.size() + 2);
  out += '"';
  for (const char c : text) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char escape[7];
          std::snprintf(escape, sizeof(escape), "\\u%04x", static_cast<unsigned int>(static_cast<unsigned char>(c)));
          out += escape;
        } else {
          out += c;
        }
    }
  }
  out += '"';
  return out;
}

void declare(nscapi::settings_helper::settings_registry &settings, const std::string &fact_set, const std::string &title, const std::string &description) {
  // The value goes nowhere on purpose: see the header. A `bool_key` would need
  // somewhere to write, and the only honest place for that is a member nothing
  // reads - which is worse than a sink that says so.
  settings.add_key_to_path("/settings/facts").add_bool(fact_set, nscapi::settings_helper::bool_fun_key([](bool) {}, false), title, description);
}

std::string to_iso8601(const std::time_t when) { return format_time(when, "%Y-%m-%dT%H:%M:%SZ"); }
std::string to_date(const std::time_t when) { return format_time(when, "%Y-%m-%d"); }

// ---------------------------------------------------------------------------
// section
// ---------------------------------------------------------------------------

section &section::value(const std::string &key, const std::string &text) {
  // An unknown value is omitted, never written as an empty string: the
  // difference matters to a server diffing yesterday's inventory.
  if (!text.empty()) node_->set_scalar(key, quote(text));
  return *this;
}
section &section::value(const std::string &key, const char *text) { return value(key, text == nullptr ? std::string() : std::string(text)); }
section &section::value(const std::string &key, const int number) { return value(key, static_cast<long long>(number)); }
section &section::value(const std::string &key, const long long number) {
  node_->set_scalar(key, std::to_string(number));
  return *this;
}
section &section::value(const std::string &key, const unsigned long long number) {
  node_->set_scalar(key, std::to_string(number));
  return *this;
}
section &section::value(const std::string &key, const double number) {
  node_->set_scalar(key, number_to_string(number));
  return *this;
}
section &section::value(const std::string &key, const bool flag) {
  node_->set_scalar(key, flag ? "true" : "false");
  return *this;
}

section &section::time(const std::string &key, const std::time_t when) { return value(key, to_iso8601(when)); }
section &section::time(const std::string &key, const std::string &iso8601) { return value(key, iso8601); }
section &section::date(const std::string &key, const std::time_t when) { return value(key, to_date(when)); }
section &section::date(const std::string &key, const std::string &yyyy_mm_dd) { return value(key, yyyy_mm_dd); }

section section::sub(const std::string &key) { return section(node_->child(key, detail::node::object_kind)); }

::nscapi::facts::list section::list(const std::string &key) { return ::nscapi::facts::list(node_->child(key, detail::node::array_kind)); }

section &section::strings(const std::string &key, const std::vector<std::string> &values) {
  detail::node *target = node_->child(key, detail::node::array_kind);
  target->entries.clear();
  for (const std::string &entry : values) {
    if (entry.empty()) continue;
    detail::node *appended = target->append();
    appended->type = detail::node::scalar_kind;
    appended->scalar = quote(entry);
  }
  return *this;
}

// ---------------------------------------------------------------------------
// list
// ---------------------------------------------------------------------------

section list::record(const std::string &id) {
  detail::node *appended = node_->append();
  appended->type = detail::node::object_kind;
  section created(appended);
  // An empty id is left out on purpose: the core then rejects the whole set
  // with "has a record without a non-empty string 'id'", which names the
  // producer's bug. Inventing an id here would hide it, and the server would
  // diff on something that is not stable across runs.
  created.value("id", id);
  return created;
}

std::size_t list::size() const { return node_->entries.size(); }

// ---------------------------------------------------------------------------
// request
// ---------------------------------------------------------------------------

namespace {

void skip_ws(const std::string &s, std::size_t &i) {
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
}

// Parse one JSON string literal at s[i]=='"', resolving standard escapes.
// Deliberately not a general parser: the request is one flat object the core
// wrote, and core_wrapper::parse_tags_json makes the same trade for the same
// reason.
bool parse_string(const std::string &s, std::size_t &i, std::string &out) {
  if (i >= s.size() || s[i] != '"') return false;
  ++i;
  out.clear();
  while (i < s.size()) {
    const char c = s[i++];
    if (c == '"') return true;
    if (c != '\\') {
      out += c;
      continue;
    }
    if (i >= s.size()) return false;
    const char escape = s[i++];
    switch (escape) {
      case '"':
        out += '"';
        break;
      case '\\':
        out += '\\';
        break;
      case '/':
        out += '/';
        break;
      case 'b':
        out += '\b';
        break;
      case 'f':
        out += '\f';
        break;
      case 'n':
        out += '\n';
        break;
      case 'r':
        out += '\r';
        break;
      case 't':
        out += '\t';
        break;
      case 'u': {
        if (i + 4 > s.size()) return false;
        unsigned int code = 0;
        for (int digit = 0; digit < 4; ++digit) {
          const char h = s[i + digit];
          code <<= 4;
          if (h >= '0' && h <= '9') {
            code |= static_cast<unsigned int>(h - '0');
          } else if (h >= 'a' && h <= 'f') {
            code |= static_cast<unsigned int>(h - 'a' + 10);
          } else if (h >= 'A' && h <= 'F') {
            code |= static_cast<unsigned int>(h - 'A' + 10);
          } else {
            return false;
          }
        }
        i += 4;
        // The core only \u-escapes control characters, so no surrogate pair
        // ever reaches this.
        if (code < 0x80) {
          out += static_cast<char>(code);
        } else if (code < 0x800) {
          out += static_cast<char>(0xc0 | (code >> 6));
          out += static_cast<char>(0x80 | (code & 0x3f));
        } else {
          out += static_cast<char>(0xe0 | (code >> 12));
          out += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
          out += static_cast<char>(0x80 | (code & 0x3f));
        }
        break;
      }
      default:
        return false;
    }
  }
  return false;
}

}  // namespace

request request::parse(const std::string &json) {
  request result;
  std::size_t i = 0;
  skip_ws(json, i);
  if (i >= json.size() || json[i] != '{') return result;
  ++i;
  skip_ws(json, i);
  if (i < json.size() && json[i] == '}') return result;
  while (i < json.size()) {
    skip_ws(json, i);
    std::string key;
    if (!parse_string(json, i, key)) return result;
    skip_ws(json, i);
    if (i >= json.size() || json[i] != ':') return result;
    ++i;
    skip_ws(json, i);
    if (i >= json.size()) return result;
    if (json[i] == '[') {
      ++i;
      skip_ws(json, i);
      while (i < json.size() && json[i] != ']') {
        std::string entry;
        if (!parse_string(json, i, entry)) return result;
        if (key == "enabled" && !entry.empty()) result.enabled_.push_back(entry);
        skip_ws(json, i);
        if (i < json.size() && json[i] == ',') {
          ++i;
          skip_ws(json, i);
        }
      }
      if (i >= json.size()) return result;
      ++i;  // ']'
    } else if (json[i] == '"') {
      std::string text;
      if (!parse_string(json, i, text)) return result;
      if (key == "reason") result.reason_ = text;
    } else {
      // A value shape this request never carries. Nothing sensible is left to
      // read, and guessing where it ends would be worse than stopping.
      return result;
    }
    skip_ws(json, i);
    if (i < json.size() && json[i] == ',') {
      ++i;
      continue;
    }
    return result;
  }
  return result;
}

bool request::wants(const std::string &fact_set) const { return std::find(enabled_.begin(), enabled_.end(), fact_set) != enabled_.end(); }

// ---------------------------------------------------------------------------
// response
// ---------------------------------------------------------------------------

response::response() : sets_(new detail::node()) {}
response::~response() = default;

section response::set(const std::string &name) {
  // `name` is the top-level key. A dotted set is produced as
  // `set("software").list("installed")`, which is why this never splits on a
  // dot: the nesting is the producer's to express.
  return section(sets_->child(name, detail::node::object_kind));
}

void response::remove(const std::string &fact_set) {
  const std::string::size_type dot = fact_set.find('.');
  if (dot == std::string::npos) {
    detail::node *target = sets_->child(fact_set, detail::node::scalar_kind);
    target->scalar = "null";
    return;
  }
  detail::node *parent = sets_->child(fact_set.substr(0, dot), detail::node::object_kind);
  detail::node *target = parent->child(fact_set.substr(dot + 1), detail::node::scalar_kind);
  target->scalar = "null";
}

void response::error(const std::string &fact_set, const std::string &message) {
  for (auto &entry : errors_) {
    if (entry.first == fact_set) {
      entry.second = message;
      return;
    }
  }
  errors_.emplace_back(fact_set, message);
}

std::string response::serialize() const {
  std::string out;
  out += "{\"sets\":";
  sets_->write(out);
  out += ",\"errors\":{";
  bool first = true;
  for (const auto &entry : errors_) {
    if (!first) out += ',';
    first = false;
    out += quote(entry.first);
    out += ':';
    out += quote(entry.second);
  }
  out += "}}";
  return out;
}

}  // namespace facts
}  // namespace nscapi
