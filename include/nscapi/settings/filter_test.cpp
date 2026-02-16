/*
 * Copyright (C) 2004-2016 Michael Medin
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

#include <boost/optional/optional_io.hpp>
#include <nscapi/settings/filter.hpp>
#include <string>
#include <vector>

namespace nscapi {
namespace settings_filters {

// ============================================================================
// filter_object constructor and basic property tests
// ============================================================================

TEST(FilterObjectTest, ConstructorBasic) {
  const filter_object obj("top_syntax", "detail_syntax", "target");
  EXPECT_EQ("top_syntax", obj.syntax_top);
  EXPECT_EQ("detail_syntax", obj.syntax_detail);
  EXPECT_EQ("target", obj.target);
  EXPECT_FALSE(obj.debug);
  EXPECT_FALSE(obj.escape_html);
  EXPECT_EQ(-1, obj.severity);
}

TEST(FilterObjectTest, ConstructorEmptyStrings) {
  const filter_object obj("", "", "");
  EXPECT_EQ("", obj.syntax_top);
  EXPECT_EQ("", obj.syntax_detail);
  EXPECT_EQ("", obj.target);
}

TEST(FilterObjectTest, CopyConstructor) {
  filter_object original("top", "detail", "target");
  original.debug = true;
  original.escape_html = true;
  original.syntax_ok = "ok_syntax";
  original.filter_ok = "ok_filter";
  original.filter_warn = "warn_filter";
  original.filter_crit = "crit_filter";
  original.perf_config = "perf_cfg";
  original.severity = 1;
  original.command = "my_command";
  original.target_id = "target_123";
  original.source_id = "source_456";
  original.timeout_msg = "timed out";
  original.set_filter_string("my_filter");
  original.set_max_age("10m");
  original.set_silent_period("30s");

  const filter_object copy(original);

  EXPECT_EQ(original.syntax_top, copy.syntax_top);
  EXPECT_EQ(original.syntax_detail, copy.syntax_detail);
  EXPECT_EQ(original.target, copy.target);
  EXPECT_EQ(original.debug, copy.debug);
  EXPECT_EQ(original.escape_html, copy.escape_html);
  EXPECT_EQ(original.syntax_ok, copy.syntax_ok);
  EXPECT_EQ(original.filter_ok, copy.filter_ok);
  EXPECT_EQ(original.filter_warn, copy.filter_warn);
  EXPECT_EQ(original.filter_crit, copy.filter_crit);
  EXPECT_EQ(original.perf_config, copy.perf_config);
  EXPECT_EQ(original.severity, copy.severity);
  EXPECT_EQ(original.command, copy.command);
  EXPECT_EQ(original.target_id, copy.target_id);
  EXPECT_EQ(original.source_id, copy.source_id);
  EXPECT_EQ(original.timeout_msg, copy.timeout_msg);
  EXPECT_STREQ(original.filter_string(), copy.filter_string());
  EXPECT_EQ(original.max_age, copy.max_age);
  EXPECT_EQ(original.silent_period, copy.silent_period);
}

// ============================================================================
// filter_string getter/setter tests
// ============================================================================

TEST(FilterObjectTest, SetFilterString) {
  filter_object obj("", "", "");
  obj.set_filter_string("test_filter");
  EXPECT_STREQ("test_filter", obj.filter_string());
}

TEST(FilterObjectTest, SetFilterStringEmpty) {
  filter_object obj("", "", "");
  obj.set_filter_string("");
  EXPECT_STREQ("", obj.filter_string());
}

TEST(FilterObjectTest, SetFilterStringReplace) {
  filter_object obj("", "", "");
  obj.set_filter_string("first");
  EXPECT_STREQ("first", obj.filter_string());
  obj.set_filter_string("second");
  EXPECT_STREQ("second", obj.filter_string());
}

// ============================================================================
// parse_time tests
// ============================================================================

TEST(FilterObjectTest, ParseTimeSeconds) {
  filter_object obj("", "", "");
  EXPECT_EQ(boost::posix_time::seconds(30), obj.parse_time("30s"));
  EXPECT_EQ(boost::posix_time::seconds(30), obj.parse_time("30S"));
}

TEST(FilterObjectTest, ParseTimeMinutes) {
  filter_object obj("", "", "");
  EXPECT_EQ(boost::posix_time::minutes(5), obj.parse_time("5m"));
  EXPECT_EQ(boost::posix_time::minutes(5), obj.parse_time("5M"));
}

TEST(FilterObjectTest, ParseTimeHours) {
  filter_object obj("", "", "");
  EXPECT_EQ(boost::posix_time::hours(2), obj.parse_time("2h"));
  EXPECT_EQ(boost::posix_time::hours(2), obj.parse_time("2H"));
}

TEST(FilterObjectTest, ParseTimeDays) {
  filter_object obj("", "", "");
  EXPECT_EQ(boost::posix_time::hours(48), obj.parse_time("2d"));
  EXPECT_EQ(boost::posix_time::hours(48), obj.parse_time("2D"));
}

TEST(FilterObjectTest, ParseTimeWeeks) {
  filter_object obj("", "", "");
  EXPECT_EQ(boost::posix_time::hours(168), obj.parse_time("1w"));
  EXPECT_EQ(boost::posix_time::hours(168), obj.parse_time("1W"));
}

TEST(FilterObjectTest, ParseTimeNoSuffix) {
  filter_object obj("", "", "");
  EXPECT_EQ(boost::posix_time::seconds(45), obj.parse_time("45"));
}

TEST(FilterObjectTest, ParseTimeZero) {
  filter_object obj("", "", "");
  EXPECT_EQ(boost::posix_time::seconds(0), obj.parse_time("0s"));
  EXPECT_EQ(boost::posix_time::minutes(0), obj.parse_time("0m"));
}

// ============================================================================
// set_max_age tests
// ============================================================================

TEST(FilterObjectTest, SetMaxAgeValid) {
  filter_object obj("", "", "");
  obj.set_max_age("10m");
  ASSERT_TRUE(obj.max_age.is_initialized());
  EXPECT_EQ(boost::posix_time::minutes(10), obj.max_age.get());
}

TEST(FilterObjectTest, SetMaxAgeNone) {
  filter_object obj("", "", "");
  obj.set_max_age("none");
  EXPECT_FALSE(obj.max_age.is_initialized());
}

TEST(FilterObjectTest, SetMaxAgeInfinite) {
  filter_object obj("", "", "");
  obj.set_max_age("infinite");
  EXPECT_FALSE(obj.max_age.is_initialized());
}

TEST(FilterObjectTest, SetMaxAgeFalse) {
  filter_object obj("", "", "");
  obj.set_max_age("false");
  EXPECT_FALSE(obj.max_age.is_initialized());
}

TEST(FilterObjectTest, SetMaxAgeOff) {
  filter_object obj("", "", "");
  obj.set_max_age("off");
  EXPECT_FALSE(obj.max_age.is_initialized());
}

TEST(FilterObjectTest, SetMaxAgeSeconds) {
  filter_object obj("", "", "");
  obj.set_max_age("30s");
  ASSERT_TRUE(obj.max_age.is_initialized());
  EXPECT_EQ(boost::posix_time::seconds(30), obj.max_age.get());
}

// ============================================================================
// set_silent_period tests
// ============================================================================

TEST(FilterObjectTest, SetSilentPeriodValid) {
  filter_object obj("", "", "");
  obj.set_silent_period("5m");
  ASSERT_TRUE(obj.silent_period.is_initialized());
  EXPECT_EQ(boost::posix_time::minutes(5), obj.silent_period.get());
}

TEST(FilterObjectTest, SetSilentPeriodNone) {
  filter_object obj("", "", "");
  obj.set_silent_period("none");
  EXPECT_FALSE(obj.silent_period.is_initialized());
}

TEST(FilterObjectTest, SetSilentPeriodInfinite) {
  filter_object obj("", "", "");
  obj.set_silent_period("infinite");
  EXPECT_FALSE(obj.silent_period.is_initialized());
}

TEST(FilterObjectTest, SetSilentPeriodFalse) {
  filter_object obj("", "", "");
  obj.set_silent_period("false");
  EXPECT_FALSE(obj.silent_period.is_initialized());
}

TEST(FilterObjectTest, SetSilentPeriodOff) {
  filter_object obj("", "", "");
  obj.set_silent_period("off");
  EXPECT_FALSE(obj.silent_period.is_initialized());
}

// ============================================================================
// set_severity tests
// ============================================================================

TEST(FilterObjectTest, SetSeverityOK) {
  filter_object obj("", "", "");
  obj.set_severity("OK");
  EXPECT_EQ(NSCAPI::query_return_codes::returnOK, obj.severity);
}

TEST(FilterObjectTest, SetSeverityOKLowercase) {
  filter_object obj("", "", "");
  obj.set_severity("ok");
  EXPECT_EQ(NSCAPI::query_return_codes::returnOK, obj.severity);
}

TEST(FilterObjectTest, SetSeverityWarning) {
  filter_object obj("", "", "");
  obj.set_severity("WARNING");
  EXPECT_EQ(NSCAPI::query_return_codes::returnWARN, obj.severity);
}

TEST(FilterObjectTest, SetSeverityWarningLowercase) {
  filter_object obj("", "", "");
  obj.set_severity("warning");
  EXPECT_EQ(NSCAPI::query_return_codes::returnWARN, obj.severity);
}

TEST(FilterObjectTest, SetSeverityCritical) {
  filter_object obj("", "", "");
  obj.set_severity("CRITICAL");
  EXPECT_EQ(NSCAPI::query_return_codes::returnCRIT, obj.severity);
}

TEST(FilterObjectTest, SetSeverityCriticalLowercase) {
  filter_object obj("", "", "");
  obj.set_severity("critical");
  EXPECT_EQ(NSCAPI::query_return_codes::returnCRIT, obj.severity);
}

TEST(FilterObjectTest, SetSeverityUnknown) {
  filter_object obj("", "", "");
  obj.set_severity("UNKNOWN");
  EXPECT_EQ(NSCAPI::query_return_codes::returnUNKNOWN, obj.severity);
}

TEST(FilterObjectTest, SetSeverityInvalid) {
  filter_object obj("", "", "");
  obj.set_severity("invalid");
  EXPECT_EQ(NSCAPI::query_return_codes::returnUNKNOWN, obj.severity);
}

// ============================================================================
// apply_parent tests
// ============================================================================

TEST(FilterObjectTest, ApplyParentCopiesEmptyFields) {
  filter_object parent("parent_top", "parent_detail", "parent_target");
  parent.filter_warn = "parent_warn";
  parent.filter_crit = "parent_crit";
  parent.filter_ok = "parent_ok";
  parent.set_filter_string("parent_filter");
  parent.target_id = "parent_target_id";
  parent.source_id = "parent_source_id";
  parent.timeout_msg = "parent_timeout";
  parent.command = "parent_command";
  parent.severity = 2;

  filter_object child("", "", "");
  child.apply_parent(parent);

  EXPECT_EQ("parent_top", child.syntax_top);
  EXPECT_EQ("parent_detail", child.syntax_detail);
  EXPECT_EQ("parent_target", child.target);
  EXPECT_EQ("parent_warn", child.filter_warn);
  EXPECT_EQ("parent_crit", child.filter_crit);
  EXPECT_EQ("parent_ok", child.filter_ok);
  EXPECT_EQ("parent_target_id", child.target_id);
  EXPECT_EQ("parent_source_id", child.source_id);
  EXPECT_EQ("parent_timeout", child.timeout_msg);
  EXPECT_EQ("parent_command", child.command);
  EXPECT_EQ(2, child.severity);
}

TEST(FilterObjectTest, ApplyParentPreservesExistingFields) {
  filter_object parent("parent_top", "parent_detail", "parent_target");
  parent.filter_warn = "parent_warn";
  parent.filter_crit = "parent_crit";
  parent.severity = 2;

  filter_object child("child_top", "child_detail", "child_target");
  child.filter_warn = "child_warn";
  child.filter_crit = "child_crit";
  child.severity = 1;

  child.apply_parent(parent);

  EXPECT_EQ("child_top", child.syntax_top);
  EXPECT_EQ("child_detail", child.syntax_detail);
  EXPECT_EQ("child_target", child.target);
  EXPECT_EQ("child_warn", child.filter_warn);
  EXPECT_EQ("child_crit", child.filter_crit);
  EXPECT_EQ(1, child.severity);
}

TEST(FilterObjectTest, ApplyParentCopiesDebugFlag) {
  filter_object parent("", "", "");
  parent.debug = true;

  filter_object child("", "", "");
  child.debug = false;

  child.apply_parent(parent);

  EXPECT_TRUE(child.debug);
}

TEST(FilterObjectTest, ApplyParentDoesNotOverwriteDebugFlagWhenChildIsTrue) {
  filter_object parent("", "", "");
  parent.debug = false;

  filter_object child("", "", "");
  child.debug = true;

  child.apply_parent(parent);

  EXPECT_TRUE(child.debug);
}

TEST(FilterObjectTest, ApplyParentSeverityOnlyWhenChildIsUnset) {
  filter_object parent("", "", "");
  parent.severity = 2;

  filter_object child("", "", "");
  child.severity = -1;  // Unset

  child.apply_parent(parent);

  EXPECT_EQ(2, child.severity);
}

TEST(FilterObjectTest, ApplyParentSeverityPreservesChildValue) {
  filter_object parent("", "", "");
  parent.severity = 2;

  filter_object child("", "", "");
  child.severity = 0;  // Set to OK

  child.apply_parent(parent);

  EXPECT_EQ(0, child.severity);
}

TEST(FilterObjectTest, ApplyParentWithPartialFields) {
  filter_object parent("", "", "");
  parent.filter_warn = "parent_warn";
  parent.target_id = "parent_target_id";

  filter_object child("", "", "");
  child.filter_crit = "child_crit";
  child.source_id = "child_source_id";

  child.apply_parent(parent);

  EXPECT_EQ("parent_warn", child.filter_warn);
  EXPECT_EQ("child_crit", child.filter_crit);
  EXPECT_EQ("parent_target_id", child.target_id);
  EXPECT_EQ("child_source_id", child.source_id);
}

// ============================================================================
// to_string tests
// ============================================================================

TEST(FilterObjectTest, ToStringReturnsNonEmpty) {
  const filter_object obj("top", "detail", "target");
  const std::string result = obj.to_string();
  EXPECT_FALSE(result.empty());
}

}  // namespace settings_filters
}  // namespace nscapi
