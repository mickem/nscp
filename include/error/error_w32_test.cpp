/*
 * Unit tests for error::win32 (include/error/error_w32.{hpp,cpp}).
 *
 * The module is Windows-only, so the entire translation unit is compiled
 * out on POSIX. The CMake side only links it into the test binary on
 * WIN32, but we add the guard here as well so a stray non-Windows build
 * configuration doesn't break.
 */

#ifdef WIN32

#include <gtest/gtest.h>

#include <error/error_w32.hpp>
#include <string>
#include <win/windows.hpp>

namespace {

// Pick a value that is extremely unlikely to collide with a real Win32
// error code. FormatMessageW will fail with ERROR_MR_MID_NOT_FOUND for
// this, which the production code maps to an empty string.
constexpr DWORD kBogusErrorCode = 0xDEADBEEFu;

// FormatMessageW understands ERROR_FILE_NOT_FOUND on every Windows
// version we care about; the message text is locale-dependent so the
// tests below only check the formatting prefix our code adds, never the
// system-supplied wording.
constexpr DWORD kKnownErrorCode = ERROR_FILE_NOT_FOUND;  // 2

}  // namespace

// ----- lookup() ------------------------------------------------------------

TEST(ErrorWin32Lookup, ReturnsValueSetByLastError) {
  ::SetLastError(0xCAFEBABEu);
  EXPECT_EQ(error::win32::lookup(), 0xCAFEBABEu);
}

TEST(ErrorWin32Lookup, ReturnsZeroWhenLastErrorCleared) {
  ::SetLastError(0);
  EXPECT_EQ(error::win32::lookup(), 0u);
}

// ----- failed() ------------------------------------------------------------

TEST(ErrorWin32Failed, FormatsBothExplicitCodes) {
  const std::string s = error::win32::failed(5, 7);
  EXPECT_NE(s.find("failed to lookup error code: 5"), std::string::npos) << s;
  EXPECT_NE(s.find("reason: 7"), std::string::npos) << s;
}

TEST(ErrorWin32Failed, ZeroSecondArgumentIsFilledFromGetLastError) {
  // When the caller passes 0 for the second arg, failed() consults
  // GetLastError() to obtain the reason. Pre-load it with a known value
  // and confirm it ends up in the formatted output.
  ::SetLastError(123);
  const std::string s = error::win32::failed(99, 0);
  EXPECT_NE(s.find("failed to lookup error code: 99"), std::string::npos) << s;
  EXPECT_NE(s.find("reason: 123"), std::string::npos) << s;
}

TEST(ErrorWin32Failed, ZeroSecondArgumentDefaultsViaOverload) {
  // The header declares the second arg as defaulted to 0; verify that
  // the single-arg call goes through the same GetLastError() path.
  ::SetLastError(456);
  const std::string s = error::win32::failed(7);
  EXPECT_NE(s.find("failed to lookup error code: 7"), std::string::npos) << s;
  EXPECT_NE(s.find("reason: 456"), std::string::npos) << s;
}

// ----- format_message(attrs, module, dwError) ------------------------------

TEST(ErrorWin32FormatMessage, KnownSystemErrorReturnsPrefixedNonEmptyString) {
  // attrs=0 -> the implementation sets FORMAT_MESSAGE_FROM_SYSTEM because
  // module is empty. The output is "<dwError-in-hex>: <localised text>".
  const std::string s = error::win32::format_message(0, "", kKnownErrorCode);
  ASSERT_FALSE(s.empty());
  // dwError prints with %lx, so 2 -> "2: ".
  EXPECT_EQ(s.rfind("2: ", 0), 0u) << s;
  // The trailing system text is locale-dependent, but it is never empty
  // and never just the prefix.
  EXPECT_GT(s.size(), 3u) << s;
}

TEST(ErrorWin32FormatMessage, BogusErrorCodeReturnsEmptyOrFailureString) {
  // FormatMessageW will fail for an unknown code. The production code has
  // two branches:
  //   * GetLastError() == ERROR_MR_MID_NOT_FOUND -> return ""
  //   * otherwise -> return failed(dwError, err)
  // Which branch is taken depends on the Windows build (the loader can
  // surface ERROR_MOD_NOT_FOUND / 126 instead). Both are valid contract
  // outcomes; what we really want to pin is "non-crashing, no garbage".
  const std::string s = error::win32::format_message(0, "", kBogusErrorCode);
  if (!s.empty()) {
    EXPECT_NE(s.find("failed to lookup error code:"), std::string::npos) << s;
  }
}

TEST(ErrorWin32FormatMessage, NonexistentModuleReportsLoadLibraryFailure) {
  // LoadLibraryEx fails -> the function returns the failed() formatting.
  const std::string s = error::win32::format_message(0, "definitely_not_a_real_module_xyz_98765.dll", kKnownErrorCode);
  EXPECT_NE(s.find("failed to lookup error code: 2"), std::string::npos) << s;
}

// ----- format_message(attrs, module, dwError, arguments) -------------------

TEST(ErrorWin32FormatMessageWithArgs, KnownSystemErrorReturnsPrefixedNonEmptyString) {
  // The system message for ERROR_FILE_NOT_FOUND contains no %1/%2 inserts,
  // so passing nullptr as the argument array is safe even though the
  // implementation sets FORMAT_MESSAGE_ARGUMENT_ARRAY. Add
  // FORMAT_MESSAGE_IGNORE_INSERTS as belt-and-braces in case a future
  // locale revision adds inserts.
  const std::string s = error::win32::format_message(FORMAT_MESSAGE_IGNORE_INSERTS, "", kKnownErrorCode, nullptr);
  ASSERT_FALSE(s.empty());
  // This overload prints dwError with %lu, so 2 -> "2: ".
  EXPECT_EQ(s.rfind("2: ", 0), 0u) << s;
  EXPECT_GT(s.size(), 3u) << s;
}

TEST(ErrorWin32FormatMessageWithArgs, BogusErrorCodeReturnsFailureString) {
  // The two-arg overload does NOT translate ERROR_MR_MID_NOT_FOUND to
  // "" - it falls through to failed(). Pin that behaviour so a future
  // refactor doesn't accidentally change it without anyone noticing.
  const std::string s = error::win32::format_message(FORMAT_MESSAGE_IGNORE_INSERTS, "", kBogusErrorCode, nullptr);
  EXPECT_NE(s.find("failed to lookup error code:"), std::string::npos) << s;
}

TEST(ErrorWin32FormatMessageWithArgs, NonexistentModuleReportsLoadLibraryFailure) {
  const std::string s = error::win32::format_message(FORMAT_MESSAGE_IGNORE_INSERTS, "definitely_not_a_real_module_xyz_98765.dll", kKnownErrorCode, nullptr);
  EXPECT_NE(s.find("failed to lookup error code: 2"), std::string::npos) << s;
}

#endif  // WIN32
