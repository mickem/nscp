// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <exception>
#include <functional>
#include <limits>
#include <settings/settings_interface.hpp>
#include <str/utf8.hpp>
#include <str/xtos.hpp>
#include <string>
#include <vector>

#include "diagnostics_round.hpp"
#include "fact_repository.hpp"
#include "failed_check_store.hpp"
#include "metrics_prune.hpp"

namespace nsclient {
namespace core {

// What the core makes of its own settings: the values an operator typed,
// turned into what the scheduler, the facts repository and the failed-check
// log are configured with.
//
// The rule every one of them follows is the same - a value that cannot be used
// is reported and what was in force before stays in force - and it is the part
// that nothing reached while it lived inside NSClientT, which needs a whole
// agent around it to run. Here the settings are a function from a key to its
// text, and what would have been logged comes back as a list, so a test can
// say "max entries = lots" and read both the outcome and the complaint.
namespace core_settings {

// One section's values: the text of `key`, or `fallback` when it is not set.
typedef std::function<std::string(const std::string &key, const std::string &fallback)> reader;

constexpr const char *const failed_checks_section = "/settings/core/failed checks";

// Hand an interval to the scheduler, and the default instead when the
// scheduler will not have it. `add` is called with the interval and throws for
// one it cannot read. `what` names the setting in the error - quoted the way it
// should read, as in "'metrics interval'".
//
// True when `value` was taken as given.
template <class Add>
bool schedule_or_fallback(Add add, const std::string &what, const std::string &value, const std::string &fallback, std::vector<std::string> &errors) {
  try {
    add(value);
    return true;
  } catch (const std::exception &e) {
    errors.push_back("Invalid " + what + " value '" + value + "', falling back to '" + fallback + "': " + utf8::utf8_from_native(e.what()));
  }
  add(fallback);
  return false;
}

// A byte or row count as an operator typed it: digits only. Not
// str::stox<std::size_t>, which is lexical_cast and takes "-1" as the largest
// size_t - a cap removed by a typo.
constexpr const char *const not_a_size = "not a non-negative whole number";
inline bool parse_size(const std::string &text, std::size_t &out) {
  unsigned long long value = 0;
  if (!str::parse_unsigned(text, value) || value > (std::numeric_limits<std::size_t>::max)()) return false;
  out = static_cast<std::size_t>(value);
  return true;
}

// [/settings/facts] max size, applied to the repository. A cap that cannot be
// used leaves the previous one in force; a cap lowered under the document the
// repository holds drops the largest sets until the rest fits, and says which.
inline void apply_facts_max_size(fact_repository &facts, const std::string &max_size, std::vector<std::string> &errors) {
  std::size_t value = 0;
  if (!parse_size(max_size, value)) {
    errors.push_back("Invalid facts 'max size' value '" + max_size + "', keeping the previous one: " + not_a_size);
    return;
  }
  try {
    if (value < fact_repository::min_max_size) {
      errors.push_back("Facts 'max size' " + max_size + " is too small to hold any fact set; keeping the previous cap of " + str::xtos(facts.get_max_size()) +
                       " bytes.");
      return;
    }
    for (const std::string &fact_set : facts.set_max_size(value)) {
      errors.push_back("Dropped the fact set '" + fact_set + "': the facts document no longer fits the lowered [/settings/facts] max size of " + max_size +
                       " bytes. Disable a set, or raise max size.");
    }
  } catch (const std::exception &e) {
    errors.push_back("Invalid facts 'max size' value '" + max_size + "', keeping the previous one: " + utf8::utf8_from_native(e.what()));
  }
}

// [/settings/core/failed checks], read on top of `options` - the configuration
// in force, which is what a value that cannot be read falls back to. That
// makes a reload with one bad key keep that key as it was and still apply the
// rest. Range limits are not applied here: failed_check_store::configure()
// clamps, so they hold for every caller.
inline failed_check_store::options read_failed_checks(const reader &get, failed_check_store::options options, std::vector<std::string> &errors) {
  const std::string section = std::string("[") + failed_checks_section + "] ";
  // A byte or row count. A value that does not parse keeps `value` as it was.
  const auto read_size = [&get, &section, &errors](const std::string &key, const std::size_t fallback, std::size_t &value) {
    const std::string text = get(key, str::xtos(fallback));
    if (!parse_size(text, value)) errors.push_back("Invalid " + section + key + " '" + text + "', keeping " + str::xtos(value) + ": " + not_a_size);
  };

  options.enabled = settings::settings_interface::string_to_bool(get("enabled", "true"));

  const std::string arguments = get("record arguments", failed_check_store::format_argument_mode(failed_check_store::argument_mode::safe));
  std::string arguments_error;
  if (!failed_check_store::parse_argument_mode(arguments, options.record_arguments, arguments_error)) {
    errors.push_back("Invalid " + section + "record arguments '" + arguments + "', keeping '" +
                     failed_check_store::format_argument_mode(options.record_arguments) + "': " + arguments_error);
  }

  read_size("max entries", failed_check_store::default_max_entries, options.max_entries);

  options.capture_metrics = settings::settings_interface::string_to_bool(get("capture metrics", "false"));
  read_size("metrics context budget", failed_check_store::default_context_budget, options.context_budget);
  read_size("max context size", failed_check_store::default_max_context_size, options.max_context_size);

  const std::string prune = get("prune metrics", "");
  std::string prune_error;
  if (!metrics_prune::parse(prune, options.prune, prune_error)) {
    errors.push_back("Invalid " + section + "prune metrics '" + prune + "', keeping '" + metrics_prune::format(options.prune) + "': " + prune_error);
  }
  options.prune.patterns = metrics_prune::parse_patterns(get("prune pattern", ""));

  options.capture_diagnostics = settings::settings_interface::string_to_bool(get("capture diagnostics", "false"));
  read_size("diagnostics rows", diagnostics_round::default_max_rows, options.diagnostics_rows);
  options.diagnostics_rows = diagnostics_round::clamp_rows(options.diagnostics_rows);

  const std::string statuses = get("statuses", failed_check_store::format_statuses(failed_check_store::default_statuses));
  std::string statuses_error;
  unsigned mask = 0;
  if (failed_check_store::parse_statuses(statuses, mask, statuses_error)) {
    options.statuses = mask;
  } else {
    errors.push_back("Invalid " + section + "statuses '" + statuses + "', keeping '" + failed_check_store::format_statuses(options.statuses) +
                     "': " + statuses_error);
  }
  return options;
}

}  // namespace core_settings
}  // namespace core
}  // namespace nsclient
