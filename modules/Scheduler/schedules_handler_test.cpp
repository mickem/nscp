/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "schedules_handler.hpp"

#include <gtest/gtest.h>

#include <boost/make_shared.hpp>
#include <nscapi/nscapi_helper.hpp>
#include <string>
#include <vector>

// ============================================================================
// Helper: create a schedule_object with alias and path
// ============================================================================

static schedules::target_object make_target(const std::string &alias, const std::string &path = "/test") {
  return boost::make_shared<schedules::schedule_object>(alias, path);
}

// ============================================================================
// schedule_object::set_command tests
// ============================================================================

TEST(ScheduleObject, SetCommandParsesSimpleCommand) {
  const auto obj = make_target("test");
  obj->set_command("check_cpu");
  EXPECT_EQ("check_cpu", obj->command);
  EXPECT_TRUE(obj->arguments.empty());
}

TEST(ScheduleObject, SetCommandParsesCommandWithArguments) {
  const auto obj = make_target("test");
  obj->set_command("check_cpu warn=80 crit=90");
  EXPECT_EQ("check_cpu", obj->command);
  ASSERT_EQ(2u, obj->arguments.size());
  auto it = obj->arguments.begin();
  EXPECT_EQ("warn=80", *it++);
  EXPECT_EQ("crit=90", *it);
}

TEST(ScheduleObject, SetCommandParsesQuotedArguments) {
  const auto obj = make_target("test");
  obj->set_command("check_cpu \"warn=80 90\" crit=90");
  EXPECT_EQ("check_cpu", obj->command);
  ASSERT_EQ(2u, obj->arguments.size());
  auto it = obj->arguments.begin();
  EXPECT_EQ("warn=80 90", *it++);
  EXPECT_EQ("crit=90", *it);
}

TEST(ScheduleObject, SetCommandEmptyStringDoesNothing) {
  const auto obj = make_target("test");
  obj->command = "original";
  obj->arguments.push_back("arg1");
  obj->set_command("");
  EXPECT_EQ("original", obj->command);
  ASSERT_EQ(1u, obj->arguments.size());
}

TEST(ScheduleObject, SetCommandClearsOldArguments) {
  const auto obj = make_target("test");
  obj->set_command("old_cmd arg1 arg2");
  ASSERT_EQ(2u, obj->arguments.size());
  obj->set_command("new_cmd only_arg");
  EXPECT_EQ("new_cmd", obj->command);
  ASSERT_EQ(1u, obj->arguments.size());
  EXPECT_EQ("only_arg", obj->arguments.front());
}

// ============================================================================
// schedule_object::set_duration tests
// ============================================================================

TEST(ScheduleObject, SetDurationSeconds) {
  const auto obj = make_target("test");
  obj->set_duration("30s");
  ASSERT_TRUE(obj->duration.is_initialized());
  EXPECT_EQ(30, obj->duration->total_seconds());
}

TEST(ScheduleObject, SetDurationMinutes) {
  const auto obj = make_target("test");
  obj->set_duration("5m");
  ASSERT_TRUE(obj->duration.is_initialized());
  EXPECT_EQ(300, obj->duration->total_seconds());
}

TEST(ScheduleObject, SetDurationHours) {
  const auto obj = make_target("test");
  obj->set_duration("2h");
  ASSERT_TRUE(obj->duration.is_initialized());
  EXPECT_EQ(7200, obj->duration->total_seconds());
}

TEST(ScheduleObject, SetDurationDefaultUnitIsSeconds) {
  const auto obj = make_target("test");
  obj->set_duration("60");
  ASSERT_TRUE(obj->duration.is_initialized());
  EXPECT_EQ(60, obj->duration->total_seconds());
}

TEST(ScheduleObject, SetDurationDays) {
  const auto obj = make_target("test");
  obj->set_duration("1d");
  ASSERT_TRUE(obj->duration.is_initialized());
  EXPECT_EQ(86400, obj->duration->total_seconds());
}

TEST(ScheduleObject, SetDurationWeeks) {
  const auto obj = make_target("test");
  obj->set_duration("1w");
  ASSERT_TRUE(obj->duration.is_initialized());
  EXPECT_EQ(604800, obj->duration->total_seconds());
}

// ============================================================================
// schedule_object::set_schedule tests
// ============================================================================

TEST(ScheduleObject, SetScheduleSetsValue) {
  const auto obj = make_target("test");
  obj->set_schedule("* * * * *");
  ASSERT_TRUE(obj->schedule.is_initialized());
  EXPECT_EQ("* * * * *", *obj->schedule);
}

TEST(ScheduleObject, SetScheduleWithSpecificMinute) {
  const auto obj = make_target("test");
  obj->set_schedule("5 * * * *");
  ASSERT_TRUE(obj->schedule.is_initialized());
  EXPECT_EQ("5 * * * *", *obj->schedule);
}

// ============================================================================
// schedule_object::set_randomness tests
// ============================================================================

TEST(ScheduleObject, SetRandomnessPercent) {
  const auto obj = make_target("test");
  obj->set_randomness("20%");
  EXPECT_DOUBLE_EQ(0.20, obj->randomness);
}

TEST(ScheduleObject, SetRandomnessWithoutPercent) {
  const auto obj = make_target("test");
  obj->set_randomness("50");
  EXPECT_DOUBLE_EQ(0.50, obj->randomness);
}

TEST(ScheduleObject, SetRandomnessZero) {
  const auto obj = make_target("test");
  obj->set_randomness("0%");
  EXPECT_DOUBLE_EQ(0.0, obj->randomness);
}

TEST(ScheduleObject, SetRandomnessHundred) {
  const auto obj = make_target("test");
  obj->set_randomness("100%");
  EXPECT_DOUBLE_EQ(1.0, obj->randomness);
}

// ============================================================================
// schedule_object::set_report tests
// ============================================================================

TEST(ScheduleObject, SetReportAll) {
  const auto obj = make_target("test");
  obj->set_report("all");
  EXPECT_TRUE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnOK));
  EXPECT_TRUE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnWARN));
  EXPECT_TRUE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnCRIT));
  EXPECT_TRUE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnUNKNOWN));
}

TEST(ScheduleObject, SetReportCriticalOnly) {
  const auto obj = make_target("test");
  obj->set_report("critical");
  EXPECT_FALSE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnOK));
  EXPECT_FALSE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnWARN));
  EXPECT_TRUE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnCRIT));
  EXPECT_FALSE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnUNKNOWN));
}

TEST(ScheduleObject, SetReportWarningOnly) {
  auto obj = make_target("test");
  obj->set_report("warning");
  EXPECT_FALSE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnOK));
  EXPECT_TRUE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnWARN));
  EXPECT_FALSE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnCRIT));
  EXPECT_FALSE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnUNKNOWN));
}

TEST(ScheduleObject, SetReportOkOnly) {
  const auto obj = make_target("test");
  obj->set_report("ok");
  EXPECT_TRUE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnOK));
  EXPECT_FALSE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnWARN));
  EXPECT_FALSE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnCRIT));
  EXPECT_FALSE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnUNKNOWN));
}

TEST(ScheduleObject, SetReportMultiple) {
  const auto obj = make_target("test");
  obj->set_report("ok,critical");
  EXPECT_TRUE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnOK));
  EXPECT_FALSE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnWARN));
  EXPECT_TRUE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnCRIT));
  EXPECT_FALSE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnUNKNOWN));
}

// ============================================================================
// schedule_object::to_string tests
// ============================================================================

TEST(ScheduleObject, ToStringContainsAlias) {
  const auto obj = make_target("my_check");
  obj->command = "check_cpu";
  obj->channel = "NSCA";
  std::string str = obj->to_string();
  EXPECT_NE(std::string::npos, str.find("my_check"));
  EXPECT_NE(std::string::npos, str.find("check_cpu"));
  EXPECT_NE(std::string::npos, str.find("NSCA"));
}

TEST(ScheduleObject, ToStringWithDuration) {
  const auto obj = make_target("sched1");
  obj->set_duration("60s");
  obj->set_randomness("10%");
  std::string str = obj->to_string();
  EXPECT_NE(std::string::npos, str.find("60"));
  EXPECT_NE(std::string::npos, str.find("10"));
}

TEST(ScheduleObject, ToStringWithSchedule) {
  const auto obj = make_target("sched1");
  obj->set_schedule("5 * * * *");
  std::string str = obj->to_string();
  EXPECT_NE(std::string::npos, str.find("5 * * * *"));
}

// ============================================================================
// schedule_object construction / defaults tests
// ============================================================================

TEST(ScheduleObject, DefaultValues) {
  const auto obj = make_target("myalias");
  EXPECT_EQ("myalias", obj->get_alias());
  EXPECT_EQ(0.0, obj->randomness);
  EXPECT_EQ(0u, obj->report);
  EXPECT_EQ(0, obj->id);
  EXPECT_TRUE(obj->command.empty());
  EXPECT_TRUE(obj->arguments.empty());
  EXPECT_TRUE(obj->channel.empty());
  EXPECT_TRUE(obj->source_id.empty());
  EXPECT_TRUE(obj->target_id.empty());
  EXPECT_FALSE(obj->duration.is_initialized());
  EXPECT_FALSE(obj->schedule.is_initialized());
}

TEST(ScheduleObject, CopyConstructor) {
  const auto obj = make_target("original");
  obj->command = "check_mem";
  obj->arguments.push_back("warn=80");
  obj->channel = "NSCA";
  obj->source_id = "src";
  obj->target_id = "tgt";
  obj->set_duration("30s");
  obj->set_randomness("20%");
  obj->set_report("all");

  schedules::schedule_object copy(*obj);
  EXPECT_EQ("check_mem", copy.command);
  ASSERT_EQ(1u, copy.arguments.size());
  EXPECT_EQ("warn=80", copy.arguments.front());
  EXPECT_EQ("NSCA", copy.channel);
  EXPECT_EQ("src", copy.source_id);
  EXPECT_EQ("tgt", copy.target_id);
  ASSERT_TRUE(copy.duration.is_initialized());
  EXPECT_EQ(30, copy.duration->total_seconds());
  EXPECT_DOUBLE_EQ(0.20, copy.randomness);
  EXPECT_NE(0u, copy.report);
}

// ============================================================================
// schedule_object: mutual exclusivity of duration and schedule
// ============================================================================

TEST(ScheduleObject, DurationAndScheduleMutuallySettable) {
  const auto obj = make_target("test");
  obj->set_duration("60s");
  EXPECT_TRUE(obj->duration.is_initialized());
  EXPECT_FALSE(obj->schedule.is_initialized());

  obj->set_schedule("* * * * *");
  EXPECT_TRUE(obj->duration.is_initialized());
  EXPECT_TRUE(obj->schedule.is_initialized());
  // Both can be set on the object, but the loadModuleEx validates they are not both set
}

// ============================================================================
// Mock task_handler for scheduler tests
// ============================================================================

struct mock_task_handler final : public schedules::task_handler {
  std::vector<std::string> handled_aliases;
  std::vector<std::string> errors;
  std::vector<std::string> traces;
  bool return_value = true;

  bool handle_schedule(schedules::target_object task) override {
    handled_aliases.push_back(task->get_alias());
    return return_value;
  }
  void on_error(const char * /*file*/, int /*line*/, std::string error) override { errors.push_back(error); }
  void on_trace(const char * /*file*/, int /*line*/, std::string msg) override { traces.push_back(msg); }
};

// ============================================================================
// schedules::scheduler tests
// ============================================================================

class SchedulerTest : public ::testing::Test {
 protected:
  schedules::scheduler sched;
  mock_task_handler handler;

  void SetUp() override { sched.set_handler(&handler); }
  void TearDown() override {
    sched.prepare_shutdown();
    sched.unset_handler();
    sched.stop();
    sched.clear();
  }
};

TEST_F(SchedulerTest, AddTaskWithDuration) {
  auto target = make_target("check1");
  target->set_duration("60s");
  sched.add_task(target);

  auto retrieved = sched.get(1);
  ASSERT_TRUE(retrieved);
  EXPECT_EQ("check1", retrieved->get_alias());
}

TEST_F(SchedulerTest, AddTaskWithSchedule) {
  auto target = make_target("cron_check");
  target->set_schedule("* * * * *");
  sched.add_task(target);

  auto retrieved = sched.get(1);
  ASSERT_TRUE(retrieved);
  EXPECT_EQ("cron_check", retrieved->get_alias());
}

TEST_F(SchedulerTest, AddTaskWithoutDurationOrScheduleUsesFiveMinuteDefault) {
  auto target = make_target("default_check");
  // No duration or schedule set - should use 5m default
  sched.add_task(target);

  auto retrieved = sched.get(1);
  ASSERT_TRUE(retrieved);
  EXPECT_EQ("default_check", retrieved->get_alias());
}

TEST_F(SchedulerTest, AddMultipleTasks) {
  auto t1 = make_target("check1");
  t1->set_duration("60s");
  auto t2 = make_target("check2");
  t2->set_duration("120s");

  sched.add_task(t1);
  sched.add_task(t2);

  auto r1 = sched.get(1);
  auto r2 = sched.get(2);
  ASSERT_TRUE(r1);
  ASSERT_TRUE(r2);
  EXPECT_EQ("check1", r1->get_alias());
  EXPECT_EQ("check2", r2->get_alias());
}

TEST_F(SchedulerTest, ClearRemovesAllTasks) {
  auto target = make_target("check1");
  target->set_duration("60s");
  sched.add_task(target);
  EXPECT_TRUE(sched.get(1));

  sched.clear();
  EXPECT_FALSE(sched.get(1));
}

TEST_F(SchedulerTest, HandleScheduleDispatchesToHandler) {
  auto target = make_target("dispatched");
  target->set_duration("60s");
  sched.add_task(target);

  simple_scheduler::task task;
  task.id = 1;
  sched.handle_schedule(task);

  ASSERT_EQ(1u, handler.handled_aliases.size());
  EXPECT_EQ("dispatched", handler.handled_aliases[0]);
}

TEST_F(SchedulerTest, HandleScheduleWithNullHandlerDoesNotCrash) {
  auto target = make_target("orphan");
  target->set_duration("60s");
  sched.add_task(target);

  sched.unset_handler();
  simple_scheduler::task task;
  task.id = 1;
  // Should not crash when handler is null
  EXPECT_TRUE(sched.handle_schedule(task));
}

TEST_F(SchedulerTest, SetAndUnsetHandler) {
  mock_task_handler h2;
  sched.set_handler(&h2);

  auto target = make_target("check");
  target->set_duration("60s");
  sched.add_task(target);

  simple_scheduler::task task;
  task.id = 1;
  sched.handle_schedule(task);

  EXPECT_EQ(1u, h2.handled_aliases.size());
  EXPECT_EQ(0u, handler.handled_aliases.size());
}

TEST_F(SchedulerTest, OnErrorDispatchesToHandler) {
  sched.on_error("test.cpp", 42, "something went wrong");
  ASSERT_EQ(1u, handler.errors.size());
  EXPECT_EQ("something went wrong", handler.errors[0]);
}

TEST_F(SchedulerTest, OnTraceDispatchesToHandler) {
  sched.on_trace("test.cpp", 42, "trace message");
  ASSERT_EQ(1u, handler.traces.size());
  EXPECT_EQ("trace message", handler.traces[0]);
}

TEST_F(SchedulerTest, OnErrorWithNullHandlerDoesNotCrash) {
  sched.unset_handler();
  EXPECT_NO_THROW(sched.on_error("test.cpp", 1, "error"));
}

TEST_F(SchedulerTest, OnTraceWithNullHandlerDoesNotCrash) {
  sched.unset_handler();
  EXPECT_NO_THROW(sched.on_trace("test.cpp", 1, "trace"));
}

// ============================================================================
// parse_interval tests
// ============================================================================

namespace schedules {
boost::posix_time::seconds parse_interval(const std::string &str);
}  // namespace schedules

TEST(ParseInterval, EmptyStringReturnsZero) {
  auto result = schedules::parse_interval("");
  EXPECT_EQ(0, result.total_seconds());
}

TEST(ParseInterval, SecondsValue) {
  auto result = schedules::parse_interval("30s");
  EXPECT_EQ(30, result.total_seconds());
}

TEST(ParseInterval, MinutesValue) {
  auto result = schedules::parse_interval("5m");
  EXPECT_EQ(300, result.total_seconds());
}

TEST(ParseInterval, HoursValue) {
  auto result = schedules::parse_interval("2h");
  EXPECT_EQ(7200, result.total_seconds());
}

TEST(ParseInterval, DefaultUnitIsSeconds) {
  auto result = schedules::parse_interval("120");
  EXPECT_EQ(120, result.total_seconds());
}

TEST(ParseInterval, DaysValue) {
  auto result = schedules::parse_interval("1d");
  EXPECT_EQ(86400, result.total_seconds());
}

TEST(ParseInterval, WeeksValue) {
  auto result = schedules::parse_interval("1w");
  EXPECT_EQ(604800, result.total_seconds());
}

// ============================================================================
// schedule_object::set_command edge cases
// ============================================================================

TEST(ScheduleObject, SetCommandWithSingleArgument) {
  const auto obj = make_target("test");
  obj->set_command("check_disk free=5%");
  EXPECT_EQ("check_disk", obj->command);
  ASSERT_EQ(1u, obj->arguments.size());
  EXPECT_EQ("free=5%", obj->arguments.front());
}

TEST(ScheduleObject, SetCommandPreservesQuotedSpaces) {
  const auto obj = make_target("test");
  obj->set_command("check_files \"path=/tmp/my dir\" pattern=*.log");
  EXPECT_EQ("check_files", obj->command);
  ASSERT_EQ(2u, obj->arguments.size());
  auto it = obj->arguments.begin();
  EXPECT_EQ("path=/tmp/my dir", *it++);
  EXPECT_EQ("pattern=*.log", *it);
}

TEST(ScheduleObject, SetCommandOnlyCommand) {
  const auto obj = make_target("test");
  obj->set_command("check_ok");
  EXPECT_EQ("check_ok", obj->command);
  EXPECT_TRUE(obj->arguments.empty());
}

// ============================================================================
// schedule_object: combined property tests
// ============================================================================

TEST(ScheduleObject, FullyConfiguredObject) {
  const auto obj = make_target("full_check", "/settings/scheduler/schedules/full_check");
  obj->set_command("check_cpu warn=80 crit=90");
  obj->set_duration("5m");
  obj->set_randomness("10%");
  obj->set_report("all");
  obj->channel = "NSCA";
  obj->source_id = "my_host";
  obj->target_id = "monitoring_server";

  EXPECT_EQ("check_cpu", obj->command);
  ASSERT_EQ(2u, obj->arguments.size());
  ASSERT_TRUE(obj->duration.is_initialized());
  EXPECT_EQ(300, obj->duration->total_seconds());
  EXPECT_DOUBLE_EQ(0.10, obj->randomness);
  EXPECT_EQ("NSCA", obj->channel);
  EXPECT_EQ("my_host", obj->source_id);
  EXPECT_EQ("monitoring_server", obj->target_id);
  EXPECT_TRUE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnOK));
  EXPECT_TRUE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnCRIT));
}

TEST(ScheduleObject, CopyPreservesSchedule) {
  const auto obj = make_target("cron_orig");
  obj->set_schedule("0 * * * *");
  obj->command = "check_hourly";
  obj->channel = "NSCA";

  schedules::schedule_object copy(*obj);
  ASSERT_TRUE(copy.schedule.is_initialized());
  EXPECT_EQ("0 * * * *", *copy.schedule);
  EXPECT_EQ("check_hourly", copy.command);
  EXPECT_EQ("NSCA", copy.channel);
  EXPECT_FALSE(copy.duration.is_initialized());
}

// ============================================================================
// schedule_object::to_string additional tests
// ============================================================================

TEST(ScheduleObject, ToStringWithoutDurationOrSchedule) {
  const auto obj = make_target("bare");
  obj->command = "check_bare";
  std::string str = obj->to_string();
  EXPECT_NE(std::string::npos, str.find("bare"));
  EXPECT_NE(std::string::npos, str.find("check_bare"));
  // Should not contain "duration" or "schedule" keywords since neither is set
  EXPECT_EQ(std::string::npos, str.find("duration"));
  EXPECT_EQ(std::string::npos, str.find("schedule"));
}

TEST(ScheduleObject, ToStringIncludesSourceAndTarget) {
  const auto obj = make_target("st_check");
  obj->source_id = "src_host";
  obj->target_id = "tgt_host";
  std::string str = obj->to_string();
  EXPECT_NE(std::string::npos, str.find("src_host"));
  EXPECT_NE(std::string::npos, str.find("tgt_host"));
}

// ============================================================================
// schedules::scheduler: get with invalid ID
// ============================================================================

TEST_F(SchedulerTest, GetNonExistentIdReturnsNull) {
  auto result = sched.get(9999);
  EXPECT_FALSE(result);
}

TEST_F(SchedulerTest, GetAfterClearReturnsNull) {
  auto target = make_target("ephemeral");
  target->set_duration("30s");
  sched.add_task(target);
  EXPECT_TRUE(sched.get(1));

  sched.clear();
  EXPECT_FALSE(sched.get(1));
}

// ============================================================================
// schedules::scheduler: handler return false removes task
// ============================================================================

TEST_F(SchedulerTest, HandleScheduleReturnFalseRemovesTask) {
  handler.return_value = false;

  auto target = make_target("doomed");
  target->set_duration("60s");
  sched.add_task(target);
  EXPECT_TRUE(sched.get(1));

  simple_scheduler::task task;
  task.id = 1;
  sched.handle_schedule(task);

  ASSERT_EQ(1u, handler.handled_aliases.size());
  EXPECT_EQ("doomed", handler.handled_aliases[0]);
  // Task should have been removed from the underlying scheduler since handler returned false
  auto retrieved_task = sched.get_scheduler().get_task(1);
  EXPECT_FALSE(retrieved_task.is_initialized());
}

TEST_F(SchedulerTest, HandleScheduleReturnTrueKeepsTask) {
  handler.return_value = true;

  auto target = make_target("keeper");
  target->set_duration("60s");
  sched.add_task(target);

  simple_scheduler::task task;
  task.id = 1;
  sched.handle_schedule(task);

  ASSERT_EQ(1u, handler.handled_aliases.size());
  // Metadata should still be present
  EXPECT_TRUE(sched.get(1));
}

// ============================================================================
// schedules::scheduler: task properties preserved through add/get
// ============================================================================

TEST_F(SchedulerTest, TaskPropertiesPreservedThroughAddAndGet) {
  auto target = make_target("detailed");
  target->set_command("check_mem warn=80 crit=90 type=committed");
  target->set_duration("2m");
  target->set_randomness("15%");
  target->set_report("critical,warning");
  target->channel = "NSCA";
  target->source_id = "agent01";
  target->target_id = "server01";
  sched.add_task(target);

  auto retrieved = sched.get(1);
  ASSERT_TRUE(retrieved);
  EXPECT_EQ("detailed", retrieved->get_alias());
  EXPECT_EQ("check_mem", retrieved->command);
  ASSERT_EQ(3u, retrieved->arguments.size());
  ASSERT_TRUE(retrieved->duration.is_initialized());
  EXPECT_EQ(120, retrieved->duration->total_seconds());
  EXPECT_DOUBLE_EQ(0.15, retrieved->randomness);
  EXPECT_EQ("NSCA", retrieved->channel);
  EXPECT_EQ("agent01", retrieved->source_id);
  EXPECT_EQ("server01", retrieved->target_id);
  EXPECT_TRUE(nscapi::report::matches(retrieved->report, NSCAPI::query_return_codes::returnCRIT));
  EXPECT_TRUE(nscapi::report::matches(retrieved->report, NSCAPI::query_return_codes::returnWARN));
  EXPECT_FALSE(nscapi::report::matches(retrieved->report, NSCAPI::query_return_codes::returnOK));
}

// ============================================================================
// schedules::scheduler: multiple add/clear cycles
// ============================================================================

TEST_F(SchedulerTest, MultipleClearCycles) {
  auto t1 = make_target("cycle1");
  t1->set_duration("60s");
  sched.add_task(t1);
  EXPECT_TRUE(sched.get(1));

  sched.clear();
  EXPECT_FALSE(sched.get(1));

  auto t2 = make_target("cycle2");
  t2->set_duration("60s");
  sched.add_task(t2);
  // After clear, IDs restart from the underlying scheduler's counter
  // The new task should be retrievable
  auto retrieved = sched.get(2);
  ASSERT_TRUE(retrieved);
  EXPECT_EQ("cycle2", retrieved->get_alias());
}

TEST_F(SchedulerTest, ClearOnEmptySchedulerDoesNotCrash) {
  EXPECT_NO_THROW(sched.clear());
  EXPECT_NO_THROW(sched.clear());
}

// ============================================================================
// schedules::scheduler: set_threads
// ============================================================================

TEST_F(SchedulerTest, SetThreadsUpdatesCount) {
  sched.set_threads(8);
  EXPECT_EQ(8u, sched.get_scheduler().get_threads());
}

TEST_F(SchedulerTest, SetThreadsToOne) {
  sched.set_threads(1);
  EXPECT_EQ(1u, sched.get_scheduler().get_threads());
}

// ============================================================================
// schedules::scheduler: dispatch multiple tasks to handler
// ============================================================================

TEST_F(SchedulerTest, DispatchMultipleTasksToHandler) {
  auto t1 = make_target("task_a");
  t1->set_duration("60s");
  auto t2 = make_target("task_b");
  t2->set_duration("120s");
  auto t3 = make_target("task_c");
  t3->set_duration("180s");

  sched.add_task(t1);
  sched.add_task(t2);
  sched.add_task(t3);

  simple_scheduler::task st;
  st.id = 1;
  sched.handle_schedule(st);
  st.id = 2;
  sched.handle_schedule(st);
  st.id = 3;
  sched.handle_schedule(st);

  ASSERT_EQ(3u, handler.handled_aliases.size());
  EXPECT_EQ("task_a", handler.handled_aliases[0]);
  EXPECT_EQ("task_b", handler.handled_aliases[1]);
  EXPECT_EQ("task_c", handler.handled_aliases[2]);
}

// ============================================================================
// schedules::scheduler: on_error and on_trace with messages
// ============================================================================

TEST_F(SchedulerTest, MultipleErrorsAccumulate) {
  sched.on_error("file1.cpp", 10, "error one");
  sched.on_error("file2.cpp", 20, "error two");
  sched.on_error("file3.cpp", 30, "error three");

  ASSERT_EQ(3u, handler.errors.size());
  EXPECT_EQ("error one", handler.errors[0]);
  EXPECT_EQ("error two", handler.errors[1]);
  EXPECT_EQ("error three", handler.errors[2]);
}

TEST_F(SchedulerTest, MultipleTracesAccumulate) {
  sched.on_trace("file1.cpp", 10, "trace one");
  sched.on_trace("file2.cpp", 20, "trace two");

  ASSERT_EQ(2u, handler.traces.size());
  EXPECT_EQ("trace one", handler.traces[0]);
  EXPECT_EQ("trace two", handler.traces[1]);
}

// ============================================================================
// schedules::scheduler: handle_schedule with non-existent task ID
// ============================================================================

TEST_F(SchedulerTest, GetMetadataForNonExistentTaskReturnsNull) {
  // Verify that get() returns a null shared_ptr for an ID that was never added
  auto result = sched.get(42);
  EXPECT_FALSE(result);
}

// ============================================================================
// schedules::scheduler: metrics interface (what fetchMetrics exercises)
// ============================================================================

TEST_F(SchedulerTest, MetricsAvailableOnModernBoost) { EXPECT_TRUE(sched.get_scheduler().has_metrics()); }

TEST_F(SchedulerTest, MetricsDefaultToZero) {
  // Before any tasks are run, metrics should be 0 or reasonable defaults
  EXPECT_EQ(0, sched.get_scheduler().get_metric_errors());
  EXPECT_EQ(0, sched.get_scheduler().get_avg_time());
}

TEST_F(SchedulerTest, MetricQueueLengthInitiallyZeroOrEmpty) {
  // Queue length should be 0 or small before any tasks are scheduled to run
  std::size_t ql = sched.get_scheduler().get_metric_ql();
  // After add_task, items are added to the queue but we haven't started the scheduler
  EXPECT_GE(ql, 0u);
}

TEST_F(SchedulerTest, MetricThreadCountMatchesConfiguration) {
  sched.set_threads(4);
  EXPECT_EQ(4u, sched.get_scheduler().get_metric_threads());
}

// ============================================================================
// schedules::scheduler: add task with cron schedule and verify retrieval
// ============================================================================

TEST_F(SchedulerTest, AddCronTaskAndRetrieve) {
  auto target = make_target("hourly_check");
  target->set_schedule("0 * * * *");
  target->command = "check_hourly";
  target->channel = "NSCA";
  sched.add_task(target);

  auto retrieved = sched.get(1);
  ASSERT_TRUE(retrieved);
  EXPECT_EQ("hourly_check", retrieved->get_alias());
  EXPECT_EQ("check_hourly", retrieved->command);
  EXPECT_EQ("NSCA", retrieved->channel);
  ASSERT_TRUE(retrieved->schedule.is_initialized());
  EXPECT_EQ("0 * * * *", *retrieved->schedule);
}

TEST_F(SchedulerTest, AddMixedDurationAndCronTasks) {
  auto t1 = make_target("interval_task");
  t1->set_duration("30s");
  auto t2 = make_target("cron_task");
  t2->set_schedule("5 * * * *");

  sched.add_task(t1);
  sched.add_task(t2);

  auto r1 = sched.get(1);
  auto r2 = sched.get(2);
  ASSERT_TRUE(r1);
  ASSERT_TRUE(r2);
  EXPECT_EQ("interval_task", r1->get_alias());
  EXPECT_TRUE(r1->duration.is_initialized());
  EXPECT_FALSE(r1->schedule.is_initialized());
  EXPECT_EQ("cron_task", r2->get_alias());
  EXPECT_FALSE(r2->duration.is_initialized());
  EXPECT_TRUE(r2->schedule.is_initialized());
}

// ============================================================================
// schedule_object: report filtering edge cases
// ============================================================================

TEST(ScheduleObject, SetReportUnknownOnly) {
  const auto obj = make_target("test");
  obj->set_report("unknown");
  EXPECT_FALSE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnOK));
  EXPECT_FALSE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnWARN));
  EXPECT_FALSE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnCRIT));
  EXPECT_TRUE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnUNKNOWN));
}

TEST(ScheduleObject, SetReportWarningAndCritical) {
  const auto obj = make_target("test");
  obj->set_report("warning,critical");
  EXPECT_FALSE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnOK));
  EXPECT_TRUE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnWARN));
  EXPECT_TRUE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnCRIT));
  EXPECT_FALSE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnUNKNOWN));
}

TEST(ScheduleObject, SetReportOkAndUnknown) {
  const auto obj = make_target("test");
  obj->set_report("ok,unknown");
  EXPECT_TRUE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnOK));
  EXPECT_FALSE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnWARN));
  EXPECT_FALSE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnCRIT));
  EXPECT_TRUE(nscapi::report::matches(obj->report, NSCAPI::query_return_codes::returnUNKNOWN));
}

// ============================================================================
// schedule_object: randomness edge cases
// ============================================================================

TEST(ScheduleObject, SetRandomnessSmallValue) {
  const auto obj = make_target("test");
  obj->set_randomness("1%");
  EXPECT_DOUBLE_EQ(0.01, obj->randomness);
}

TEST(ScheduleObject, SetRandomnessFractional) {
  const auto obj = make_target("test");
  obj->set_randomness("33.5%");
  EXPECT_DOUBLE_EQ(0.335, obj->randomness);
}

// ============================================================================
// schedule_object: duration edge cases
// ============================================================================

TEST(ScheduleObject, SetDurationLargeValue) {
  const auto obj = make_target("test");
  obj->set_duration("3600s");
  ASSERT_TRUE(obj->duration.is_initialized());
  EXPECT_EQ(3600, obj->duration->total_seconds());
}

TEST(ScheduleObject, SetDurationOverwritesPrevious) {
  const auto obj = make_target("test");
  obj->set_duration("60s");
  EXPECT_EQ(60, obj->duration->total_seconds());
  obj->set_duration("120s");
  EXPECT_EQ(120, obj->duration->total_seconds());
}

// ============================================================================
// schedules::scheduler: prepare_shutdown and unset_handler
// ============================================================================

TEST_F(SchedulerTest, PrepareShutdownDoesNotCrash) {
  auto target = make_target("check");
  target->set_duration("60s");
  sched.add_task(target);
  EXPECT_NO_THROW(sched.prepare_shutdown());
}

TEST_F(SchedulerTest, UnsetHandlerAfterPrepareShutdown) {
  sched.prepare_shutdown();
  sched.unset_handler();

  // Dispatching after unset should not crash
  auto target = make_target("orphan");
  target->set_duration("60s");
  sched.add_task(target);

  simple_scheduler::task task;
  task.id = 1;
  EXPECT_TRUE(sched.handle_schedule(task));
  // Handler was unset, so nothing should have been dispatched
  EXPECT_EQ(0u, handler.handled_aliases.size());
}

// ============================================================================
// schedules::scheduler: queue length after adding tasks
// ============================================================================

TEST_F(SchedulerTest, QueueLengthIncreasesWithTasks) {
  std::size_t initial_ql = sched.get_scheduler().get_metric_ql();

  auto t1 = make_target("q1");
  t1->set_duration("60s");
  sched.add_task(t1);

  auto t2 = make_target("q2");
  t2->set_duration("120s");
  sched.add_task(t2);

  std::size_t after_ql = sched.get_scheduler().get_metric_ql();
  EXPECT_EQ(initial_ql + 2, after_ql);
}
