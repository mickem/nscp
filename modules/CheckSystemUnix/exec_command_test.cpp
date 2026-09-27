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
}  // namespace

TEST(exec_command, inventory_rejects_failed_commands_and_partial_stdout) {
  EXPECT_EQ(system_exec::run_inventory_command({"/bin/sh", "-c", "printf complete"}), "complete");
  EXPECT_THROW(system_exec::run_inventory_command({"/bin/sh", "-c", "printf partial; exit 7"}), std::runtime_error);
  EXPECT_THROW(system_exec::run_inventory_command({"/nonexistent-systemctl"}), std::runtime_error);
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
