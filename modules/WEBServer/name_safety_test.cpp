#include "name_safety.hpp"

#include <gtest/gtest.h>

#include <string>

using name_safety::is_safe_module_name;
using name_safety::is_safe_script_name;

TEST(NameSafety, ModuleNameAcceptsBasic) {
  EXPECT_TRUE(is_safe_module_name("CheckSystem"));
  EXPECT_TRUE(is_safe_module_name("check_system"));
  EXPECT_TRUE(is_safe_module_name("check-system"));
  EXPECT_TRUE(is_safe_module_name("CheckSystem.dll"));
}

TEST(NameSafety, ModuleNameRejectsEmpty) { EXPECT_FALSE(is_safe_module_name("")); }

TEST(NameSafety, ModuleNameRejectsTraversal) {
  EXPECT_FALSE(is_safe_module_name(".."));
  EXPECT_FALSE(is_safe_module_name("."));
  EXPECT_FALSE(is_safe_module_name("../etc/passwd"));
  EXPECT_FALSE(is_safe_module_name("..\\Windows\\System32"));
}

TEST(NameSafety, ModuleNameRejectsSeparators) {
  EXPECT_FALSE(is_safe_module_name("a/b"));
  EXPECT_FALSE(is_safe_module_name("a\\b"));
  EXPECT_FALSE(is_safe_module_name("/abs"));
  EXPECT_FALSE(is_safe_module_name("\\abs"));
}

TEST(NameSafety, ModuleNameRejectsControlAndUnsafeChars) {
  EXPECT_FALSE(is_safe_module_name("a b"));
  EXPECT_FALSE(is_safe_module_name("a\tb"));
  EXPECT_FALSE(is_safe_module_name("a;b"));
  EXPECT_FALSE(is_safe_module_name("a$b"));
  EXPECT_FALSE(is_safe_module_name(std::string("a\0b", 3)));
}

TEST(NameSafety, ModuleNameRejectsTooLong) { EXPECT_FALSE(is_safe_module_name(std::string(200, 'a'))); }

TEST(NameSafety, ScriptNameAcceptsBasic) {
  EXPECT_TRUE(is_safe_script_name("check_disk.ps1"));
  EXPECT_TRUE(is_safe_script_name("check-disk"));
  EXPECT_TRUE(is_safe_script_name("subdir/check.ps1"));
  EXPECT_TRUE(is_safe_script_name("subdir\\check.ps1"));
  EXPECT_TRUE(is_safe_script_name("a/b/c/d"));
}

TEST(NameSafety, ScriptNameRejectsTraversal) {
  EXPECT_FALSE(is_safe_script_name(".."));
  EXPECT_FALSE(is_safe_script_name("../etc/passwd"));
  EXPECT_FALSE(is_safe_script_name("a/../b"));
  EXPECT_FALSE(is_safe_script_name("a\\..\\b"));
  EXPECT_FALSE(is_safe_script_name("./a"));
  EXPECT_FALSE(is_safe_script_name("a/./b"));
  EXPECT_FALSE(is_safe_script_name(".."));
}

TEST(NameSafety, ScriptNameRejectsLeadingSeparator) {
  EXPECT_FALSE(is_safe_script_name("/abs"));
  EXPECT_FALSE(is_safe_script_name("\\abs"));
}

TEST(NameSafety, ScriptNameRejectsDriveLetter) {
  EXPECT_FALSE(is_safe_script_name("C:foo"));
  EXPECT_FALSE(is_safe_script_name("c:/foo"));
}

TEST(NameSafety, ScriptNameRejectsEmptySegment) {
  EXPECT_FALSE(is_safe_script_name("a//b"));
  EXPECT_FALSE(is_safe_script_name("a/"));  // trailing separator
  EXPECT_FALSE(is_safe_script_name("a\\\\b"));
}

TEST(NameSafety, ScriptNameRejectsControlAndUnsafeChars) {
  EXPECT_FALSE(is_safe_script_name("a b"));
  EXPECT_FALSE(is_safe_script_name("a;b"));
  EXPECT_FALSE(is_safe_script_name("a$b"));
  EXPECT_FALSE(is_safe_script_name(std::string("a\0b", 3)));
  EXPECT_FALSE(is_safe_script_name("a\nb"));
}
