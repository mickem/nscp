// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <set>
#include <string>
#include <utility>
#include <vector>

// The NCPA wire contract, free of any dispatch: what a request looks like on
// the way in (path, query string or form body, token) and what the answer
// looks like on the way out (the JSON bodies check_ncpa.py reads). Nothing in
// here talks to the core, so all of it unit-tests without one.
//
// The contract comes from the NCPA agent and plugin source:
//   client/check_ncpa.py, agent/listener/server.py, agent/listener/nodes.py,
//   agent/listener/pluginnodes.py (github.com/NagiosEnterprises/ncpa).
namespace ncpa {

// The request parameters, query string then form body, in order and with
// repeats: what Mongoose::Request::getVariablesVector() and
// Mongoose::Request::parseVariables() return.
typedef std::vector<std::pair<std::string, std::string> > form_vector;

// Split the raw (still percent-encoded) path below /api into its segments:
// split on '/' first, then decode each segment, so an argument check_ncpa sent
// with an escaped slash (`%2F`) stays one segment. Empty segments are dropped,
// which also absorbs the trailing '/' check_ncpa always puts after the metric.
std::vector<std::string> split_path(const std::string &raw);

// First value of `key`, or `fallback` when it is absent.
std::string form_value(const form_vector &form, const std::string &key, const std::string &fallback = "");
// Whether `key` is present at all (even with an empty value).
bool form_has(const form_vector &form, const std::string &key);
// Every value of `key`, in order.
std::vector<std::string> form_values(const form_vector &form, const std::string &key);

// Split the plugin arguments into tokens. NCPA joins the `args=` values and
// the path segments with spaces and splits the result again with shlex; this
// does the same split with the interactive prompt's tokenizer
// (str::utils::parse_prompt_command) with backslash escapes off:
//   - whitespace separates tokens; empty tokens are dropped;
//   - "..." groups and is removed;
//   - '...' groups and is removed only at the start of a token or right after
//     its first '=' (`'a b'`, `path='C:\x y'`); anywhere else it is kept, so a
//     filter's own quotes survive (`filter=core='total'`);
//   - a backslash is always an ordinary character, so Windows paths survive
//     (`path=C:\Windows\Temp`, `"path=C:\Temp\"`), as with NCPA on Windows.
// check_ncpa.py needs the second split: it tokenises -a with a non-POSIX
// shlex that keeps the quotes, so -a '"filter=load > 80"' arrives as the one
// segment `"filter=load > 80"`.
std::vector<std::string> split_args(const std::string &value);

enum class token_result {
  accepted,
  // No token is configured, so nothing can be accepted. Distinct from a wrong
  // token so the log can say what to fix; the caller sees the same answer.
  not_configured,
  rejected
};
// Check `given` against the primary token and, when one is set, the backup
// token, with str::constant_time_eq. Both comparisons always run, so the time
// taken does not say which one matched.
token_result check_token(const std::string &given, const std::string &primary, const std::string &backup);

// Which registered queries `plugins/` exposes.
struct plugin_policy {
  enum class mode_type {
    // Every registered query.
    any,
    // Only the commands CheckExternalScripts registers (external scripts and
    // its aliases).
    scripts,
    // Only the names listed.
    list
  };
  mode_type mode = mode_type::any;
  // Lower-cased; only used with mode_type::list.
  std::set<std::string> names;

  // Parse the `plugins` setting: `any`, `scripts`, or a comma-separated list
  // of query names. An empty value is an error (it would expose nothing, which
  // is never what an operator who writes it means).
  static bool parse(const std::string &value, plugin_policy &out, std::string &error);

  // Whether the query `name`, registered by the module `module` (its name,
  // not the alias it was loaded under), is exposed.
  bool allows(const std::string &name, const std::string &module) const;
  // Whether allows() looks at the module at all (only `scripts` does), so a
  // caller can skip resolving it.
  bool needs_module() const { return mode == mode_type::scripts; }

  std::string to_string() const;
};

// The module name CheckExternalScripts registers its commands under.
extern const char *const kScriptsModule;

// Turn what a query returned into the text a Nagios plugin prints: the message,
// then `|` and the performance data when there is any. Line endings are
// normalised to '\n' and surrounding whitespace is trimmed, as NCPA does with a
// plugin's output.
std::string nagios_output(const std::string &message, const std::string &perf);

// Whether a request parameter reads as true: present, non-empty and not one of
// 0/false/no/off (any case). check_ncpa sends `check=1` and, when -d is not
// given, the literal `delta=False`, which therefore has to read as false.
bool is_truthy(const std::string &value);

// ---- Response bodies -------------------------------------------------------

// `{"error": "<message>"}`: check_ncpa turns it into CRITICAL with the message.
std::string error_body(const std::string &message);

// `{"returncode": <rc>, "stdout": "<text>"}`: the answer to a check.
std::string check_body(int returncode, const std::string &stdout_text);

// `{"<node>": ["a", "b", ...]}`: a node listing names, such as /api/plugins.
std::string list_body(const std::string &node, const std::vector<std::string> &names);

// What NCPA answers for a path that names nothing. In list mode it is an error
// object carrying the path, code 100 and which kind of node was missing; in
// check mode it is an UNKNOWN result.
std::string missing_node_body(const std::string &full_path, const std::string &node_type, const std::string &name);
std::string missing_node_check_body(const std::string &node_type, const std::string &name);

}  // namespace ncpa
