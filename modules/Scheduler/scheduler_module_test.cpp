// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the Scheduler module class: the settings it registers, which
// configured schedules it accepts, and run_schedules - which runs schedules
// on demand, synchronously, on the calling thread, so the whole path (query
// the command through the core, filter on the report mode, submit on the
// channel) can be driven through the stub core.
//
// Everything loads with NSCAPI::dontStart: the schedules are parsed and handed
// to the scheduler, but its worker threads are never started, so nothing runs
// on a timer. schedules_handler_test.cpp covers the schedule objects on their
// own.

#include "Scheduler.h"

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>
#include <utility>
#include <vector>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

const char *const kRoot = "/settings/scheduler";
const char *const kDefault = "/settings/scheduler/schedules/default";

class SchedulerModule : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }

  void SetUp() override {
    core().reset();
    module_.set_id(42);
    // Every schedule inherits an interval from the default one unless a test
    // says otherwise; without one a schedule is rejected.
    core().set_setting(kDefault, "interval", "1h");
  }
  void TearDown() override {
    module_.unloadModule();
    core().reset();
  }

  bool load() { return module_.loadModuleEx("", NSCAPI::dontStart); }

  // `alias = command` one-liners under [/settings/scheduler/schedules].
  void schedules(const std::vector<std::pair<std::string, std::string> > &entries) { core().set_keys("schedules", entries); }

  PB::Commands::QueryResponseMessage::Response run(const std::vector<std::string> &args, const PB::Commands::QueryRequestMessage &request_message = {}) {
    PB::Commands::QueryRequestMessage::Request request;
    request.set_command("run_schedules");
    for (const std::string &a : args) request.add_arguments(a);
    PB::Commands::QueryResponseMessage::Response response;
    module_.run_schedules(request, &response, request_message);
    return response;
  }

  static std::string message_of(const PB::Commands::QueryResponseMessage::Response &r) { return r.lines_size() > 0 ? r.lines(0).message() : std::string(); }

  nscapi::test_helpers::stdout_capture out_;
  Scheduler module_;
};

}  // namespace

// ============================================================================
// The settings contract
// ============================================================================

TEST_F(SchedulerModule, LoadRegistersItsKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for(kRoot, "threads"), "5");
  EXPECT_EQ(core().default_for(kRoot, "startup window"), "0s");
  EXPECT_EQ(core().default_for(kRoot, "timezone"), "local");
}

// The default schedule is the template every other one inherits from; its
// channel and report mode are the ones a one-liner schedule ends up with.
TEST_F(SchedulerModule, TheDefaultScheduleIsPublished) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for(kDefault, "channel"), "NSCA");
  EXPECT_EQ(core().default_for(kDefault, "report"), "all");
  EXPECT_EQ(core().default_for(kDefault, "run on startup"), "false");
  EXPECT_TRUE(core().has_key(kDefault, "interval"));
  EXPECT_TRUE(core().has_key(kDefault, "schedule"));
}

TEST_F(SchedulerModule, TheSchedulesSectionIsRegistered) {
  ASSERT_TRUE(load());

  const std::vector<std::string> &paths = core().registered_paths();
  EXPECT_NE(std::find(paths.begin(), paths.end(), "/settings/scheduler/schedules"), paths.end());
}

TEST_F(SchedulerModule, AnInvalidStartupWindowFallsBackToZero) {
  core().set_setting(kRoot, "startup window", "soon");

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_logged("Invalid 'startup window' value 'soon', falling back to 0s"));
}

TEST_F(SchedulerModule, AValidStartupWindowIsAccepted) {
  core().set_setting(kRoot, "startup window", "30s");

  ASSERT_TRUE(load());
  EXPECT_FALSE(core().has_logged("startup window"));
  EXPECT_TRUE(module_.startModule()) << "no startup schedules: nothing to queue";
}

// ============================================================================
// Which schedules are accepted
// ============================================================================

TEST_F(SchedulerModule, NoSchedulesMeansRunSchedulesHasNothingToRun) {
  ASSERT_TRUE(load());

  const PB::Commands::QueryResponseMessage::Response r = run({});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(message_of(r), "No schedules configured");
}

TEST_F(SchedulerModule, AScheduleWithAZeroIntervalIsRejected) {
  core().set_setting(kDefault, "interval", "0s");
  schedules({{"cpu", "check_cpu"}});

  ASSERT_TRUE(load());
  EXPECT_TRUE(out_.contains("WE cant add schedules with 0 duration: cpu"));
  EXPECT_EQ(message_of(run({})), "No schedules configured");
}

TEST_F(SchedulerModule, AScheduleWithBothAnIntervalAndACronIsRejected) {
  core().set_setting(kDefault, "schedule", "0 * * * *");
  schedules({{"cpu", "check_cpu"}});

  ASSERT_TRUE(load());
  EXPECT_TRUE(out_.contains("WE cant add schedules with both duration and schedule: cpu"));
  EXPECT_EQ(message_of(run({})), "No schedules configured");
}

TEST_F(SchedulerModule, AScheduleWithNeitherIntervalNorCronIsRejected) {
  core().set_setting(kDefault, "interval", "");
  schedules({{"cpu", "check_cpu"}});

  ASSERT_TRUE(load());
  EXPECT_TRUE(out_.contains("WE need wither duration or schedule: cpu"));
}

TEST_F(SchedulerModule, ACronScheduleIsAccepted) {
  core().set_setting(kDefault, "interval", "");
  core().set_setting(kDefault, "schedule", "0 * * * *");
  schedules({{"cpu", "check_cpu"}});
  core().set_query_result("check_cpu", PB::Common::ResultCode::OK, "fine");

  ASSERT_TRUE(load());
  EXPECT_EQ(message_of(run({})), "Ran 1 schedule(s): cpu");
}

// ============================================================================
// run_schedules
// ============================================================================

TEST_F(SchedulerModule, RunSchedulesRunsTheCommandAndSubmitsTheResult) {
  schedules({{"cpu", "check_cpu warn=load>80"}});
  core().set_query_result("check_cpu", PB::Common::ResultCode::WARNING, "CPU high");

  ASSERT_TRUE(load());
  const PB::Commands::QueryResponseMessage::Response r = run({});

  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK) << message_of(r);
  EXPECT_EQ(message_of(r), "Ran 1 schedule(s): cpu");

  ASSERT_EQ(core().queries().size(), 1u);
  const PB::Commands::QueryRequestMessage::Request &q = core().queries()[0].payload(0);
  EXPECT_EQ(q.command(), "check_cpu");
  ASSERT_EQ(q.arguments_size(), 1);
  EXPECT_EQ(q.arguments(0), "warn=load>80");

  ASSERT_EQ(core().submissions().size(), 1u);
  const PB::Commands::SubmitRequestMessage &s = core().submissions()[0];
  EXPECT_EQ(s.channel(), "NSCA");
  ASSERT_EQ(s.payload_size(), 1);
  EXPECT_EQ(s.payload(0).alias(), "cpu");
  EXPECT_EQ(s.payload(0).result(), PB::Common::ResultCode::WARNING);
}

TEST_F(SchedulerModule, RunSchedulesRunsEverySchedule) {
  schedules({{"mem", "check_memory"}, {"cpu", "check_cpu"}});
  core().set_query_result("check_cpu", PB::Common::ResultCode::OK, "fine");
  core().set_query_result("check_memory", PB::Common::ResultCode::OK, "fine");

  ASSERT_TRUE(load());
  EXPECT_EQ(message_of(run({})), "Ran 2 schedule(s): cpu, mem") << "sorted by alias";
  EXPECT_EQ(core().submissions().size(), 2u);
}

TEST_F(SchedulerModule, RunSchedulesCanPickOneSchedule) {
  schedules({{"mem", "check_memory"}, {"cpu", "check_cpu"}});
  core().set_query_result("check_cpu", PB::Common::ResultCode::OK, "fine");
  core().set_query_result("check_memory", PB::Common::ResultCode::OK, "fine");

  ASSERT_TRUE(load());
  EXPECT_EQ(message_of(run({"schedule=mem"})), "Ran 1 schedule(s): mem");
  ASSERT_EQ(core().queries().size(), 1u);
  EXPECT_EQ(core().queries()[0].payload(0).command(), "check_memory");
}

TEST_F(SchedulerModule, AnUnknownScheduleNamesTheAvailableOnes) {
  schedules({{"mem", "check_memory"}, {"cpu", "check_cpu"}});

  ASSERT_TRUE(load());
  const PB::Commands::QueryResponseMessage::Response r = run({"schedule=disk"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(message_of(r), "No such schedule: disk (available: cpu, mem)");
  EXPECT_TRUE(core().queries().empty());
}

TEST_F(SchedulerModule, AnUnknownOptionIsASyntaxError) {
  schedules({{"cpu", "check_cpu"}});

  ASSERT_TRUE(load());
  const PB::Commands::QueryResponseMessage::Response r = run({"bogus=1"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(message_of(r).find("Invalid command line"), std::string::npos) << message_of(r);
}

// A command the core does not know is reported, and an UNKNOWN is submitted
// in its place so the server sees the schedule is broken.
TEST_F(SchedulerModule, AMissingCommandSubmitsUnknown) {
  schedules({{"cpu", "check_nothing"}});

  ASSERT_TRUE(load());
  const PB::Commands::QueryResponseMessage::Response r = run({});

  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(message_of(r), "Failed to run: cpu (Command was not found: check_nothing)");
  ASSERT_EQ(core().submissions().size(), 1u);
  EXPECT_EQ(core().submissions()[0].channel(), "NSCA");
  ASSERT_EQ(core().submissions()[0].payload_size(), 1);
  EXPECT_EQ(core().submissions()[0].payload(0).result(), PB::Common::ResultCode::UNKNOWN);
}

TEST_F(SchedulerModule, OneFailingScheduleDoesNotHideTheOthers) {
  schedules({{"bad", "check_nothing"}, {"cpu", "check_cpu"}});
  core().set_query_result("check_cpu", PB::Common::ResultCode::OK, "fine");

  ASSERT_TRUE(load());
  EXPECT_EQ(message_of(run({})), "Failed to run: bad (Command was not found: check_nothing), ran: cpu");
}

// The report mode filters what reaches the channel: a result it does not
// match is a successful run with nothing submitted.
TEST_F(SchedulerModule, TheReportModeFiltersWhatIsSubmitted) {
  core().set_setting(kDefault, "report", "critical");
  schedules({{"cpu", "check_cpu"}});
  core().set_query_result("check_cpu", PB::Common::ResultCode::OK, "fine");

  ASSERT_TRUE(load());
  EXPECT_EQ(message_of(run({})), "Ran 1 schedule(s): cpu");
  EXPECT_EQ(core().queries().size(), 1u);
  EXPECT_TRUE(core().submissions().empty());
}

TEST_F(SchedulerModule, TheDropChannelDiscardsTheResult) {
  core().set_setting(kDefault, "channel", "drop");
  schedules({{"cpu", "check_cpu"}});
  core().set_query_result("check_cpu", PB::Common::ResultCode::CRITICAL, "bad");

  ASSERT_TRUE(load());
  EXPECT_EQ(message_of(run({})), "Ran 1 schedule(s): cpu");
  EXPECT_TRUE(core().submissions().empty());
}

TEST_F(SchedulerModule, AScheduleWithoutAChannelFails) {
  core().set_setting(kDefault, "channel", "");
  schedules({{"cpu", "check_cpu"}});
  core().set_query_result("check_cpu", PB::Common::ResultCode::OK, "fine");

  ASSERT_TRUE(load());
  EXPECT_EQ(message_of(run({})), "Failed to run: cpu (No channel specified for cpu)");
  EXPECT_TRUE(core().submissions().empty());
}

// A permission denial is not the check's result: it must never reach the
// channel, where it would overwrite the last real result.
TEST_F(SchedulerModule, ADeniedQueryIsNotSubmitted) {
  schedules({{"cpu", "check_cpu"}});
  PB::Commands::QueryResponseMessage denied;
  PB::Common::KeyValue *kv = denied.mutable_header()->add_metadata();
  kv->set_key("nscp.query_denied");
  kv->set_value("true");
  PB::Commands::QueryResponseMessage::Response *p = denied.add_payload();
  p->set_command("check_cpu");
  p->set_result(PB::Common::ResultCode::UNKNOWN);
  p->add_lines()->set_message("Permission denied");
  core().set_query_response("check_cpu", denied);

  ASSERT_TRUE(load());
  EXPECT_EQ(message_of(run({})), "Failed to run: cpu (Permission denied: not allowed to run check_cpu)");
  EXPECT_TRUE(core().submissions().empty());
}

// An on-demand run is checked against whoever asked for it: the caller
// identity on the request is forwarded on the query.
TEST_F(SchedulerModule, TheCallerIdentityIsForwarded) {
  schedules({{"cpu", "check_cpu"}});
  core().set_query_result("check_cpu", PB::Common::ResultCode::OK, "fine");
  PB::Commands::QueryRequestMessage request_message;
  PB::Common::KeyValue *kv = request_message.mutable_header()->add_metadata();
  kv->set_key("nscp.caller_plugin_id");
  kv->set_value("7");
  kv = request_message.mutable_header()->add_metadata();
  kv->set_key("nscp.principal");
  kv->set_value("alice");

  ASSERT_TRUE(load());
  run({}, request_message);

  ASSERT_EQ(core().queries().size(), 1u);
  std::string caller, principal;
  for (const PB::Common::KeyValue &m : core().queries()[0].header().metadata()) {
    if (m.key() == "nscp.caller_plugin_id") caller = m.value();
    if (m.key() == "nscp.principal") principal = m.value();
  }
  EXPECT_EQ(caller, "7");
  EXPECT_EQ(principal, "alice");
}

// A schedule that runs run_schedules would recurse through the core; it is
// refused up front.
TEST_F(SchedulerModule, AScheduleRunningRunSchedulesIsRefused) {
  schedules({{"loop", "run_schedules"}});

  ASSERT_TRUE(load());
  EXPECT_EQ(message_of(run({})), "Failed to run: loop (schedule runs run_schedules, refusing to recurse)");
  EXPECT_TRUE(core().queries().empty());
}

// The indirect case: a check that calls run_schedules back on the same
// thread bounces off the reentrancy guard instead of recursing.
TEST_F(SchedulerModule, ANestedRunSchedulesIsRefused) {
  schedules({{"cpu", "check_cpu"}});
  std::string nested;
  core().set_query_handler([this, &nested](const PB::Commands::QueryRequestMessage &, PB::Commands::QueryResponseMessage &reply) {
    nested = message_of(run({}));
    PB::Commands::QueryResponseMessage::Response *p = reply.add_payload();
    p->set_command("check_cpu");
    p->set_result(PB::Common::ResultCode::OK);
    p->add_lines()->set_message("fine");
    return true;
  });

  ASSERT_TRUE(load());
  EXPECT_EQ(message_of(run({})), "Ran 1 schedule(s): cpu");
  EXPECT_EQ(nested, "run_schedules invoked from within run_schedules (a schedule runs run_schedules?); refusing to recurse");
}

// ============================================================================
// Metrics and lifecycle
// ============================================================================

TEST_F(SchedulerModule, MetricsArePublishedUnderTheSchedulerBundle) {
  ASSERT_TRUE(load());

  PB::Metrics::MetricsMessage::Response response;
  module_.fetchMetrics(&response);
  ASSERT_EQ(response.bundles_size(), 1);
  EXPECT_EQ(response.bundles(0).key(), "scheduler");
  EXPECT_GT(response.bundles(0).value_size(), 0);
}

TEST_F(SchedulerModule, PrepareShutdownAndUnloadWithoutStartingAreSafe) {
  schedules({{"cpu", "check_cpu"}});
  ASSERT_TRUE(load());

  module_.prepareShutdown();
  EXPECT_TRUE(module_.unloadModule());
  EXPECT_TRUE(module_.unloadModule());
}
