// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <parsers/where/dll_defines.hpp>

#include <cstddef>
#include <string>

namespace parsers {
namespace where {

// Bounds on the regular-expression matching the filter engine does.
//
// Both halves of a `=~` are untrusted: the pattern comes from a `filter`,
// `warning` or `critical` argument, and the subject is whatever the check is
// looking at - a log line, a process name, an event-log string - which a local
// unprivileged user can usually plant. A pattern like `(a+)+$` against a long
// non-matching subject backtracks exponentially. Boost.Regex has a built-in
// state-count ceiling, so a single match cannot run forever, but it can burn on
// the order of a second of CPU, and a check evaluates its filter once per
// record: over a large event log or a big file that is minutes of a pinned
// worker thread for one request.
//
// So the engine keeps a per-check budget: the total wall-clock time spent
// inside regex matching, across every record of one check, is capped. Past the
// cap, further matches are refused and reported (the check comes back UNKNOWN
// rather than silently under-matching). The budget is thread-local, because one
// check runs on one thread and concurrent checks must not spend each other's.
//
// `reset_regex_budget()` opens a new budget and is called by the filter driver
// at the start of every check (modern_filter::start_match). A thread that never
// calls it - a module matching outside the filter engine - simply keeps the
// budget it has; the first reset makes it exact.
NSCAPI_EXPORT void reset_regex_budget();

// Budget in milliseconds. Deliberately generous: a legitimate filter over a
// large event log spends far less, and anything approaching this is either a
// pathological pattern or a subject nobody meant to hand it. Exposed so tests
// can lower it instead of burning the real budget.
NSCAPI_EXPORT void set_regex_budget_ms(unsigned long budget_ms);
NSCAPI_EXPORT unsigned long get_regex_budget_ms();

// True when this thread has spent its budget. Matching keeps refusing until the
// next reset.
NSCAPI_EXPORT bool regex_budget_exhausted();

// Longest subject handed to the matcher. Backtracking cost grows with the
// subject, and no real check keyword is a megabyte long; a longer one is
// refused rather than truncated, because a silent truncation would turn a
// non-match into a match (or the other way round) with nothing to show for it.
NSCAPI_EXPORT std::size_t max_regex_subject_bytes();

}  // namespace where
}  // namespace parsers
