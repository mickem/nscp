// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "plugin_interface.hpp"

#include <gtest/gtest.h>

// ============================================================================
// plugin_exception tests
// ============================================================================

TEST(PluginExceptionTest, ConstructorWithModuleAndError) {
  const nsclient::core::plugin_exception ex("TestModule", "Test error message");
  EXPECT_STREQ(ex.what(), "Test error message");
  EXPECT_EQ(ex.file(), "TestModule");
  EXPECT_EQ(ex.reason(), "Test error message");
}

TEST(PluginExceptionTest, InheritsFromStdException) {
  const nsclient::core::plugin_exception ex("Module", "Error");
  const std::exception* base_ptr = &ex;
  EXPECT_NE(base_ptr->what(), nullptr);
  EXPECT_STREQ(base_ptr->what(), "Error");
}

TEST(PluginExceptionTest, CopyConstructor) {
  const nsclient::core::plugin_exception ex1("Module1", "Error1");
  const nsclient::core::plugin_exception ex2(ex1);
  EXPECT_EQ(ex2.file(), "Module1");
  EXPECT_EQ(ex2.reason(), "Error1");
}

TEST(PluginExceptionTest, ThrowAndCatch) {
  try {
    throw nsclient::core::plugin_exception("TestPlugin", "Plugin failed to load");
  } catch (const nsclient::core::plugin_exception& ex) {
    EXPECT_EQ(ex.file(), "TestPlugin");
    EXPECT_EQ(ex.reason(), "Plugin failed to load");
  }
}

TEST(PluginExceptionTest, ThrowAndCatchAsStdException) {
  try {
    throw nsclient::core::plugin_exception("TestPlugin", "Plugin error");
  } catch (const std::exception& ex) {
    EXPECT_STREQ(ex.what(), "Plugin error");
  }
}

TEST(PluginExceptionTest, WhatReturnsValidPointerAfterCopy) {
  const nsclient::core::plugin_exception ex1("Module", "Original error");
  const nsclient::core::plugin_exception ex2 = ex1;
  const char* what1 = ex1.what();
  const char* what2 = ex2.what();
  EXPECT_STREQ(what1, "Original error");
  EXPECT_STREQ(what2, "Original error");
}
