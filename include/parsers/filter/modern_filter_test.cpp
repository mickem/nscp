/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <parsers/filter/modern_filter.hpp>

// Provide the NSCAPI singleton so modern_filter.cpp can link.
// The core_wrapper is constructed with null function pointers, which means
// all log calls are harmless no-ops.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

// ============================================================================
// match_result tests
// ============================================================================

TEST(MatchResult, DefaultConstruction) {
  const modern_filter::match_result r;
  EXPECT_FALSE(r.matched_filter);
  EXPECT_FALSE(r.matched_bound);
  EXPECT_FALSE(r.is_done_);
}

TEST(MatchResult, TwoArgConstruction) {
  const modern_filter::match_result r(true, false);
  EXPECT_TRUE(r.matched_filter);
  EXPECT_FALSE(r.matched_bound);
  EXPECT_FALSE(r.is_done_);

  const modern_filter::match_result r2(false, true);
  EXPECT_FALSE(r2.matched_filter);
  EXPECT_TRUE(r2.matched_bound);
  EXPECT_FALSE(r2.is_done_);
}

TEST(MatchResult, CopyConstruction) {
  modern_filter::match_result original(true, true);
  original.is_done_ = true;

  const modern_filter::match_result copy(original);
  EXPECT_TRUE(copy.matched_filter);
  EXPECT_TRUE(copy.matched_bound);
  EXPECT_TRUE(copy.is_done_);
}

TEST(MatchResult, CopyAssignment) {
  modern_filter::match_result original(true, false);
  original.is_done_ = true;

  const modern_filter::match_result assigned = original;
  EXPECT_TRUE(assigned.matched_filter);
  EXPECT_FALSE(assigned.matched_bound);
  EXPECT_TRUE(assigned.is_done_);
}

TEST(MatchResult, AppendBothFalse) {
  modern_filter::match_result base;
  const modern_filter::match_result other;
  base.append(other);

  EXPECT_FALSE(base.matched_filter);
  EXPECT_FALSE(base.matched_bound);
  EXPECT_FALSE(base.is_done_);
}

TEST(MatchResult, AppendSetsFilterFromOther) {
  modern_filter::match_result base;
  const modern_filter::match_result other(true, false);
  base.append(other);

  EXPECT_TRUE(base.matched_filter);
  EXPECT_FALSE(base.matched_bound);
  EXPECT_FALSE(base.is_done_);
}

TEST(MatchResult, AppendSetsBoundFromOther) {
  modern_filter::match_result base;
  const modern_filter::match_result other(false, true);
  base.append(other);

  EXPECT_FALSE(base.matched_filter);
  EXPECT_TRUE(base.matched_bound);
  EXPECT_FALSE(base.is_done_);
}

TEST(MatchResult, AppendSetsDoneFromOther) {
  modern_filter::match_result base;
  modern_filter::match_result other;
  other.is_done_ = true;
  base.append(other);

  EXPECT_FALSE(base.matched_filter);
  EXPECT_FALSE(base.matched_bound);
  EXPECT_TRUE(base.is_done_);
}

TEST(MatchResult, AppendPreservesExistingTrue) {
  modern_filter::match_result base(true, true);
  base.is_done_ = true;
  const modern_filter::match_result other;
  base.append(other);

  EXPECT_TRUE(base.matched_filter);
  EXPECT_TRUE(base.matched_bound);
  EXPECT_TRUE(base.is_done_);
}

TEST(MatchResult, AppendCombinesBothTrue) {
  modern_filter::match_result base(true, false);
  modern_filter::match_result other(false, true);
  other.is_done_ = true;
  base.append(other);

  EXPECT_TRUE(base.matched_filter);
  EXPECT_TRUE(base.matched_bound);
  EXPECT_TRUE(base.is_done_);
}

TEST(MatchResult, AppendMultipleTimes) {
  modern_filter::match_result base;

  const modern_filter::match_result r1(true, false);
  base.append(r1);
  EXPECT_TRUE(base.matched_filter);
  EXPECT_FALSE(base.matched_bound);
  EXPECT_FALSE(base.is_done_);

  const modern_filter::match_result r2(false, true);
  base.append(r2);
  EXPECT_TRUE(base.matched_filter);
  EXPECT_TRUE(base.matched_bound);
  EXPECT_FALSE(base.is_done_);

  modern_filter::match_result r3;
  r3.is_done_ = true;
  base.append(r3);
  EXPECT_TRUE(base.matched_filter);
  EXPECT_TRUE(base.matched_bound);
  EXPECT_TRUE(base.is_done_);
}

TEST(MatchResult, AppendIsIdempotent) {
  modern_filter::match_result base(true, true);
  base.is_done_ = true;
  modern_filter::match_result other(true, true);
  other.is_done_ = true;
  base.append(other);

  EXPECT_TRUE(base.matched_filter);
  EXPECT_TRUE(base.matched_bound);
  EXPECT_TRUE(base.is_done_);
}

// ============================================================================
// error_handler_impl tests
// ============================================================================

TEST(ErrorHandlerImpl, InitialStateNoErrors) {
  const modern_filter::error_handler_impl handler(false);
  EXPECT_FALSE(handler.has_errors());
  EXPECT_TRUE(handler.get_errors().empty());
}

TEST(ErrorHandlerImpl, DebugFlagFromConstructor) {
  const modern_filter::error_handler_impl handler_no_debug(false);
  EXPECT_FALSE(handler_no_debug.is_debug());

  const modern_filter::error_handler_impl handler_debug(true);
  EXPECT_TRUE(handler_debug.is_debug());
}

TEST(ErrorHandlerImpl, SetDebug) {
  modern_filter::error_handler_impl handler(false);
  EXPECT_FALSE(handler.is_debug());

  handler.set_debug(true);
  EXPECT_TRUE(handler.is_debug());

  handler.set_debug(false);
  EXPECT_FALSE(handler.is_debug());
}

TEST(ErrorHandlerImpl, LogErrorStoresMessage) {
  modern_filter::error_handler_impl handler(false);
  handler.log_error("something went wrong");

  EXPECT_TRUE(handler.has_errors());
  EXPECT_EQ(handler.get_errors(), "something went wrong");
}

TEST(ErrorHandlerImpl, LogErrorOverwritesPreviousError) {
  modern_filter::error_handler_impl handler(false);
  handler.log_error("first error");
  handler.log_error("second error");

  EXPECT_TRUE(handler.has_errors());
  EXPECT_EQ(handler.get_errors(), "second error");
}

TEST(ErrorHandlerImpl, LogWarningDoesNotSetError) {
  modern_filter::error_handler_impl handler(false);
  handler.log_warning("a warning");

  EXPECT_FALSE(handler.has_errors());
  EXPECT_TRUE(handler.get_errors().empty());
}

TEST(ErrorHandlerImpl, LogDebugDoesNotSetError) {
  modern_filter::error_handler_impl handler(true);
  handler.log_debug("debug info");

  EXPECT_FALSE(handler.has_errors());
  EXPECT_TRUE(handler.get_errors().empty());
}

TEST(ErrorHandlerImpl, LogErrorAfterWarningAndDebug) {
  modern_filter::error_handler_impl handler(true);
  handler.log_warning("warn1");
  handler.log_debug("dbg1");
  EXPECT_FALSE(handler.has_errors());

  handler.log_error("real error");
  EXPECT_TRUE(handler.has_errors());
  EXPECT_EQ(handler.get_errors(), "real error");
}

TEST(ErrorHandlerImpl, LogErrorWithEmptyString) {
  modern_filter::error_handler_impl handler(false);
  handler.log_error("");

  // Empty string is stored — has_errors() checks !error.empty()
  EXPECT_FALSE(handler.has_errors());
  EXPECT_EQ(handler.get_errors(), "");
}

TEST(ErrorHandlerImpl, PolymorphicUsageThroughInterface) {
  std::shared_ptr<parsers::where::error_handler_interface> iface(new modern_filter::error_handler_impl(false));

  EXPECT_FALSE(iface->is_debug());
  iface->set_debug(true);
  EXPECT_TRUE(iface->is_debug());

  iface->log_error("interface error");
  // Downcast to check has_errors (not on the interface)
  auto *impl = dynamic_cast<modern_filter::error_handler_impl *>(iface.get());
  ASSERT_NE(impl, nullptr);
  EXPECT_TRUE(impl->has_errors());
  EXPECT_EQ(impl->get_errors(), "interface error");
}

// ============================================================================
// perf_config_parser tests
// ============================================================================

// Minimal factory exposing only what perf_config_parser::parse needs.
struct mock_perf_factory {
  std::map<std::string, std::map<std::string, std::string> > configs;
  void add_perf_config(const std::string &name, const std::map<std::string, std::string> &options) { configs[name] = options; }
};

namespace {
std::shared_ptr<parsers::where::error_handler_interface> make_error_handler() {
  return std::shared_ptr<parsers::where::error_handler_interface>(new modern_filter::error_handler_impl(false));
}
}  // namespace

// Regression test for "perf-config=none" reporting "Failed to parse syntax".
// "none" (and an empty string) must be accepted as "no perf-config" rather
// than fed to the grammar, which only accepts keyword(options) form.
TEST(PerfConfigParser, NoneIsAcceptedAndAddsNoConfig) {
  modern_filter::perf_config_parser<mock_perf_factory> parser;
  std::shared_ptr<mock_perf_factory> factory(new mock_perf_factory());
  auto error = make_error_handler();

  EXPECT_TRUE(parser.parse(factory, "none", error));
  EXPECT_TRUE(factory->configs.empty());
  EXPECT_FALSE(error->is_debug());
}

TEST(PerfConfigParser, EmptyStringIsAccepted) {
  modern_filter::perf_config_parser<mock_perf_factory> parser;
  std::shared_ptr<mock_perf_factory> factory(new mock_perf_factory());

  EXPECT_TRUE(parser.parse(factory, "", make_error_handler()));
  EXPECT_TRUE(factory->configs.empty());
}

TEST(PerfConfigParser, ValidConfigIsParsedAndStored) {
  modern_filter::perf_config_parser<mock_perf_factory> parser;
  std::shared_ptr<mock_perf_factory> factory(new mock_perf_factory());

  EXPECT_TRUE(parser.parse(factory, "cpu(unit:%)", make_error_handler()));
  ASSERT_EQ(1u, factory->configs.size());
  ASSERT_EQ(1u, factory->configs.count("cpu"));
  EXPECT_EQ("%", factory->configs["cpu"]["unit"]);
}

TEST(PerfConfigParser, InvalidConfigStillFails) {
  modern_filter::perf_config_parser<mock_perf_factory> parser;
  std::shared_ptr<mock_perf_factory> factory(new mock_perf_factory());
  auto error = make_error_handler();

  // Bare keyword without "(options)" is not "none" and is not valid syntax.
  EXPECT_FALSE(parser.parse(factory, "bogus", error));
}
