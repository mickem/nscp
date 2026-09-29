// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The per-check regex budget lives here rather than in operators.cpp, which is
// where it is spent, for a link-time reason.
//
// These five functions are exported from nscp_where_filter, because
// modern_filter.hpp calls reset_regex_budget() and that header is compiled into
// every check module. parsers_where_test, meanwhile, compiles operators.cpp
// straight into the test binary AND links nscp_where_filter, so any symbol that
// file defines and the library exports is defined twice on Windows: once in
// operators.obj and once by the import library, which is LNK2005. Keeping the
// definitions in a file only the library compiles leaves operators.cpp with
// nothing but references, and a reference resolves through the import library
// the way any other cross-DLL call does.
//
// The state is thread-local because one check runs on one thread: concurrent
// checks must not spend each other's budget. See regex_guard.hpp for what the
// budget is for.

#include <parsers/where/regex_guard.hpp>

namespace parsers {
namespace where {

namespace {
const unsigned long default_regex_budget_ms = 30000;
const std::size_t regex_subject_cap = 1024u * 1024u;

thread_local unsigned long regex_budget_ms = default_regex_budget_ms;
thread_local unsigned long regex_spent_ms = 0;
}  // namespace

void reset_regex_budget() { regex_spent_ms = 0; }
void set_regex_budget_ms(const unsigned long budget_ms) { regex_budget_ms = budget_ms; }
unsigned long get_regex_budget_ms() { return regex_budget_ms; }
bool regex_budget_exhausted() { return regex_spent_ms >= regex_budget_ms; }
std::size_t max_regex_subject_bytes() { return regex_subject_cap; }
void charge_regex_time_ms(const unsigned long spent_ms) { regex_spent_ms += spent_ms; }

}  // namespace where
}  // namespace parsers
