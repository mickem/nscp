// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Reading the loosely typed JSON Icinga 2 sends. Icinga serialises custom
// variables as whatever the operator wrote - a threshold may arrive as a
// string, an integer or a double, a flag as a bool or as "1" - so every read
// goes through these helpers rather than through as_string()/as_bool() on the
// raw value, which throw on the "wrong" type.

#include <boost/json.hpp>
#include <string>
#include <vector>

namespace icinga_agent {
namespace json_util {

// The value under `key`, or nullptr when there is none.
const boost::json::value *find(const boost::json::object &obj, const char *key);

// A scalar rendered the way Icinga renders it into a command line: strings
// as they are, booleans as true/false, integral doubles without a fraction
// (Icinga's JSON writer sends 5 as 5.0), null as empty. Arrays and objects
// are serialised as JSON.
std::string to_text(const boost::json::value &v);

// to_text() of the value under `key`, or empty when there is none.
std::string text_field(const boost::json::object &obj, const char *key);

// Null, an empty string, an empty array or an empty object: what Icinga
// treats as "not set" when it decides whether to pass an argument.
bool is_empty(const boost::json::value &v);

// Icinga's truthiness for `set_if` and the boolean attributes: false, 0,
// "", "0", "false" and empty containers are false, everything else true.
bool is_truthy(const boost::json::value &v);

// The three readers a translation needs of the macros a master sends. The
// master sends a macro only when something gave it a value, so "not there"
// and "empty" are one case: Icinga leaves the argument out for both.

// There is a non-empty value under `key`.
bool has_value(const boost::json::object &obj, const char *key);

// is_truthy() of the value under `key`; false when there is none.
bool truthy_field(const boost::json::object &obj, const char *key);

// The value under `key` as a list of texts: the elements of an array, a
// scalar as a list of one, nothing when it is missing or empty.
std::vector<std::string> text_list(const boost::json::object &obj, const char *key);

// An integer attribute such as `order`, sent as a number or a string.
// `fallback` when it is neither.
long to_long(const boost::json::value &v, long fallback);

}  // namespace json_util
}  // namespace icinga_agent
