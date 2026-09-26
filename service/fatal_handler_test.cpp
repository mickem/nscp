// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "fatal_handler.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

// The terminate handler itself ends in abort() and cannot be tested in
// process; what can be, and what decides whether the report it writes is any
// use, is how it names the exception that killed the agent.

namespace {
struct not_an_exception {};
}  // namespace

TEST(fatal_handler, describes_a_std_exception_with_its_message) {
  std::string described;
  try {
    throw std::runtime_error("failed to read the counter");
  } catch (...) {
    described = nsclient::describe_current_exception();
  }
  EXPECT_NE(described.find("failed to read the counter"), std::string::npos);
}

TEST(fatal_handler, describes_a_derived_exception_type) {
  // The type is what points at the culprit when what() is generic - the
  // reason an out_of_range and a bad_alloc do not read the same in the
  // report.
  std::string described;
  try {
    throw std::out_of_range("index");
  } catch (...) {
    described = nsclient::describe_current_exception();
  }
  EXPECT_NE(described.find("out_of_range"), std::string::npos);
  EXPECT_NE(described.find("index"), std::string::npos);
}

TEST(fatal_handler, describes_an_exception_that_is_not_a_std_exception) {
  std::string described;
  try {
    throw not_an_exception();
  } catch (...) {
    described = nsclient::describe_current_exception();
  }
  EXPECT_FALSE(described.empty());
  EXPECT_NE(described.find("not derived from std::exception"), std::string::npos);
}

TEST(fatal_handler, describes_a_thrown_string) {
  std::string described;
  try {
    throw std::string("a bare string");
  } catch (...) {
    described = nsclient::describe_current_exception();
  }
  EXPECT_NE(described.find("a bare string"), std::string::npos);
}

TEST(fatal_handler, says_so_when_there_is_no_active_exception) {
  // terminate() can also be called directly - a joinable thread destroyed, a
  // noexcept function that threw. The report has to say that rather than
  // claim an exception it cannot see.
  const std::string described = nsclient::describe_current_exception();
  EXPECT_NE(described.find("no active exception"), std::string::npos);
}

TEST(fatal_handler, installing_twice_is_harmless) {
  // load_configuration can run more than once (a settings reload), and the
  // handler must not end up chained to itself - that would recurse until the
  // stack ran out instead of writing the report.
  nsclient::install_fatal_handlers();
  nsclient::install_fatal_handlers();
  SUCCEED();
}
