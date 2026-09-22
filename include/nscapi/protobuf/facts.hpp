// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/dll_defines_protobuf.hpp>

#ifdef WIN32
#pragma warning(push)
#pragma warning(disable : 4018)
#pragma warning(disable : 4100)
#pragma warning(disable : 4913)
#pragma warning(disable : 4512)
#pragma warning(disable : 4244)
#pragma warning(disable : 4127)
#pragma warning(disable : 4251)
#pragma warning(disable : 4275)
#pragma warning(disable : 4996)
#include <protobuf/facts.pb.h>
#pragma warning(pop)
#else
#include <protobuf/facts.pb.h>
#endif

#include <algorithm>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

// Working with the facts tree (PB::Facts::Value / Object / List).
//
// Header-only and protobuf-only, because both sides need it: the core stores
// and canonicalises the document, while a consumer - the REST controller, the
// `nscp test` console - walks what the core hands back. The producer side is
// elsewhere (nscapi/nscapi_facts_helper.hpp), which builds this tree rather
// than reading it.
namespace nscapi {
namespace facts {
namespace tree {

// The field named `key`, or null. Linear, which is the right shape here: an
// object in this document has a handful of fields, and keeping them in a
// sorted vector is what makes the encoding canonical (see sort_fields).
inline const PB::Facts::Field *find(const PB::Facts::Object &object, const std::string &key) {
  for (const PB::Facts::Field &field : object.fields()) {
    if (field.key() == key) return &field;
  }
  return nullptr;
}

inline const PB::Facts::Value *get(const PB::Facts::Object &object, const std::string &key) {
  const PB::Facts::Field *field = find(object, key);
  return field == nullptr ? nullptr : &field->value();
}

// Sort every object in the tree by key, in place.
//
// This is the document's canonical order, and the core applies it to a fact
// set as it accepts it. Two consequences hang off that: the serialised bytes
// of two collections of the same inventory compare equal (so a producer
// returning the same facts in a different order does not move the revision),
// and the JSON rendering below is canonical without sorting again.
//
// Lists are left alone. A list is ordered data - the order a producer reports
// its records in is the order it means - and sorting it would be a change to
// the document rather than to its encoding.
inline void sort_fields(PB::Facts::Object *object) {
  if (object == nullptr) return;
  // Copied out, sorted, put back rather than std::sort over the repeated
  // field in place: sorting a RepeatedPtrField swaps the elements themselves,
  // which is the one operation on a protobuf message whose cost and arena
  // behaviour depend on how the library was built. An object here holds a
  // handful of fields and is sorted once, when the core accepts the set.
  std::vector<PB::Facts::Field> fields(object->fields().begin(), object->fields().end());
  std::sort(fields.begin(), fields.end(), [](const PB::Facts::Field &lhs, const PB::Facts::Field &rhs) { return lhs.key() < rhs.key(); });
  object->clear_fields();
  for (const PB::Facts::Field &field : fields) *object->add_fields() = field;
  for (PB::Facts::Field &field : *object->mutable_fields()) {
    PB::Facts::Value *value = field.mutable_value();
    if (value->has_object_value()) {
      sort_fields(value->mutable_object_value());
    } else if (value->has_list_value()) {
      for (PB::Facts::Value &item : *value->mutable_list_value()->mutable_values()) {
        if (item.has_object_value()) sort_fields(item.mutable_object_value());
      }
    }
  }
}

// A double, in the shortest decimal form that reads back as the same value.
//
// Through streams imbued with the classic locale, never printf or a default
// stringstream: both follow the locale, and this agent runs on hosts whose
// locale writes 1,5 - which is not a number in JSON, and not what the rest of
// the document's numbers look like either.
inline std::string format_double(const double value) {
  for (int precision = 15; precision <= 17; ++precision) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(precision) << value;
    const std::string text = out.str();
    std::istringstream in(text);
    in.imbue(std::locale::classic());
    double round_trip = 0;
    if ((in >> round_trip) && round_trip == value) return text;
  }
  return "0";
}

namespace detail {

// A JSON string literal: the six escapes JSON names, \u00XX for the remaining
// control characters, and every other byte through untouched. UTF-8 stays
// UTF-8 - the document is UTF-8 and so is JSON.
inline void write_json_string(const std::string &text, std::string &out) {
  static const char *digits = "0123456789abcdef";
  out += '"';
  for (const char raw : text) {
    const unsigned char c = static_cast<unsigned char>(raw);
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
        if (c < 0x20) {
          out += "\\u00";
          out += digits[(c >> 4) & 0xf];
          out += digits[c & 0xf];
        } else {
          out += raw;
        }
    }
  }
  out += '"';
}

}  // namespace detail

inline void write_json(const PB::Facts::Value &value, std::string &out);

inline void write_json(const PB::Facts::Object &object, std::string &out) {
  out += '{';
  bool first = true;
  for (const PB::Facts::Field &field : object.fields()) {
    if (!first) out += ',';
    first = false;
    detail::write_json_string(field.key(), out);
    out += ':';
    write_json(field.value(), out);
  }
  out += '}';
}

inline void write_json(const PB::Facts::Value &value, std::string &out) {
  switch (value.kind_case()) {
    case PB::Facts::Value::kStringValue:
      detail::write_json_string(value.string_value(), out);
      return;
    case PB::Facts::Value::kIntValue:
      out += std::to_string(value.int_value());
      return;
    case PB::Facts::Value::kUintValue:
      out += std::to_string(value.uint_value());
      return;
    case PB::Facts::Value::kDoubleValue:
      out += format_double(value.double_value());
      return;
    case PB::Facts::Value::kBoolValue:
      out += value.bool_value() ? "true" : "false";
      return;
    case PB::Facts::Value::kObjectValue:
      write_json(value.object_value(), out);
      return;
    case PB::Facts::Value::kListValue: {
      out += '[';
      bool first = true;
      for (const PB::Facts::Value &item : value.list_value().values()) {
        if (!first) out += ',';
        first = false;
        write_json(item, out);
      }
      out += ']';
      return;
    }
    default:
      // A Value with nothing set cannot reach the document: an unknown value
      // is omitted rather than stored. Rendering it as null keeps a
      // hand-built message from producing invalid JSON.
      out += "null";
      return;
  }
}

// The document as JSON: no whitespace, and object keys in the order the tree
// holds them, which for a document the core stored is sorted (see
// sort_fields). This is the format the fleet upload sends and hashes - the
// agent's own storage and ABI are protobuf, but what crosses to another
// implementation is JSON, which has a canonical form to agree on.
inline std::string to_json(const PB::Facts::Object &object) {
  std::string out;
  write_json(object, out);
  return out;
}

inline std::string to_json(const PB::Facts::Value &value) {
  std::string out;
  write_json(value, out);
  return out;
}

}  // namespace tree
}  // namespace facts
}  // namespace nscapi
