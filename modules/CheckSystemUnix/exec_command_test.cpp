// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "exec_command.h"

#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>

namespace {
double seconds_since(const std::chrono::steady_clock::time_point started) {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
}

std::string inventory_error(const std::vector<std::string> &argv, int timeout_ms = 30000, std::size_t max_output = 4 * 1024 * 1024) {
  try {
    system_exec::run_inventory_command(argv, timeout_ms, max_output);
    ADD_FAILURE() << "Expected an inventory command failure";
  } catch (const std::runtime_error &error) {
    return error.what();
  }
  return "";
}
}  // namespace

TEST(exec_command, inventory_rejects_failed_commands_and_partial_stdout) {
  EXPECT_EQ(system_exec::run_inventory_command({"/bin/sh", "-c", "printf complete"}), "complete");
  EXPECT_THROW(system_exec::run_inventory_command({"/bin/sh", "-c", "printf partial; exit 7"}), std::runtime_error);
  EXPECT_THROW(system_exec::run_inventory_command({"/nonexistent-systemctl"}), std::runtime_error);
}

TEST(exec_command, inventory_errors_identify_command_condition_status_and_stderr) {
  const auto failed = inventory_error({"/bin/sh", "-c", "printf 'no running manager' >&2; exit 7"});
  EXPECT_NE(failed.find("/bin/sh"), std::string::npos);
  EXPECT_NE(failed.find("nonzero exit"), std::string::npos);
  EXPECT_NE(failed.find("exit_code=7"), std::string::npos);
  EXPECT_NE(failed.find("stderr: no running manager"), std::string::npos);
  const auto missing = inventory_error({"/nonexistent-systemctl"});
  EXPECT_NE(missing.find("/nonexistent-systemctl"), std::string::npos);
  EXPECT_NE(missing.find("exec or child setup failed"), std::string::npos);
  EXPECT_NE(missing.find("exit_code=127"), std::string::npos);
  const auto limited = inventory_error({"/bin/sh", "-c", "printf 12345"}, 30000, 4);
  EXPECT_NE(limited.find("stdout limit exceeded (4 bytes)"), std::string::npos);
  EXPECT_NE(limited.find("exit_code="), std::string::npos);
  const auto timeout = inventory_error({"/bin/sh", "-c", "exec sleep 30"}, 50);
  EXPECT_NE(timeout.find("timed out after 50 ms"), std::string::npos);
}

TEST(exec_command, stderr_is_separate_bounded_and_drained) {
  const auto result =
      system_exec::run({"/bin/sh", "-c", "i=0; while [ $i -lt 10000 ]; do printf diagnostic >&2; i=$((i+1)); done; printf complete"}, 30000, 8, true);
  EXPECT_EQ(result.output, "complete");
  EXPECT_EQ(result.error_output.size(), 4096u);
  EXPECT_EQ(result.exit_code, 0);
  EXPECT_FALSE(result.output_failed);
  EXPECT_FALSE(result.timed_out);
}

TEST(exec_command, program_exit_127_is_not_an_exec_failure) {
  const auto result = system_exec::run({"/bin/sh", "-c", "exit 127"});
  EXPECT_TRUE(result.started);
  EXPECT_EQ(result.exit_code, 127);
  EXPECT_TRUE(result.failure.empty());
}

TEST(exec_command, inventory_enforces_the_output_limit) {
  EXPECT_EQ(system_exec::run_inventory_command({"/bin/sh", "-c", "printf 1234"}, 30000, 4), "1234");
  EXPECT_THROW(system_exec::run_inventory_command({"/bin/sh", "-c", "printf 12345"}, 30000, 4), std::runtime_error);
}

TEST(exec_command, inventory_rejects_timeouts_even_after_stdout_closes) {
  EXPECT_THROW(system_exec::run_inventory_command({"/bin/sh", "-c", "printf partial; exec sleep 30"}, 100), std::runtime_error);
  EXPECT_THROW(system_exec::run_inventory_command({"/bin/sh", "-c", "exec >&-; exec sleep 30"}, 100), std::runtime_error);
}

TEST(exec_command, captures_stdout_and_the_exit_code) {
  const system_exec::exec_result r = system_exec::run({"/bin/sh", "-c", "echo hello; exit 3"});
  EXPECT_TRUE(r.started);
  EXPECT_FALSE(r.timed_out);
  EXPECT_EQ(r.output, "hello\n");
  EXPECT_EQ(r.exit_code, 3);
}

TEST(exec_command, a_program_that_does_not_exist_did_not_start) {
  const system_exec::exec_result r = system_exec::run({"/nonexistent/program"});
  EXPECT_FALSE(r.started);
}

TEST(exec_command, a_child_that_never_closes_stdout_is_killed_at_the_deadline) {
  const auto started = std::chrono::steady_clock::now();
  const system_exec::exec_result r = system_exec::run({"/bin/sh", "-c", "echo partial; exec sleep 30"}, 500);
  EXPECT_TRUE(r.timed_out);
  EXPECT_EQ(r.output, "partial\n");
  EXPECT_EQ(r.exit_code, -1);
  EXPECT_LT(seconds_since(started), 10.0);
}

TEST(exec_command, a_child_that_closes_stdout_and_lingers_is_killed_at_the_deadline) {
  // What a daemonising helper does: end of output long before the process
  // ends. The wait for its exit is under the same deadline as the read.
  const auto started = std::chrono::steady_clock::now();
  const system_exec::exec_result r = system_exec::run({"/bin/sh", "-c", "echo done; exec >&-; exec sleep 30"}, 500);
  EXPECT_TRUE(r.timed_out);
  EXPECT_EQ(r.output, "done\n");
  EXPECT_EQ(r.exit_code, -1);
  EXPECT_LT(seconds_since(started), 10.0);
}
