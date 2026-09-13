// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <string>

// Extract the class a WQL query reads from, so `check_wmi` can hold it against
// an allow list.
//
// This is a gate, not a parser: its job is to answer "which class does WMI
// read if I hand it this string" and to answer *nothing* rather than guess.
// Every form it cannot account for exactly - ASSOCIATORS OF, REFERENCES OF, a
// statement separator, a class path with a namespace or machine prefix - is
// refused, because a wrong answer here is a bypass: the gate would approve one
// class and WMI would read another.
//
// The cost is that a site restricting by class can only use plain
// `SELECT ... FROM <class> [WHERE ...]`. Anything more elaborate has to be
// configured as a predefined query, where the operator has already vouched for
// it. That is the right side to err on, and it is what the documentation says.

namespace check {
namespace wql {

struct parse_result {
  bool ok;
  std::string class_name;
  std::string error;

  parse_result() : ok(false) {}
};

// Accepts: optional whitespace, SELECT, a column list which is `*` or a
// comma-separated list of identifiers and nothing else, FROM, a bare class
// identifier, and optionally a WHERE clause (whose contents are not our
// business - WMI evaluates it against the class we just approved, it cannot
// reach another one).
//
// The column list is spelled out in the grammar rather than checked afterwards
// because a looser one lets the class move. With the column list written as
// "anything without a semicolon", `SELECT a FROM Win32_Foo FROM Win32_Allowed`
// matched with the columns swallowing the first FROM, and the gate reported
// the *last* class in the query while WMI would read the first.
inline parse_result extract_class(const std::string &query) {
  parse_result result;

  // One query per call. A separator means there may be a second statement
  // whose class we have not looked at.
  if (query.find(';') != std::string::npos) {
    result.error = "the query contains ';' and only a single statement can be checked";
    return result;
  }
  if (query.find('\0') != std::string::npos) {
    result.error = "the query contains a NUL character";
    return result;
  }

  // A WQL class name is a bare identifier. Refusing `:`, `\` and `/` here is
  // what keeps a query from naming a class in another namespace or on another
  // machine (`\\\\host\\root\\cimv2:Win32_Process`) and slipping past a check
  // which only compared the trailing identifier.
  // A column or class named after a keyword leaves the statement ambiguous -
  // `SELECT FROM FROM Win32_Allowed` parses two ways and WMI picks one of them
  // - so no identifier here may be one. `\\b` keeps `FROMAGE` an ordinary name.
  static const std::string ident = "(?!(?:FROM|WHERE|SELECT)\\b)[A-Za-z_][A-Za-z0-9_]*";
  static const boost::regex re(
      "\\A\\s*SELECT\\s+"
      "(\\*|" +
          ident + "(?:\\s*,\\s*" + ident +
          ")*)"
          "\\s+FROM\\s+(" +
          ident + ")\\s*(?:WHERE\\s+(.*))?\\z",
      boost::regex::perl | boost::regex::icase | boost::regex::mod_s);

  boost::smatch what;
  if (!boost::regex_match(query, what, re)) {
    result.error =
        "only a plain 'SELECT ... FROM <class> [WHERE ...]' query can be checked against 'allowed classes'; "
        "configure it as a predefined query instead";
    return result;
  }

  result.ok = true;
  result.class_name = what[2].str();
  return result;
}

}  // namespace wql
}  // namespace check
