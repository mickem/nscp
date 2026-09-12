// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <check/access_policy.hpp>
#include <check/path_access_policy.hpp>
#include <check/prefix_access_policy.hpp>
#include <check/wql_query.hpp>
#include <fstream>

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

}  // namespace
