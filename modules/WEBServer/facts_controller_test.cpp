// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "facts_controller.hpp"

#include <gtest/gtest.h>

#include <string>

// The routing side needs a Mongoose server to exercise, but the path check
// does not, and it is the part that matters: `/api/v2/facts/<path>` puts a
// caller-supplied string into the query the core parses, so what a URL may
// spell is pinned here rather than left to the regex that got it this far.

TEST(FactsControllerPath, AcceptsAFactSetId) {
  EXPECT_TRUE(facts_controller::is_safe_fact_path("os"));
  EXPECT_TRUE(facts_controller::is_safe_fact_path("software.installed"));
  EXPECT_TRUE(facts_controller::is_safe_fact_path("network.interfaces"));
}

// A path may reach inside a set, not only name one: `os.family` is how a
// caller asks for one value without pulling the document.
TEST(FactsControllerPath, AcceptsAPathInsideASet) {
  EXPECT_TRUE(facts_controller::is_safe_fact_path("os.family"));
  EXPECT_TRUE(facts_controller::is_safe_fact_path("hardware.cpu.model"));
}

TEST(FactsControllerPath, AcceptsDigitsAndUnderscoresAfterTheFirstCharacter) {
  EXPECT_TRUE(facts_controller::is_safe_fact_path("total_bytes"));
  EXPECT_TRUE(facts_controller::is_safe_fact_path("core_0"));
}

TEST(FactsControllerPath, RejectsAnEmptyOrDotOnlyPath) {
  EXPECT_FALSE(facts_controller::is_safe_fact_path(""));
  EXPECT_FALSE(facts_controller::is_safe_fact_path("."));
  EXPECT_FALSE(facts_controller::is_safe_fact_path(".."));
}

TEST(FactsControllerPath, RejectsEmptyComponents) {
  EXPECT_FALSE(facts_controller::is_safe_fact_path(".os"));
  EXPECT_FALSE(facts_controller::is_safe_fact_path("os."));
  EXPECT_FALSE(facts_controller::is_safe_fact_path("os..family"));
}

TEST(FactsControllerPath, RejectsAComponentThatDoesNotStartWithALowerCaseLetter) {
  EXPECT_FALSE(facts_controller::is_safe_fact_path("Os"));
  EXPECT_FALSE(facts_controller::is_safe_fact_path("1st"));
  EXPECT_FALSE(facts_controller::is_safe_fact_path("_private"));
  EXPECT_FALSE(facts_controller::is_safe_fact_path("os._family"));
}

// Nothing that could change what the core parses, or reach outside the
// document, may survive the check.
TEST(FactsControllerPath, RejectsQuotesEscapesAndTraversal) {
  EXPECT_FALSE(facts_controller::is_safe_fact_path("os\""));
  EXPECT_FALSE(facts_controller::is_safe_fact_path("os\\"));
  EXPECT_FALSE(facts_controller::is_safe_fact_path("../../etc/passwd"));
  EXPECT_FALSE(facts_controller::is_safe_fact_path("os family"));
  EXPECT_FALSE(facts_controller::is_safe_fact_path("os\nfamily"));
  EXPECT_FALSE(facts_controller::is_safe_fact_path(std::string("os\0family", 9)));
}

TEST(FactsControllerPath, RejectsAnAbsurdlyLongPath) { EXPECT_FALSE(facts_controller::is_safe_fact_path(std::string(257, 'a'))); }
