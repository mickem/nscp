// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <atomic>
#include <boost/filesystem.hpp>
#include <check/access_policy.hpp>
#include <check/path_access_policy.hpp>
#include <check/prefix_access_policy.hpp>
#include <check/wql_query.hpp>
#include <fstream>
#include <thread>
#include <vector>

namespace fs = boost::filesystem;
using check::access::decision;
using check::access::mode;

namespace {

check::access::policy make_policy() { return check::access::policy("counter", "counters", "/settings/system/windows"); }

// ---------------------------------------------------------------------------
// mode parsing
// ---------------------------------------------------------------------------

TEST(access_mode, parses_the_documented_spellings) {
  mode m = mode::predefined;
  EXPECT_TRUE(check::access::parse_mode("any", m));
  EXPECT_EQ(mode::any, m);
  EXPECT_TRUE(check::access::parse_mode("allowed", m));
  EXPECT_EQ(mode::allowed, m);
  EXPECT_TRUE(check::access::parse_mode("predefined", m));
  EXPECT_EQ(mode::predefined, m);
}

TEST(access_mode, is_case_and_whitespace_insensitive) {
  mode m = mode::any;
  EXPECT_TRUE(check::access::parse_mode("  PreDefined \t", m));
  EXPECT_EQ(mode::predefined, m);
}

TEST(access_mode, an_unset_value_is_any) {
  mode m = mode::predefined;
  EXPECT_TRUE(check::access::parse_mode("", m));
  EXPECT_EQ(mode::any, m);
}

TEST(access_mode, rejects_an_unknown_value) {
  mode m = mode::any;
  EXPECT_FALSE(check::access::parse_mode("predefinedd", m));
  EXPECT_FALSE(check::access::parse_mode("none", m));
}

// A typo in a security setting must not read as "no restriction": the policy
// keeps the error and refuses everything until it is fixed.
TEST(access_policy, an_invalid_mode_fails_closed) {
  check::access::policy p = make_policy();
  p.set_mode("allwed");
  EXPECT_TRUE(p.is_restricted());
  EXPECT_FALSE(p.get_config_error().empty());
  const decision d = p.resolve("\\Memory\\Pages/sec");
  EXPECT_FALSE(d.allowed);
  EXPECT_NE(std::string::npos, d.error.find("expected any, allowed or predefined"));
}

// ---------------------------------------------------------------------------
// mode: any
// ---------------------------------------------------------------------------

TEST(access_policy, any_is_the_default_and_accepts_everything) {
  const check::access::policy p = make_policy();
  EXPECT_EQ(mode::any, p.get_mode());
  EXPECT_FALSE(p.is_restricted());
  const decision d = p.resolve("\\Processor(_Total)\\% Processor Time");
  EXPECT_TRUE(d.allowed);
  EXPECT_EQ("\\Processor(_Total)\\% Processor Time", d.value);
}

// ---------------------------------------------------------------------------
// mode: allowed
// ---------------------------------------------------------------------------

TEST(access_policy, allowed_accepts_a_value_matching_the_list) {
  check::access::policy p = make_policy();
  p.set_mode("allowed");
  p.set_allow_list("\\Processor(*)\\*, \\Memory\\*");
  EXPECT_TRUE(p.resolve("\\Processor(_Total)\\% Processor Time").allowed);
  EXPECT_TRUE(p.resolve("\\Memory\\Pages/sec").allowed);
}

TEST(access_policy, allowed_refuses_a_value_outside_the_list) {
  check::access::policy p = make_policy();
  p.set_mode("allowed");
  p.set_allow_list("\\Memory\\*");
  const decision d = p.resolve("\\Process(lsass)\\ID Process");
  EXPECT_FALSE(d.allowed);
  EXPECT_NE(std::string::npos, d.error.find("\\Process(lsass)\\ID Process"));
  EXPECT_NE(std::string::npos, d.error.find("allowed counters"));
}

// The refusal tells the operator what was rejected and which setting governs
// it, but never what the list holds - the caller does not get to enumerate it.
TEST(access_policy, a_refusal_does_not_disclose_the_list) {
  check::access::policy p = make_policy();
  p.set_mode("allowed");
  p.set_allow_list("\\SecretObject\\SecretCounter");
  const decision d = p.resolve("\\Memory\\Pages/sec");
  ASSERT_FALSE(d.allowed);
  EXPECT_EQ(std::string::npos, d.error.find("SecretObject"));
  EXPECT_EQ(std::string::npos, d.error.find("SecretCounter"));
}

TEST(access_policy, an_empty_allow_list_in_allowed_mode_refuses_everything) {
  check::access::policy p = make_policy();
  p.set_mode("allowed");
  p.set_allow_list("");
  EXPECT_EQ(0u, p.allow_list_size());
  EXPECT_FALSE(p.resolve("\\Memory\\Pages/sec").allowed);
}

// A glob entry is matched literally apart from `*` and `?`; the regex
// metacharacters a counter name is full of must not change what it matches.
TEST(access_policy, glob_metacharacters_are_literal) {
  check::access::policy p = make_policy();
  p.set_mode("allowed");
  p.set_allow_list("\\Processor(_Total)\\*");
  EXPECT_TRUE(p.resolve("\\Processor(_Total)\\% Processor Time").allowed);
  // Without escaping, "(_Total)" would be a capture group and this would match.
  EXPECT_FALSE(p.resolve("\\Processor_Total\\% Processor Time").allowed);
}

TEST(access_policy, a_glob_is_anchored_at_both_ends) {
  check::access::policy p = make_policy();
  p.set_mode("allowed");
  p.set_allow_list("Win32_Service");
  EXPECT_TRUE(p.resolve("Win32_Service").allowed);
  EXPECT_FALSE(p.resolve("Win32_ServiceExtra").allowed);
  EXPECT_FALSE(p.resolve("XWin32_Service").allowed);
}

TEST(access_policy, question_mark_matches_one_character) {
  check::access::policy p = make_policy();
  p.set_mode("allowed");
  p.set_allow_list("Win32_Disk?");
  EXPECT_TRUE(p.resolve("Win32_Disk1").allowed);
  EXPECT_FALSE(p.resolve("Win32_Disk12").allowed);
}

TEST(access_policy, matching_is_case_insensitive_by_default) {
  check::access::policy p = make_policy();
  p.set_mode("allowed");
  p.set_allow_list("win32_service");
  EXPECT_TRUE(p.resolve("Win32_Service").allowed);
}

TEST(access_policy, a_case_sensitive_policy_does_not_fold_case) {
  check::access::policy p("file", "files", "/settings/logfile", true);
  p.set_mode("allowed");
  p.set_allow_list("/var/log/app.log");
  EXPECT_TRUE(p.resolve("/var/log/app.log").allowed);
  EXPECT_FALSE(p.resolve("/var/log/App.log").allowed);
}

// ---------------------------------------------------------------------------
// mode: predefined
// ---------------------------------------------------------------------------

TEST(access_policy, predefined_expands_a_configured_name) {
  check::access::policy p = make_policy();
  p.set_mode("predefined");
  p.add_predefined("cpu", "\\Processor(_Total)\\% Processor Time");
  const decision d = p.resolve("cpu");
  EXPECT_TRUE(d.allowed);
  EXPECT_EQ("\\Processor(_Total)\\% Processor Time", d.value);
}

TEST(access_policy, predefined_refuses_a_raw_value) {
  check::access::policy p = make_policy();
  p.set_mode("predefined");
  p.add_predefined("cpu", "\\Processor(_Total)\\% Processor Time");
  const decision d = p.resolve("\\Memory\\Pages/sec");
  EXPECT_FALSE(d.allowed);
  EXPECT_NE(std::string::npos, d.error.find("set to predefined"));
}

// Operator-authored names resolve in every mode. That is what lets a site name
// its checks first and tighten the mode afterwards without touching the
// monitoring server's commands.
TEST(access_policy, a_predefined_name_resolves_in_any_mode) {
  check::access::policy p = make_policy();
  p.add_predefined("cpu", "\\Processor(_Total)\\% Processor Time");
  EXPECT_EQ("\\Processor(_Total)\\% Processor Time", p.resolve("cpu").value);

  p.set_mode("allowed");
  p.set_allow_list("\\Memory\\*");
  EXPECT_EQ("\\Processor(_Total)\\% Processor Time", p.resolve("cpu").value);
}

// A predefined value is operator-authored, so it is trusted even when it would
// not have passed the allow list itself.
TEST(access_policy, a_predefined_value_is_not_held_against_the_allow_list) {
  check::access::policy p = make_policy();
  p.set_mode("allowed");
  p.set_allow_list("\\Memory\\*");
  p.add_predefined("cpu", "\\Processor(_Total)\\% Processor Time");
  EXPECT_TRUE(p.resolve("cpu").allowed);
}

// The predefined entries are appended by the settings callbacks, so a reload
// which does not clear them first would double every entry under the threads
// reading them...
TEST(access_policy, reset_clears_the_predefined_entries_for_a_reload) {
  check::access::policy p = make_policy();
  p.add_predefined("cpu", "x");
  p.reset();
  EXPECT_FALSE(p.has_predefined("cpu"));
}

// ...but the mode and the allow list are replaced by theirs, and must stay in
// force: a reload calls loadModuleEx on the live module, and a check arriving
// between reset() and settings.notify() must not find the gate open.
TEST(access_policy, reset_keeps_the_gate_closed_until_the_settings_are_reread) {
  check::access::policy p = make_policy();
  p.set_mode("allowed");
  p.set_allow_list("\\Memory\\*");
  p.reset();
  EXPECT_EQ(mode::allowed, p.get_mode());
  EXPECT_TRUE(p.is_restricted());
  EXPECT_EQ(1u, p.allow_list_size());
  EXPECT_TRUE(p.resolve("\\Memory\\Available Bytes").allowed);
  EXPECT_FALSE(p.resolve("\\Processor(_Total)\\% Processor Time").allowed);

  p.set_mode("predefined");
  p.reset();
  EXPECT_FALSE(p.resolve("\\Memory\\Available Bytes").allowed);
}

TEST(access_policy, an_invalid_mode_survives_a_reset) {
  check::access::policy p = make_policy();
  p.set_mode("alowed");
  p.reset();
  EXPECT_TRUE(p.is_restricted());
  EXPECT_FALSE(p.resolve("\\Memory\\Available Bytes").allowed);
}

TEST(access_policy, a_copy_carries_the_configuration) {
  check::access::policy p = make_policy();
  p.set_mode("allowed");
  p.set_allow_list("\\Memory\\*");
  p.add_predefined("cpu", "x");
  const check::access::policy copy(p);
  EXPECT_EQ(mode::allowed, copy.get_mode());
  EXPECT_TRUE(copy.resolve("\\Memory\\Available Bytes").allowed);
  EXPECT_TRUE(copy.has_predefined("cpu"));
  check::access::policy assigned = make_policy();
  assigned = p;
  EXPECT_TRUE(assigned.resolve("\\Memory\\Available Bytes").allowed);
  EXPECT_TRUE(assigned.has_predefined("cpu"));
}

TEST(access_policy, check_value_reports_the_dimension_it_rejected) {
  check::access::policy p("namespace", "namespaces", "/settings/wmi");
  p.set_allow_list("root\\cimv2");
  EXPECT_TRUE(p.check_value("root\\cimv2", "namespace").allowed);
  const decision d = p.check_value("root\\securitycenter2", "namespace");
  EXPECT_FALSE(d.allowed);
  EXPECT_NE(std::string::npos, d.error.find("allowed namespaces"));
}

// ---------------------------------------------------------------------------
// WQL class extraction
// ---------------------------------------------------------------------------

TEST(wql, extracts_the_class_from_a_plain_select) {
  const check::wql::parse_result r = check::wql::extract_class("SELECT * FROM Win32_OperatingSystem");
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_EQ("Win32_OperatingSystem", r.class_name);
}

TEST(wql, handles_a_column_list_and_a_where_clause) {
  const check::wql::parse_result r = check::wql::extract_class("select Name, State from Win32_Service where State = 'Running'");
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_EQ("Win32_Service", r.class_name);
}

TEST(wql, tolerates_newlines_and_padding) {
  const check::wql::parse_result r = check::wql::extract_class("\n  SELECT *\n  FROM Win32_Process\n  WHERE Name = 'x'\n");
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_EQ("Win32_Process", r.class_name);
}

// "FROM" inside a string literal in the WHERE clause must not be mistaken for
// the real FROM: the non-greedy column list stops at the first one.
TEST(wql, a_where_clause_mentioning_from_does_not_move_the_class) {
  const check::wql::parse_result r = check::wql::extract_class("SELECT * FROM Win32_Process WHERE Name = 'FROM Win32_Service'");
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_EQ("Win32_Process", r.class_name);
}

TEST(wql, refuses_a_second_statement) {
  const check::wql::parse_result r = check::wql::extract_class("SELECT * FROM Win32_Service; SELECT * FROM Win32_Process");
  EXPECT_FALSE(r.ok);
  EXPECT_NE(std::string::npos, r.error.find("single statement"));
}

// A class path carrying a namespace or a machine would be approved on its
// trailing identifier and read somewhere else entirely.
TEST(wql, refuses_a_qualified_class_path) {
  EXPECT_FALSE(check::wql::extract_class("SELECT * FROM root\\cimv2:Win32_Process").ok);
  EXPECT_FALSE(check::wql::extract_class("SELECT * FROM \\\\host\\root\\cimv2:Win32_Process").ok);
  EXPECT_FALSE(check::wql::extract_class("SELECT * FROM root/cimv2:Win32_Process").ok);
}

TEST(wql, refuses_the_association_forms) {
  EXPECT_FALSE(check::wql::extract_class("ASSOCIATORS OF {Win32_LogicalDisk.DeviceID='C:'}").ok);
  EXPECT_FALSE(check::wql::extract_class("REFERENCES OF {Win32_Service.Name='x'}").ok);
}

TEST(wql, refuses_a_column_list_which_is_not_plain) {
  EXPECT_FALSE(check::wql::extract_class("SELECT (SELECT * FROM Win32_Process) FROM Win32_Service").ok);
  EXPECT_FALSE(check::wql::extract_class("SELECT a.b FROM Win32_Service").ok);
}

TEST(wql, refuses_an_embedded_nul) {
  std::string q("SELECT * FROM Win32_Service");
  q.push_back('\0');
  q += " extra";
  EXPECT_FALSE(check::wql::extract_class(q).ok);
}

TEST(wql, refuses_nonsense) {
  EXPECT_FALSE(check::wql::extract_class("").ok);
  EXPECT_FALSE(check::wql::extract_class("DELETE FROM Win32_Service").ok);
  EXPECT_FALSE(check::wql::extract_class("SELECT * FROM").ok);
}

// ---------------------------------------------------------------------------
// path policy
// ---------------------------------------------------------------------------

class path_access_test : public ::testing::Test {
 protected:
  void SetUp() override {
    root_ = fs::temp_directory_path() / fs::unique_path("nscp-access-%%%%%%%%");
    fs::create_directories(root_ / "logs" / "sub");
    fs::create_directories(root_ / "secret");
    write(root_ / "logs" / "app.log", "in\n");
    write(root_ / "logs" / "sub" / "deep.log", "deep\n");
    write(root_ / "logs" / "notes.txt", "txt\n");
    write(root_ / "secret" / "shadow", "secret\n");
  }
  void TearDown() override {
    boost::system::error_code ec;
    fs::remove_all(root_, ec);
  }
  static void write(const fs::path &p, const std::string &body) {
    std::ofstream f(p.string().c_str());
    f << body;
  }
  std::string at(const std::string &rel) const { return (root_ / rel).string(); }

  fs::path root_;
};

TEST_F(path_access_test, any_accepts_any_path_unchanged) {
  const check::access::path_policy p("file", "files", "/settings/logfile");
  const decision d = p.resolve(at("secret/shadow"));
  EXPECT_TRUE(d.allowed);
  EXPECT_EQ(at("secret/shadow"), d.value);
}

TEST_F(path_access_test, a_directory_entry_covers_the_whole_subtree) {
  check::access::path_policy p("file", "files", "/settings/logfile");
  p.set_mode("allowed");
  p.set_allow_list(at("logs"));
  EXPECT_TRUE(p.resolve(at("logs/app.log")).allowed);
  EXPECT_TRUE(p.resolve(at("logs/sub/deep.log")).allowed);
  EXPECT_FALSE(p.resolve(at("secret/shadow")).allowed);
}

// `*` stops at a directory separator: an operator who wrote a one-level
// pattern gets one level, not the subtree.
TEST_F(path_access_test, a_glob_entry_matches_only_its_own_level) {
  check::access::path_policy p("file", "files", "/settings/logfile");
  p.set_mode("allowed");
  p.set_allow_list(at("logs") + "/*.log");
  EXPECT_TRUE(p.resolve(at("logs/app.log")).allowed);
  EXPECT_FALSE(p.resolve(at("logs/notes.txt")).allowed);
  EXPECT_FALSE(p.resolve(at("logs/sub/deep.log")).allowed);
  EXPECT_FALSE(p.resolve(at("secret/shadow")).allowed);
}

TEST_F(path_access_test, a_question_mark_does_not_cross_a_separator) {
  check::access::path_policy p("file", "files", "/settings/logfile");
  p.set_mode("allowed");
  p.set_allow_list(at("logs") + "/?u?/deep.log");
  EXPECT_TRUE(p.resolve(at("logs/sub/deep.log")).allowed);
  fs::create_directories(root_ / "logs" / "a" / "b");
  write(root_ / "logs" / "a" / "b" / "deep.log", "x\n");
  EXPECT_FALSE(p.resolve(at("logs/a/b/deep.log")).allowed);
}

// `**` is the spelling which crosses separators, for when the subtree is meant.
TEST_F(path_access_test, a_double_star_glob_covers_the_subtree) {
  check::access::path_policy p("file", "files", "/settings/logfile");
  p.set_mode("allowed");
  p.set_allow_list(at("logs") + "/**.log");
  EXPECT_TRUE(p.resolve(at("logs/app.log")).allowed);
  EXPECT_TRUE(p.resolve(at("logs/sub/deep.log")).allowed);
  EXPECT_FALSE(p.resolve(at("logs/notes.txt")).allowed);
  EXPECT_FALSE(p.resolve(at("secret/shadow")).allowed);
}

// A directory which is not there when the settings are read (a volume mounted
// later, a log directory the application creates on first run) must still
// cover what appears beneath it, without a reload.
TEST_F(path_access_test, a_directory_entry_which_does_not_exist_yet_covers_what_appears_beneath_it) {
  check::access::path_policy p("file", "files", "/settings/logfile");
  p.set_mode("allowed");
  p.set_allow_list(at("later"));
  fs::create_directories(root_ / "later" / "sub");
  write(root_ / "later" / "app.log", "x\n");
  write(root_ / "later" / "sub" / "deep.log", "x\n");
  EXPECT_TRUE(p.resolve(at("later/app.log")).allowed);
  EXPECT_TRUE(p.resolve(at("later/sub/deep.log")).allowed);
  EXPECT_FALSE(p.resolve(at("secret/shadow")).allowed);
}

TEST_F(path_access_test, a_file_entry_which_does_not_exist_yet_covers_that_file) {
  check::access::path_policy p("file", "files", "/settings/logfile");
  p.set_mode("allowed");
  p.set_allow_list(at("logs/later.log"));
  write(root_ / "logs" / "later.log", "x\n");
  EXPECT_TRUE(p.resolve(at("logs/later.log")).allowed);
  EXPECT_FALSE(p.resolve(at("logs/app.log")).allowed);
}

#ifdef WIN32
// Directory containment has to fold case like the glob entries do, or the
// same tree is allowed under one drive-letter spelling and refused under
// another.
TEST_F(path_access_test, a_directory_entry_ignores_case_on_windows) {
  check::access::path_policy p("file", "files", "/settings/logfile");
  p.set_mode("allowed");
  p.set_allow_list(boost::algorithm::to_upper_copy(at("logs")));
  EXPECT_TRUE(p.resolve(boost::algorithm::to_lower_copy(at("logs/app.log"))).allowed);
  EXPECT_TRUE(p.resolve(at("logs/sub/deep.log")).allowed);
}
#endif

TEST_F(path_access_test, reset_keeps_the_allow_list_until_the_settings_are_reread) {
  check::access::path_policy p("file", "files", "/settings/logfile");
  p.set_mode("allowed");
  p.set_allow_list(at("logs"));
  p.add_predefined("app", at("logs/app.log"));
  p.reset();
  EXPECT_EQ(mode::allowed, p.get_mode());
  EXPECT_TRUE(p.resolve(at("logs/app.log")).allowed);
  EXPECT_FALSE(p.resolve(at("secret/shadow")).allowed);
  EXPECT_FALSE(p.resolve("app").allowed);
}

// The whole reason a path needs its own policy: `..` has to be flattened
// before the comparison, or the allow list is decoration.
TEST_F(path_access_test, dot_dot_traversal_out_of_an_allowed_directory_is_refused) {
  check::access::path_policy p("file", "files", "/settings/logfile");
  p.set_mode("allowed");
  p.set_allow_list(at("logs"));
  EXPECT_FALSE(p.resolve(at("logs/../secret/shadow")).allowed);
  EXPECT_FALSE(p.resolve(at("logs/sub/../../secret/shadow")).allowed);
}

TEST_F(path_access_test, a_sibling_directory_with_a_shared_prefix_is_not_inside) {
  fs::create_directories(root_ / "logs-private");
  write(root_ / "logs-private" / "x.log", "x\n");
  check::access::path_policy p("file", "files", "/settings/logfile");
  p.set_mode("allowed");
  p.set_allow_list(at("logs"));
  EXPECT_FALSE(p.resolve(at("logs-private/x.log")).allowed);
}

#ifndef WIN32
// A symlink planted inside an allowed directory is the other way the string
// comparison lies; weakly_canonical resolves it before the test.
TEST_F(path_access_test, a_symlink_leading_out_of_an_allowed_directory_is_refused) {
  boost::system::error_code ec;
  fs::create_symlink(root_ / "secret" / "shadow", root_ / "logs" / "escape.log", ec);
  if (ec) GTEST_SKIP() << "cannot create symlinks here: " << ec.message();

  check::access::path_policy p("file", "files", "/settings/logfile");
  p.set_mode("allowed");
  p.set_allow_list(at("logs"));
  EXPECT_FALSE(p.resolve(at("logs/escape.log")).allowed);
}
#endif

// What is matched is what gets opened: resolve() hands back the path it
// approved, so nothing re-resolves the name between the check and the read.
TEST_F(path_access_test, an_accepted_path_comes_back_resolved) {
  check::access::path_policy p("file", "files", "/settings/logfile");
  p.set_mode("allowed");
  p.set_allow_list(at("logs"));
  const decision d = p.resolve(at("logs/sub/../app.log"));
  ASSERT_TRUE(d.allowed) << d.error;
  EXPECT_EQ(check::access::path_policy::canonical(at("logs/app.log")), d.value);
}

TEST_F(path_access_test, predefined_files_resolve_by_name) {
  check::access::path_policy p("file", "files", "/settings/logfile");
  p.set_mode("predefined");
  p.add_predefined("app", at("logs/app.log"));
  const decision d = p.resolve("app");
  EXPECT_TRUE(d.allowed);
  EXPECT_EQ(at("logs/app.log"), d.value);
  EXPECT_FALSE(p.resolve(at("secret/shadow")).allowed);
}

TEST_F(path_access_test, an_exact_file_entry_allows_only_that_file) {
  check::access::path_policy p("file", "files", "/settings/logfile");
  p.set_mode("allowed");
  p.set_allow_list(at("logs/app.log"));
  EXPECT_TRUE(p.resolve(at("logs/app.log")).allowed);
  EXPECT_FALSE(p.resolve(at("logs/notes.txt")).allowed);
}

TEST_F(path_access_test, a_trailing_separator_on_a_directory_entry_is_harmless) {
  check::access::path_policy p("file", "files", "/settings/logfile");
  p.set_mode("allowed");
  p.set_allow_list(at("logs") + "/");
  EXPECT_TRUE(p.resolve(at("logs/app.log")).allowed);
}

TEST_F(path_access_test, several_entries_are_all_considered) {
  check::access::path_policy p("file", "files", "/settings/logfile");
  p.set_mode("allowed");
  p.set_allow_list(at("logs/app.log") + " , " + at("logs/sub"));
  EXPECT_TRUE(p.resolve(at("logs/app.log")).allowed);
  EXPECT_TRUE(p.resolve(at("logs/sub/deep.log")).allowed);
  EXPECT_FALSE(p.resolve(at("logs/notes.txt")).allowed);
}

// ---------------------------------------------------------------------------
// prefix policy (registry keys, event log channels)
// ---------------------------------------------------------------------------

namespace {
// The registry accepts both spellings of every hive, so the gate compares them
// in one spelling or an operator's list silently misses half the requests.
std::string normalize_hive(const std::string &key) {
  static const char *pairs[][2] = {
      {"HKEY_LOCAL_MACHINE", "HKLM"}, {"HKEY_CURRENT_USER", "HKCU"}, {"HKEY_CLASSES_ROOT", "HKCR"}, {"HKEY_USERS", "HKU"}, {"HKEY_CURRENT_CONFIG", "HKCC"}};
  for (const auto &pair : pairs) {
    const std::string full(pair[0]);
    if (key.size() >= full.size() && boost::algorithm::iequals(key.substr(0, full.size()), full)) {
      return std::string(pair[1]) + key.substr(full.size());
    }
  }
  return key;
}

check::access::prefix_policy registry_policy() {
  return check::access::prefix_policy("registry key", "registry keys", "/settings/system/windows", '\\', &normalize_hive);
}
check::access::prefix_policy channel_policy() { return check::access::prefix_policy("log", "logs", "/settings/eventlog", '/'); }
}  // namespace

TEST(prefix_policy, any_is_the_default) {
  const check::access::prefix_policy p = registry_policy();
  EXPECT_FALSE(p.is_restricted());
  EXPECT_TRUE(p.resolve("HKLM\\SAM").allowed);
}

TEST(prefix_policy, a_literal_entry_covers_the_key_itself) {
  check::access::prefix_policy p = registry_policy();
  p.set_mode("allowed");
  p.set_allow_list("HKLM\\SOFTWARE\\MyApp");
  EXPECT_TRUE(p.resolve("HKLM\\SOFTWARE\\MyApp").allowed);
}

TEST(prefix_policy, a_literal_entry_covers_the_subtree) {
  check::access::prefix_policy p = registry_policy();
  p.set_mode("allowed");
  p.set_allow_list("HKLM\\SOFTWARE\\MyApp");
  EXPECT_TRUE(p.resolve("HKLM\\SOFTWARE\\MyApp\\Settings").allowed);
  EXPECT_TRUE(p.resolve("HKLM\\SOFTWARE\\MyApp\\Deep\\Nested\\Key").allowed);
}

// The whole reason this is not a plain glob: a prefix only counts when the next
// character ends the segment, or `MyApp` would cover `MyAppEvil`.
TEST(prefix_policy, a_prefix_does_not_match_mid_segment) {
  check::access::prefix_policy p = registry_policy();
  p.set_mode("allowed");
  p.set_allow_list("HKLM\\SOFTWARE\\MyApp");
  EXPECT_FALSE(p.resolve("HKLM\\SOFTWARE\\MyAppEvil").allowed);
  EXPECT_FALSE(p.resolve("HKLM\\SOFTWARE\\MyAppEvil\\Sub").allowed);
}

TEST(prefix_policy, refuses_a_sibling_and_another_hive) {
  check::access::prefix_policy p = registry_policy();
  p.set_mode("allowed");
  p.set_allow_list("HKLM\\SOFTWARE\\MyApp");
  EXPECT_FALSE(p.resolve("HKLM\\SOFTWARE\\Other").allowed);
  EXPECT_FALSE(p.resolve("HKLM\\SAM").allowed);
  EXPECT_FALSE(p.resolve("HKCU\\SOFTWARE\\MyApp").allowed);
}

TEST(prefix_policy, the_long_hive_spelling_resolves_to_the_same_entry) {
  check::access::prefix_policy p = registry_policy();
  p.set_mode("allowed");
  p.set_allow_list("HKLM\\SOFTWARE\\MyApp");
  EXPECT_TRUE(p.resolve("HKEY_LOCAL_MACHINE\\SOFTWARE\\MyApp\\Sub").allowed);
  EXPECT_FALSE(p.resolve("HKEY_LOCAL_MACHINE\\SAM").allowed);
}

TEST(prefix_policy, an_entry_written_with_the_long_spelling_works_too) {
  check::access::prefix_policy p = registry_policy();
  p.set_mode("allowed");
  p.set_allow_list("HKEY_LOCAL_MACHINE\\SOFTWARE\\MyApp");
  EXPECT_TRUE(p.resolve("HKLM\\SOFTWARE\\MyApp\\Sub").allowed);
}

TEST(prefix_policy, registry_matching_ignores_case) {
  check::access::prefix_policy p = registry_policy();
  p.set_mode("allowed");
  p.set_allow_list("HKLM\\Software\\MyApp");
  EXPECT_TRUE(p.resolve("hklm\\SOFTWARE\\myapp\\Sub").allowed);
}

TEST(prefix_policy, a_trailing_separator_on_an_entry_is_harmless) {
  check::access::prefix_policy p = registry_policy();
  p.set_mode("allowed");
  p.set_allow_list("HKLM\\SOFTWARE\\MyApp\\");
  EXPECT_TRUE(p.resolve("HKLM\\SOFTWARE\\MyApp\\Sub").allowed);
  EXPECT_FALSE(p.resolve("HKLM\\SOFTWARE\\MyAppEvil").allowed);
}

TEST(prefix_policy, a_wildcard_entry_is_matched_as_a_glob) {
  check::access::prefix_policy p = registry_policy();
  p.set_mode("allowed");
  p.set_allow_list("HKLM\\SOFTWARE\\*\\Version");
  EXPECT_TRUE(p.resolve("HKLM\\SOFTWARE\\MyApp\\Version").allowed);
  EXPECT_FALSE(p.resolve("HKLM\\SOFTWARE\\MyApp\\Secrets").allowed);
}

TEST(prefix_policy, predefined_names_resolve_in_every_mode) {
  check::access::prefix_policy p = registry_policy();
  p.add_predefined("myapp", "HKLM\\SOFTWARE\\MyApp");
  EXPECT_EQ("HKLM\\SOFTWARE\\MyApp", p.resolve("myapp").value);

  p.set_mode("predefined");
  EXPECT_EQ("HKLM\\SOFTWARE\\MyApp", p.resolve("myapp").value);
  const check::access::decision d = p.resolve("HKLM\\SAM");
  EXPECT_FALSE(d.allowed);
  EXPECT_NE(std::string::npos, d.error.find("set to predefined"));
}

TEST(prefix_policy, an_invalid_mode_fails_closed) {
  check::access::prefix_policy p = registry_policy();
  p.set_mode("allwed");
  const check::access::decision d = p.resolve("HKLM\\SOFTWARE\\MyApp");
  EXPECT_FALSE(d.allowed);
  EXPECT_NE(std::string::npos, d.error.find("expected any, allowed or predefined"));
}

TEST(prefix_policy, a_refusal_does_not_disclose_the_list) {
  check::access::prefix_policy p = registry_policy();
  p.set_mode("allowed");
  p.set_allow_list("HKLM\\SOFTWARE\\SecretVendor");
  const check::access::decision d = p.resolve("HKLM\\SAM");
  ASSERT_FALSE(d.allowed);
  EXPECT_EQ(std::string::npos, d.error.find("SecretVendor"));
}

TEST(prefix_policy, reset_drops_the_predefined_entries_and_keeps_the_gate) {
  check::access::prefix_policy p = registry_policy();
  p.set_mode("allowed");
  p.set_allow_list("HKLM\\a,HKLM\\b");
  p.add_predefined("x", "HKLM\\x");
  p.reset();
  EXPECT_EQ(check::access::mode::allowed, p.get_mode());
  EXPECT_EQ(2u, p.allow_list_size());
  EXPECT_FALSE(p.resolve("x").allowed);
  EXPECT_TRUE(p.resolve("HKLM\\a\\sub").allowed);
  EXPECT_FALSE(p.resolve("HKLM\\SAM").allowed);
}

// The event log uses the same machinery with '/' as the separator, so a channel
// family can be allowed without naming each channel.
TEST(prefix_policy, an_event_log_channel_family_is_covered_by_its_prefix) {
  check::access::prefix_policy p = channel_policy();
  p.set_mode("allowed");
  p.set_allow_list("Application, Microsoft-Windows-Sysmon");
  EXPECT_TRUE(p.resolve("Application").allowed);
  EXPECT_TRUE(p.resolve("Microsoft-Windows-Sysmon/Operational").allowed);
  EXPECT_FALSE(p.resolve("Security").allowed);
  // The separator rule again: a sibling family sharing the prefix is not in.
  EXPECT_FALSE(p.resolve("Microsoft-Windows-SysmonEvil/Operational").allowed);
}

TEST(prefix_policy, an_event_log_refusal_names_the_setting) {
  check::access::prefix_policy p = channel_policy();
  p.set_mode("allowed");
  p.set_allow_list("Application");
  const check::access::decision d = p.resolve("Security");
  EXPECT_FALSE(d.allowed);
  EXPECT_NE(std::string::npos, d.error.find("Refusing log 'Security'"));
  EXPECT_NE(std::string::npos, d.error.find("allowed logs"));
  EXPECT_NE(std::string::npos, d.error.find("/settings/eventlog"));
}

// ---------------------------------------------------------------------------
// Negative tests: the ways a path gate is fooled
//
// Every bug found in review of this gate had the same shape - resolve() said
// yes and handed back a path which, read by the OS, was somewhere else. So
// these tests do not assert "refused"; they assert the invariant that makes a
// refusal unnecessary:
//
//   whatever the caller spelled, the value handed back must resolve - through
//   boost, independently of anything in path_access_policy - to a path inside
//   the allowed directory.
//
// A gate which accepts a hostile token but returns a path that stays inside is
// just as correct as one which refuses it, and this says so without pinning
// the test to today's behaviour.
// ---------------------------------------------------------------------------

class path_escape_test : public ::testing::Test {
 protected:
  void SetUp() override {
    root_ = fs::temp_directory_path() / fs::unique_path("nscp-escape-%%%%%%%%");
    fs::create_directories(root_ / "logs" / "sub");
    fs::create_directories(root_ / "secret");
    write(root_ / "logs" / "app.log", "in\n");
    write(root_ / "logs" / "sub" / "deep.log", "deep\n");
    write(root_ / "secret" / "shadow", "SECRET\n");
  }
  void TearDown() override {
    boost::system::error_code ec;
    fs::remove_all(root_, ec);
  }
  static void write(const fs::path &p, const std::string &body) {
    std::ofstream f(p.string().c_str());
    f << body;
  }
  std::string at(const std::string &rel) const { return (root_ / rel).string(); }
  std::string allowed_dir() const { return at("logs"); }

  check::access::path_policy restricted() const {
    check::access::path_policy p("file", "files", "/settings/logfile");
    p.set_mode("allowed");
    p.set_allow_list(allowed_dir());
    return p;
  }

  // Resolve `token` against a policy allowing only <root>/logs, and check the
  // invariant. Returns whether it was accepted, so a caller can additionally
  // pin today's answer where that is worth pinning.
  bool accepted_and_contained(const check::access::path_policy &p, const std::string &token) const {
    const decision d = p.resolve(token);
    if (!d.allowed) return false;
    // Resolve the *returned* value the way the operating system will when the
    // check opens it. Deliberately not path_policy::canonical: a bug in that
    // function is exactly what this is meant to catch.
    boost::system::error_code ec;
    fs::path landed = fs::weakly_canonical(fs::path(d.value), ec);
    if (ec) landed = fs::path(d.value).lexically_normal();
    fs::path base = fs::weakly_canonical(fs::path(allowed_dir()), ec);
    if (ec) base = fs::path(allowed_dir()).lexically_normal();

    const std::string landed_s = landed.string();
    const std::string base_s = base.string();
    const bool inside = landed_s == base_s || (landed_s.size() > base_s.size() && landed_s.compare(0, base_s.size(), base_s) == 0 &&
                                               (landed_s[base_s.size()] == '/' || landed_s[base_s.size()] == '\\'));
    EXPECT_TRUE(inside) << "accepted '" << token << "' but it lands on '" << landed_s << "', outside '" << base_s << "'";
    return true;
  }

  fs::path root_;
};

// The table this suite exists for. Each token is a way of writing "leave the
// allowed directory"; none of them may come back as a path which does.
TEST_F(path_escape_test, no_spelling_of_a_traversal_escapes_the_allowed_directory) {
  const check::access::path_policy p = restricted();
  const std::string logs = allowed_dir();
  const std::vector<std::string> tokens = {
      logs + "/../secret/shadow",
      logs + "/../../etc/passwd",
      // Written with a backslash. On Windows that is a separator and must be
      // flattened; on Linux it is an ordinary character and must not become
      // one after the match - which is what let this through (#1516 review).
      logs + "/..\\..\\secret/shadow",
      logs + "/..\\secret\\shadow",
      logs + "\\..\\secret\\shadow",
      logs + "/./../secret/shadow",
      logs + "/.././secret/shadow",
      logs + "//../secret/shadow",
      logs + "/sub/../../secret/shadow",
      logs + "/./sub/./../../secret/shadow",
      logs + "/../logs/../secret/shadow",
      logs + "/sub/../sub/../../secret/shadow",
      logs + "/../../../../../../../../etc/passwd",
      at("secret/shadow"),
      "/etc/passwd",
  };
  for (const std::string &token : tokens) accepted_and_contained(p, token);
}

// The same table, but the allow list is a wildcard rather than a directory.
TEST_F(path_escape_test, a_glob_entry_is_not_a_way_round_the_traversal_rule) {
  check::access::path_policy p("file", "files", "/settings/logfile");
  p.set_mode("allowed");
  p.set_allow_list(allowed_dir() + "/*.log");
  const std::string logs = allowed_dir();
  for (const std::string &token : {logs + "/../secret/shadow.log", logs + "/..\\..\\secret/shadow.log", logs + "/sub/../../secret/shadow.log"}) {
    accepted_and_contained(p, std::string(token));
  }
}

#ifndef WIN32
// Both link shapes, since only one of them was covered before: a file link and
// a directory link, each planted inside the allowed directory.
TEST_F(path_escape_test, neither_a_file_nor_a_directory_symlink_leads_out) {
  boost::system::error_code ec;
  fs::create_symlink(root_ / "secret" / "shadow", root_ / "logs" / "escape.log", ec);
  if (ec) GTEST_SKIP() << "cannot create symlinks here: " << ec.message();
  fs::create_directory_symlink(root_ / "secret", root_ / "logs" / "out", ec);
  ASSERT_FALSE(ec) << ec.message();

  const check::access::path_policy p = restricted();
  EXPECT_FALSE(accepted_and_contained(p, at("logs/escape.log")));
  EXPECT_FALSE(accepted_and_contained(p, at("logs/out/shadow")));
  // Through the directory link and back out again.
  accepted_and_contained(p, at("logs/out/../secret/shadow"));
}

// A symlink whose own name is a traversal written with a backslash: on Linux
// that is a legal file name, so the resolver does see a link here.
TEST_F(path_escape_test, a_link_named_like_a_traversal_is_resolved_not_pattern_matched) {
  boost::system::error_code ec;
  fs::create_symlink(root_ / "secret", root_ / "logs" / "..\\..", ec);
  if (ec) GTEST_SKIP() << "cannot create that name here: " << ec.message();
  const check::access::path_policy p = restricted();
  accepted_and_contained(p, at("logs") + "/..\\../shadow");
}

// Linux file names are case sensitive, so folding case would hand one allow
// list entry two different files.
TEST_F(path_escape_test, case_is_significant_on_posix) {
  fs::create_directories(root_ / "LOGS");
  write(root_ / "LOGS" / "app.log", "other\n");
  const check::access::path_policy p = restricted();
  EXPECT_FALSE(p.resolve(at("LOGS/app.log")).allowed);
  EXPECT_TRUE(p.resolve(at("logs/app.log")).allowed);
}
#endif

// An allow list of "/" means the whole filesystem. It used to mean nothing at
// all: the containment test looked for a separator after "/" and never found
// one, so every path below it was refused.
TEST_F(path_escape_test, the_filesystem_root_as_an_entry_allows_what_is_below_it) {
  check::access::path_policy p("file", "files", "/settings/logfile");
  p.set_mode("allowed");
#ifdef WIN32
  p.set_allow_list("C:\\");
  EXPECT_TRUE(p.resolve("C:\\Windows\\win.ini").allowed);
  // And the root keeps its separator, or Win32 reads "C:" as the current
  // directory on that drive rather than its root.
  EXPECT_EQ("C:/", check::access::path_policy::canonical("C:\\"));
  EXPECT_EQ("C:/", check::access::path_policy::canonical("C:/"));
#else
  p.set_allow_list("/");
  EXPECT_TRUE(p.resolve("/etc/passwd").allowed);
  EXPECT_TRUE(p.resolve(at("secret/shadow")).allowed);
  EXPECT_EQ("/", check::access::path_policy::canonical("/"));
#endif
}

// A path handed back must never still carry a `..`: the check opens exactly
// what resolve() returned, so an element the resolver left behind would be
// walked after the match had already passed.
TEST_F(path_escape_test, an_accepted_path_never_carries_a_parent_element) {
  const check::access::path_policy p = restricted();
  const std::string logs = allowed_dir();
  for (const std::string &token : {logs + "/sub/../app.log", logs + "/./app.log", logs + "//app.log", logs + "/sub/../sub/deep.log"}) {
    const decision d = p.resolve(std::string(token));
    if (!d.allowed) continue;
    EXPECT_EQ(std::string::npos, d.value.find("/../")) << d.value;
    EXPECT_FALSE(d.value.size() > 3 && d.value.compare(d.value.size() - 3, 3, "/..") == 0) << d.value;
  }
}

// Degenerate allow lists must fail closed rather than match everything.
TEST_F(path_escape_test, a_degenerate_allow_list_allows_nothing) {
  const std::vector<std::string> lists = {"", " ", ",", " , , ", "\t"};
  for (const std::string &list : lists) {
    check::access::path_policy p("file", "files", "/settings/logfile");
    p.set_mode("allowed");
    p.set_allow_list(list);
    EXPECT_EQ(0u, p.allow_list_size()) << "list '" << list << "'";
    EXPECT_FALSE(p.resolve(at("logs/app.log")).allowed) << "list '" << list << "'";
    EXPECT_FALSE(p.resolve(at("secret/shadow")).allowed) << "list '" << list << "'";
  }
}

// A bare `*` is one element, not the whole filesystem: it cannot match a path
// which still has separators in it.
TEST_F(path_escape_test, a_bare_star_entry_does_not_match_an_absolute_path) {
  check::access::path_policy p("file", "files", "/settings/logfile");
  p.set_mode("allowed");
  p.set_allow_list("*");
  EXPECT_FALSE(p.resolve(at("secret/shadow")).allowed);
}

// Reloads happen while checks are running. This hammers both sides: if the
// gate were rebuilt in place - cleared and refilled, as it once was - a reader
// would see an empty list (and accept nothing) or a torn one (and crash under
// a sanitiser). Nothing here may ever be accepted from outside both lists.
TEST_F(path_escape_test, a_reload_never_opens_the_gate_for_a_concurrent_check) {
  check::access::path_policy p("file", "files", "/settings/logfile");
  p.set_mode("allowed");
  p.set_allow_list(allowed_dir());

  std::atomic<bool> stop(false);
  std::atomic<int> escaped(0);
  std::atomic<int> accepted(0);

  std::vector<std::thread> readers;
  for (int i = 0; i < 4; ++i) {
    readers.emplace_back([&] {
      while (!stop.load()) {
        if (p.resolve(at("secret/shadow")).allowed) escaped.fetch_add(1);
        if (p.resolve(at("logs/app.log")).allowed) accepted.fetch_add(1);
        p.allow_list_size();
        p.is_restricted();
      }
    });
  }
  for (int i = 0; i < 300; ++i) {
    p.reset();
    p.set_allow_list(allowed_dir());
    p.set_mode("allowed");
    p.add_predefined("app", at("logs/app.log"));
  }
  stop.store(true);
  for (std::thread &t : readers) t.join();

  EXPECT_EQ(0, escaped.load()) << "a concurrent reload let a path outside the allow list through";
  EXPECT_GT(accepted.load(), 0) << "the allow list was empty for the whole run, so the test proved nothing";
}

// ---------------------------------------------------------------------------
// Negative tests: WQL shapes which must not yield a class
// ---------------------------------------------------------------------------

TEST(wql_negative, refuses_every_query_whose_class_is_not_unambiguous) {
  const std::vector<std::string> queries = {
      // A second FROM: the column list used to swallow the first one and the
      // gate then reported the last class while WMI reads the first (#1516).
      "SELECT a FROM Win32_Foo FROM Win32_Allowed",
      "SELECT a FROM Win32_Foo FROM Win32_Bar FROM Win32_Allowed",
      "SELECT * FROM Win32_Foo FROM Win32_Allowed",
      "SELECT FROM FROM Win32_Allowed",
      // Keywords and punctuation in the column list.
      "SELECT (SELECT * FROM Win32_Shadow) FROM Win32_Allowed",
      "SELECT a.b FROM Win32_Allowed",
      "SELECT a-b FROM Win32_Allowed",
      "SELECT a b FROM Win32_Allowed",
      "SELECT a,, FROM Win32_Allowed",
      "SELECT ,a FROM Win32_Allowed",
      "SELECT a, FROM Win32_Allowed",
      // Qualified or non-identifier class names.
      "SELECT * FROM root\\cimv2:Win32_Process",
      "SELECT * FROM \\\\host\\root\\cimv2:Win32_Process",
      "SELECT * FROM root/cimv2:Win32_Process",
      "SELECT * FROM 9Win32_Process",
      "SELECT * FROM Win32_Process, Win32_Service",
      // Not a SELECT at all.
      "ASSOCIATORS OF {Win32_Process.Handle='1'}",
      "REFERENCES OF {Win32_Process.Handle='1'}",
      "DELETE FROM Win32_Process",
      "UPDATE Win32_Process SET a = 1",
      // Trailing material the grammar does not account for.
      "SELECT * FROM Win32_Process Win32_Other",
      "SELECT * FROM Win32_Process HAVING x",
      "SELECT * FROM Win32_Process GROUP BY a",
      "SELECT * FROM Win32_Process WITHIN 10",
      // Statement separators and embedded NUL.
      "SELECT * FROM Win32_Service; SELECT * FROM Win32_Process",
      std::string("SELECT * FROM Win32_Proc\0ess", 28),
      // Empty and near-empty.
      "",
      "   ",
      "SELECT",
      "SELECT *",
      "SELECT * FROM",
      "SELECT * FROM ",
  };
  for (const std::string &query : queries) {
    const check::wql::parse_result r = check::wql::extract_class(query);
    EXPECT_FALSE(r.ok) << "accepted '" << query << "' as class '" << r.class_name << "'";
    EXPECT_FALSE(r.error.empty()) << "refused '" << query << "' without saying why";
  }
}

// The shapes which must keep working, so the grammar above is not simply
// "refuse everything".
TEST(wql_negative, still_accepts_the_plain_shapes) {
  struct {
    const char *query;
    const char *expected;
  } cases[] = {
      {"SELECT * FROM Win32_Process", "Win32_Process"},
      {"select * from win32_process", "win32_process"},
      {"SELECT Name FROM Win32_Service", "Win32_Service"},
      {"SELECT Name,State FROM Win32_Service", "Win32_Service"},
      {"SELECT Name , State FROM Win32_Service", "Win32_Service"},
      {"SELECT __CLASS, Name FROM Win32_Service", "Win32_Service"},
      {"\n\tSELECT\n*\nFROM\tWin32_Process\n", "Win32_Process"},
      {"SELECT * FROM Win32_Process WHERE Name = 'x'", "Win32_Process"},
      // A WHERE clause is opaque on purpose: WMI evaluates it against the
      // class already approved, so the words in it cannot move the class.
      {"SELECT * FROM Win32_Process WHERE Name = 'FROM Win32_Service'", "Win32_Process"},
      {"SELECT * FROM Win32_Process WHERE Name = 'a;b'", nullptr},
  };
  for (const auto &c : cases) {
    const check::wql::parse_result r = check::wql::extract_class(c.query);
    if (c.expected == nullptr) {
      EXPECT_FALSE(r.ok) << c.query;  // the ';' rule wins, even inside a literal
      continue;
    }
    ASSERT_TRUE(r.ok) << c.query << ": " << r.error;
    EXPECT_EQ(c.expected, r.class_name) << c.query;
  }
}

// ---------------------------------------------------------------------------
// Negative tests: the hierarchical (registry / event log) gate
// ---------------------------------------------------------------------------

TEST(prefix_negative, nothing_outside_the_allowed_subtree_resolves) {
  check::access::prefix_policy p = registry_policy();
  p.set_mode("allowed");
  p.set_allow_list("HKLM\\SOFTWARE\\MyApp");
  const std::vector<std::string> refused = {
      "HKLM\\SOFTWARE\\MyAppEvil",
      "HKLM\\SOFTWARE\\MyAppEvil\\Sub",
      "HKLM\\SOFTWARE\\MyApp2",
      "HKLM\\SOFTWARE",
      "HKLM\\SAM",
      "HKEY_LOCAL_MACHINE\\SAM",
      "HKCU\\SOFTWARE\\MyApp",
      "HKLM\\SOFTWARE\\MyAppEvil\\..\\MyApp",
      // The other separator is not a separator here, so it cannot end a
      // segment on this gate's behalf.
      "HKLM/SOFTWARE/MyApp/Sub",
      "",
  };
  for (const std::string &key : refused) EXPECT_FALSE(p.resolve(key).allowed) << "accepted '" << key << "'";

  const std::vector<std::string> allowed = {
      "HKLM\\SOFTWARE\\MyApp",
      "HKLM\\SOFTWARE\\MyApp\\",
      "HKLM\\SOFTWARE\\MyApp\\Sub",
      "HKLM\\SOFTWARE\\MyApp\\Sub\\Deeper",
      "HKEY_LOCAL_MACHINE\\SOFTWARE\\MyApp\\Sub",
  };
  for (const std::string &key : allowed) EXPECT_TRUE(p.resolve(key).allowed) << "refused '" << key << "'";
}

TEST(prefix_negative, a_degenerate_allow_list_allows_nothing) {
  for (const std::string list : {"", " ", ",", " , , "}) {
    check::access::prefix_policy p("log", "logs", "/settings/eventlog", '/');
    p.set_mode("allowed");
    p.set_allow_list(list);
    EXPECT_EQ(0u, p.allow_list_size());
    EXPECT_FALSE(p.resolve("Application").allowed);
    EXPECT_FALSE(p.resolve("Security").allowed);
  }
}

}  // namespace
