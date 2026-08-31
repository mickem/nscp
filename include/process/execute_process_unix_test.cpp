// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

/*
 * Unit tests for the Unix process launcher (execute_process_unix.cpp).
 *
 * Coverage:
 *   - argv path (fork + execvp): stdout/stderr capture, exit-code mapping,
 *     large output spanning multiple reads, exec failure (127), timeout kill
 *   - legacy popen path (argv empty): shell execution and exit codes
 *
 * Everything runs real child processes against /bin/sh and friends, which is
 * deterministic on any POSIX build host.
 */

#include <gtest/gtest.h>

#include <NSCAPI.h>
#include <ctime>
#include <process/execute_process.hpp>
#include <string>
#include <vector>

namespace {

process::exec_arguments make_args(const std::string& command, unsigned int timeout = 10) {
  process::exec_arguments args("", command, timeout, "", "", false, false, false);
  args.alias = "test_command";
  return args;
}

}  // namespace

// =============================================================================
// argv path (fork + execvp, no shell)
// =============================================================================

TEST(ExecuteProcessUnix, ArgvCapturesStdout) {
  process::exec_arguments args = make_args("echo");
  args.argv = {"/bin/echo", "hello", "world"};
  std::string output;
  const int ret = process::execute_process(args, output);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(output, "hello world\n");
}

TEST(ExecuteProcessUnix, ArgvArgumentsAreNotShellInterpreted) {
  // Metacharacters must arrive verbatim: there is no shell on this path.
  process::exec_arguments args = make_args("echo");
  args.argv = {"/bin/echo", "$(reboot); `id`", "a;b|c"};
  std::string output;
  const int ret = process::execute_process(args, output);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(output, "$(reboot); `id` a;b|c\n");
}

TEST(ExecuteProcessUnix, ArgvMapsExitCode) {
  process::exec_arguments args = make_args("sh");
  args.argv = {"/bin/sh", "-c", "echo crit output; exit 2"};
  std::string output;
  const int ret = process::execute_process(args, output);
  EXPECT_EQ(ret, 2);
  EXPECT_EQ(output, "crit output\n");
}

TEST(ExecuteProcessUnix, ArgvCapturesStderr) {
  process::exec_arguments args = make_args("sh");
  args.argv = {"/bin/sh", "-c", "echo to-stderr 1>&2"};
  std::string output;
  const int ret = process::execute_process(args, output);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(output, "to-stderr\n");
}

TEST(ExecuteProcessUnix, ArgvCollectsOutputLargerThanOneBuffer) {
  // 9000 bytes of "a\n" pairs forces several reads through the 4096-byte
  // buffer in drain_with_timeout.
  process::exec_arguments args = make_args("sh");
  args.argv = {"/bin/sh", "-c", "yes a | head -c 9000"};
  std::string output;
  const int ret = process::execute_process(args, output);
  EXPECT_EQ(ret, 0);
  ASSERT_EQ(output.size(), 9000u);
  EXPECT_EQ(output.substr(0, 4), "a\na\n");
}

TEST(ExecuteProcessUnix, ArgvExecFailureReturns127) {
  process::exec_arguments args = make_args("missing");
  args.argv = {"/no/such/binary/anywhere"};
  std::string output;
  const int ret = process::execute_process(args, output);
  EXPECT_EQ(ret, 127);
  EXPECT_EQ(output, "");
}

TEST(ExecuteProcessUnix, ArgvTimeoutKillsChildAndReportsUnknown) {
  process::exec_arguments args = make_args("sleep", 1);
  args.argv = {"/bin/sleep", "30"};
  std::string output;
  const time_t start = time(nullptr);
  const int ret = process::execute_process(args, output);
  const time_t elapsed = time(nullptr) - start;

  EXPECT_EQ(ret, NSCAPI::query_return_codes::returnUNKNOWN);
  EXPECT_EQ(output, "Command test_command didn't terminate within 1s; killed");
  // SIGTERM should end /bin/sleep promptly; well before its 30s runtime.
  EXPECT_LT(elapsed, 10);
}

// =============================================================================
// legacy popen path (argv empty, /bin/sh -c)
// =============================================================================

TEST(ExecuteProcessUnix, PopenRunsCommandThroughShell) {
  process::exec_arguments args = make_args("echo popen test && echo second");
  std::string output;
  const int ret = process::execute_process(args, output);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(output, "popen test\nsecond\n");
}

TEST(ExecuteProcessUnix, PopenMapsExitCode) {
  process::exec_arguments args = make_args("exit 3");
  std::string output;
  const int ret = process::execute_process(args, output);
  EXPECT_EQ(ret, 3);
  EXPECT_EQ(output, "");
}
