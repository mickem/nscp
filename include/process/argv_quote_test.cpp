// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "argv_quote.hpp"

#include <gtest/gtest.h>

#ifdef _WIN32
// clang-format off
// windows.h must come before shellapi.h - shellapi.h relies on
// EXTERN_C / DECLSPEC_IMPORT / HDROP defined in the Windows SDK headers and
// will fail to compile if pulled in alone.
#include <windows.h>
#include <shellapi.h>
// clang-format on
#endif

#include <string>
#include <vector>

using process::build_command_line_w;
using process::is_batch_target;
using process::quote_argv_w;

// Recognising a batch target is what decides whether a caller's argument values
// are held to the shell rules: CreateProcess cannot execute a .bat or .cmd and
// re-launches cmd.exe for it, which re-parses the command line. Cross-platform,
// because the decision is taken in CheckExternalScripts on every platform.
TEST(IsBatchTarget, RecognisesBatchAndCmdWhateverTheCase) {
  EXPECT_TRUE(is_batch_target("scripts\\check.bat"));
  EXPECT_TRUE(is_batch_target("scripts/check.CMD"));
  EXPECT_TRUE(is_batch_target("C:\\Program Files\\NSClient++\\scripts\\Check.Bat"));
  EXPECT_TRUE(is_batch_target("\"C:\\path with space\\check.bat\"")) << "an operator-quoted path is still a batch file";
  EXPECT_TRUE(is_batch_target("\\\\server\\share\\check.bat")) << "UNC paths take the argv path, which is how this was reachable";
}

TEST(IsBatchTarget, LeavesEverythingElseAlone) {
  for (const char *program : {"check.exe", "powershell.exe", "/usr/bin/check_disk", "scripts/check.ps1", "scripts/check.vbs", "check.batch", "check.bat.exe",
                              "", ".bat.ps1"}) {
    EXPECT_FALSE(is_batch_target(program)) << "program: '" << program << "'";
  }
}

TEST(IsBatchTarget, BareExtensionIsStillABatchFile) {
  // Degenerate, but `.bat` as a whole name is a batch file to CreateProcess.
  EXPECT_TRUE(is_batch_target(".bat"));
  EXPECT_TRUE(is_batch_target(".cmd"));
}

#ifdef _WIN32
// Round-trip a single argument through quote_argv_w and CommandLineToArgvW.
// The result must be exactly one argument, byte-equal to the original.
static std::wstring round_trip(const std::wstring& arg) {
  // Prefix with a fake argv[0] because CommandLineToArgvW treats argv[0]
  // specially (no backslash escaping). We then ignore the first token.
  const std::wstring cmd = L"argv0 " + quote_argv_w(arg);
  int argc = 0;
  LPWSTR* argv = CommandLineToArgvW(cmd.c_str(), &argc);
  EXPECT_NE(argv, nullptr);
  EXPECT_EQ(argc, 2);
  std::wstring got;
  if (argv != nullptr && argc >= 2) {
    got.assign(argv[1]);
  }
  if (argv != nullptr) LocalFree(argv);
  return got;
}
#endif

TEST(QuoteArgvW, BareWordIsNotQuoted) { EXPECT_EQ(quote_argv_w(L"foo"), L"foo"); }

TEST(QuoteArgvW, EmptyStringIsQuoted) { EXPECT_EQ(quote_argv_w(L""), L"\"\""); }

TEST(QuoteArgvW, SpacesForceQuotes) { EXPECT_EQ(quote_argv_w(L"hello world"), L"\"hello world\""); }

TEST(QuoteArgvW, EmbeddedQuoteIsEscaped) { EXPECT_EQ(quote_argv_w(L"a\"b"), L"\"a\\\"b\""); }

TEST(QuoteArgvW, BackslashesNotAdjacentToQuotePassThrough) { EXPECT_EQ(quote_argv_w(L"a\\b\\c"), L"a\\b\\c"); }

TEST(QuoteArgvW, BackslashesBeforeEmbeddedQuoteAreDoubled) {
  // Source: a\"b   ->  "a\\\"b"
  EXPECT_EQ(quote_argv_w(L"a\\\"b"), L"\"a\\\\\\\"b\"");
}

TEST(QuoteArgvW, TrailingBackslashesBeforeClosingQuoteAreDoubled) {
  // Source: needs-quote\   ->  "needs-quote\\"
  EXPECT_EQ(quote_argv_w(L"needs quote\\"), L"\"needs quote\\\\\"");
}

#ifdef _WIN32
TEST(QuoteArgvW, RoundTripsArbitraryStrings) {
  const std::vector<std::wstring> samples{
      L"simple",
      L"",
      L"hello world",
      L"with\ttab",
      L"with\"quote",
      L"with\\backslash",
      L"trailing\\",
      L"trailing\\\\",
      L"\\\"awkward",
      L"a\\b\"c\\d",
      L"--flag=value",
      L"--flag=hello world",
      L"c:\\Program Files\\app\\app.exe",
      L"\"already quoted\"",
  };
  for (const auto& s : samples) {
    EXPECT_EQ(round_trip(s), s) << "round-trip failed for: [" << std::string(s.begin(), s.end()) << "]";
  }
}

TEST(BuildCommandLineW, RoundTripsArgv) {
  const std::vector<std::string> argv{"powershell.exe", "-NoProfile", "-File", "scripts\\check disk.ps1", "-w", "10;reboot", "-c", "20"};
  const std::wstring cmd = build_command_line_w(argv);
  int argc = 0;
  LPWSTR* parsed = CommandLineToArgvW(cmd.c_str(), &argc);
  ASSERT_NE(parsed, nullptr);
  ASSERT_EQ(argc, static_cast<int>(argv.size()));
  for (int i = 0; i < argc; ++i) {
    const std::wstring got = parsed[i];
    EXPECT_EQ(got, std::wstring(argv[i].begin(), argv[i].end())) << "mismatch at i=" << i;
  }
  LocalFree(parsed);
}

TEST(BuildCommandLineW, MetacharsInArgStayInOneArg) {
  // The whole point: $ARGn$ substitution that contains shell-active chars
  // (`;`, `$`, `(`, `)`, spaces) reaches the child as ONE argv element, not
  // multiple.
  const std::vector<std::string> argv{"check.exe", "--input", "10;reboot $(id) value", "--c", "20"};
  const std::wstring cmd = build_command_line_w(argv);
  int argc = 0;
  LPWSTR* parsed = CommandLineToArgvW(cmd.c_str(), &argc);
  ASSERT_NE(parsed, nullptr);
  ASSERT_EQ(argc, 5);
  EXPECT_EQ(std::wstring(parsed[2]), L"10;reboot $(id) value");
  LocalFree(parsed);
}
#endif
