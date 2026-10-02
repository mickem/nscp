// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

/*
 * Unit tests for the Unix process launcher (execute_process_unix.cpp).
 *
 * Coverage:
 *   - argv path (fork + execvp): stdout/stderr capture, exit-code mapping,
 *     large output spanning multiple reads, exec failure (127), timeout kill
 *   - legacy popen path (argv empty): shell execution and exit codes
 *   - run-as settings (user/domain/password): refused, never silently ignored
 *   - kill tree: a backgrounded helper dies with the script on timeout and on
 *     kill_all(), and the script runs in a session of its own
 *   - kill_all() ends a running child and the check says why; a script killed
 *     by a signal says which; stdin is /dev/null
 *
 * Everything runs real child processes against /bin/sh and friends, which is
 * deterministic on any POSIX build host.
 */

#include <gtest/gtest.h>

#include <NSCAPI.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <ctime>
#include <process/execute_process.hpp>
#include <string>
#include <threads/guarded_thread.hpp>
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

// =============================================================================
// run-as settings: Windows-only, refused here rather than silently ignored
// =============================================================================

TEST(ExecuteProcessUnix, RunAsUserIsRefusedOnArgvPath) {
  // Before the guard the script ran as the service identity with `user`
  // ignored; an operator sandboxing an untrusted script with `user = nobody`
  // got root (or the service account) instead. The command must not execute.
  process::exec_arguments args = make_args("touch");
  args.user = "nobody";
  args.argv = {"/bin/sh", "-c", "echo RAN"};
  std::string output;
  const int ret = process::execute_process(args, output);
  EXPECT_EQ(ret, NSCAPI::query_return_codes::returnUNKNOWN);
  EXPECT_EQ(output.find("RAN"), std::string::npos);
  EXPECT_NE(output.find("only supported on Windows"), std::string::npos);
  EXPECT_NE(output.find("sudo"), std::string::npos);
  EXPECT_NE(output.find("test_command"), std::string::npos);
}

TEST(ExecuteProcessUnix, RunAsUserIsRefusedOnShellPath) {
  process::exec_arguments args = make_args("echo RAN");
  args.user = "nobody";
  std::string output;
  const int ret = process::execute_process(args, output);
  EXPECT_EQ(ret, NSCAPI::query_return_codes::returnUNKNOWN);
  EXPECT_EQ(output.find("RAN"), std::string::npos);
}

TEST(ExecuteProcessUnix, PasswordOrDomainAloneIsRefused) {
  // A password (or domain) without a user is a misconfiguration on every
  // platform; it must not slip through because `user` happens to be empty.
  for (const char* which : {"password", "domain"}) {
    process::exec_arguments args = make_args("echo RAN");
    args.argv = {"/bin/echo", "RAN"};
    if (std::string(which) == "password")
      args.password = "secret";
    else
      args.domain = "EXAMPLE";
    std::string output;
    const int ret = process::execute_process(args, output);
    EXPECT_EQ(ret, NSCAPI::query_return_codes::returnUNKNOWN) << which;
    EXPECT_EQ(output.find("RAN"), std::string::npos) << which;
    // The refusal must never echo the secret.
    EXPECT_EQ(output.find("secret"), std::string::npos) << which;
  }
}

// =============================================================================
// kill tree: the script leads a process group, and the group is what dies
// =============================================================================

namespace {

// Start `sleep 30` in the background from a shell, record its pid in `pidfile`,
// then wait for it - a script that leaves a helper holding the pipe open.
std::string backgrounded_helper_script(const std::string& pidfile) { return "/bin/sleep 30 & echo $! > " + pidfile + "; wait"; }

pid_t read_pid(const std::string& pidfile) {
  FILE* f = fopen(pidfile.c_str(), "r");
  if (!f) return 0;
  long pid = 0;
  const int n = fscanf(f, "%ld", &pid);
  fclose(f);
  return n == 1 ? static_cast<pid_t>(pid) : 0;
}

// A process that no longer runs: gone, or a zombie waiting for init to reap it
// (a container's pid 1 is not always quick about that).
bool is_terminated(pid_t pid) {
  if (kill(pid, 0) != 0) return errno == ESRCH;
#if defined(__linux__)
  const std::string stat = "/proc/" + std::to_string(pid) + "/stat";
  FILE* f = fopen(stat.c_str(), "r");
  if (!f) return true;
  char buf[512] = {0};
  const size_t n = fread(buf, 1, sizeof(buf) - 1, f);
  fclose(f);
  // "pid (comm) state ..." - comm may contain spaces, so find the last ')'.
  const char* close_paren = n ? strrchr(buf, ')') : nullptr;
  return close_paren != nullptr && close_paren[1] == ' ' && close_paren[2] == 'Z';
#else
  return false;
#endif
}

bool wait_terminated(pid_t pid, int attempts = 50) {
  for (int i = 0; i < attempts; ++i) {
    if (is_terminated(pid)) return true;
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 100 * 1000 * 1000;
    nanosleep(&ts, nullptr);
  }
  return is_terminated(pid);
}

std::string temp_pidfile() {
  char path[] = "/tmp/nscp-killtree-XXXXXX";
  const int fd = mkstemp(path);
  if (fd >= 0) close(fd);
  return path;
}

}  // namespace

TEST(ExecuteProcessUnix, KillTreeEndsBackgroundedHelperOnTimeout) {
  const std::string pidfile = temp_pidfile();
  process::exec_arguments args = make_args("helper", 1);
  args.argv = {"/bin/sh", "-c", backgrounded_helper_script(pidfile)};
  args.kill_tree = true;
  std::string output;
  const int ret = process::execute_process(args, output);
  const pid_t helper = read_pid(pidfile);
  unlink(pidfile.c_str());

  EXPECT_EQ(ret, NSCAPI::query_return_codes::returnUNKNOWN);
  EXPECT_EQ(output, "Command test_command didn't terminate within 1s; killed");
  ASSERT_GT(helper, 0);
  EXPECT_TRUE(wait_terminated(helper)) << "helper " << helper << " outlived the script it was started from";
  kill(helper, SIGKILL);  // never leave one behind if the expectation failed
}

TEST(ExecuteProcessUnix, WithoutKillTreeBackgroundedHelperSurvives) {
  // Pins the other side of the switch: off, only the script itself is
  // signalled, as before - so an operator who turns it on sees a change.
  const std::string pidfile = temp_pidfile();
  process::exec_arguments args = make_args("helper", 1);
  args.argv = {"/bin/sh", "-c", backgrounded_helper_script(pidfile)};
  args.kill_tree = false;
  std::string output;
  const int ret = process::execute_process(args, output);
  const pid_t helper = read_pid(pidfile);
  unlink(pidfile.c_str());

  EXPECT_EQ(ret, NSCAPI::query_return_codes::returnUNKNOWN);
  ASSERT_GT(helper, 0);
  EXPECT_FALSE(is_terminated(helper)) << "helper " << helper << " died although kill tree was off";
  kill(helper, SIGKILL);
  wait_terminated(helper);
}

// Poll until `path` has content: the script has started. With the child
// registered under the lock that is held across fork(), a started script is a
// registered one, so kill_all() after this cannot miss it however loaded the
// runner is.
bool wait_for_file(const std::string& path, int timeout_seconds = 20) {
  const time_t deadline = time(nullptr) + timeout_seconds;
  while (time(nullptr) < deadline) {
    FILE* f = fopen(path.c_str(), "r");
    if (f) {
      const int c = fgetc(f);
      fclose(f);
      if (c != EOF) return true;
    }
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 20 * 1000 * 1000;
    nanosleep(&ts, nullptr);
  }
  return false;
}

// Run execute_process on a guarded worker thread, kill_all() once the script
// has started, and return how long the worker took to come back after that.
time_t run_and_kill_all(process::exec_arguments args, const std::string& started, int& ret, std::string& output) {
  std::string escaped;
  ret = -1;
  const auto worker = threads::start_guarded_thread(
      "execute_process_test worker", [&] { ret = process::execute_process(args, output); }, [&escaped](const std::string& line) { escaped = line; });
  EXPECT_TRUE(wait_for_file(started)) << "the script never started";
  const time_t killed_at = time(nullptr);
  process::kill_all();
  worker->join();
  EXPECT_EQ(escaped, "");
  return time(nullptr) - killed_at;
}

TEST(ExecuteProcessUnix, KillAllEndsRunningChildren) {
  // kill_all() is what module unload calls; a worker blocked on a script that
  // will not finish must come back, well before the script's own timeout, and
  // say why the script ended.
  const std::string started = temp_pidfile();
  process::exec_arguments args = make_args("sleep", 60);
  args.argv = {"/bin/sh", "-c", "echo up > " + started + "; exec /bin/sleep 60"};
  int ret = -1;
  std::string output;
  const time_t elapsed = run_and_kill_all(args, started, ret, output);
  unlink(started.c_str());

  EXPECT_EQ(ret, NSCAPI::query_return_codes::returnUNKNOWN);
  EXPECT_EQ(output, "Command test_command was killed: the module is unloading");
  EXPECT_LT(elapsed, 10);
}

TEST(ExecuteProcessUnix, KillAllWithKillTreeEndsBackgroundedHelper) {
  const std::string started = temp_pidfile();
  process::exec_arguments args = make_args("helper", 60);
  args.argv = {"/bin/sh", "-c", "/bin/sleep 60 & echo $! > " + started + "; wait"};
  args.kill_tree = true;
  int ret = -1;
  std::string output;
  const time_t elapsed = run_and_kill_all(args, started, ret, output);
  const pid_t helper = read_pid(started);
  unlink(started.c_str());

  EXPECT_EQ(ret, NSCAPI::query_return_codes::returnUNKNOWN);
  EXPECT_EQ(output, "Command test_command was killed: the module is unloading");
  EXPECT_LT(elapsed, 10);
  ASSERT_GT(helper, 0);
  EXPECT_TRUE(wait_terminated(helper)) << "helper " << helper << " outlived the unload";
  kill(helper, SIGKILL);
}

TEST(ExecuteProcessUnix, ScriptKilledBySignalSaysSo) {
  process::exec_arguments args = make_args("suicide");
  args.argv = {"/bin/sh", "-c", "echo partial; kill -9 $$"};
  std::string output;
  const int ret = process::execute_process(args, output);
  EXPECT_EQ(ret, NSCAPI::query_return_codes::returnUNKNOWN);
  EXPECT_EQ(output, "Command test_command was terminated by signal 9 (SIGKILL)\npartial\n");
}

#if defined(__linux__)
TEST(ExecuteProcessUnix, StdinIsDevNullWithAndWithoutKillTree) {
  for (const bool kill_tree : {false, true}) {
    process::exec_arguments args = make_args("stdin");
    args.argv = {"/bin/sh", "-c", "readlink /proc/$$/fd/0"};
    args.kill_tree = kill_tree;
    std::string output;
    const int ret = process::execute_process(args, output);
    EXPECT_EQ(ret, 0) << "kill_tree=" << kill_tree;
    EXPECT_EQ(output, "/dev/null\n") << "kill_tree=" << kill_tree;
  }
}

TEST(ExecuteProcessUnix, KillTreeRunsTheScriptInASessionOfItsOwn) {
  // A session of its own has no controlling terminal, so a script that touches
  // the tty cannot be stopped by SIGTTIN/SIGTTOU as a background group would.
  // Field 6 of /proc/<pid>/stat is the session id; for /bin/sh the comm field
  // has no spaces, so a plain field split is safe.
  for (const bool kill_tree : {false, true}) {
    process::exec_arguments args = make_args("session");
    args.argv = {"/bin/sh", "-c", "echo $$ $(cut -d' ' -f6 /proc/$$/stat)"};
    args.kill_tree = kill_tree;
    std::string output;
    const int ret = process::execute_process(args, output);
    ASSERT_EQ(ret, 0) << output;
    long pid = 0, sid = 0;
    ASSERT_EQ(sscanf(output.c_str(), "%ld %ld", &pid, &sid), 2) << output;
    if (kill_tree)
      EXPECT_EQ(pid, sid) << "with kill tree the script should lead its own session";
    else
      EXPECT_NE(pid, sid) << "without kill tree the script should stay in the agent's session";
  }
}
#endif
