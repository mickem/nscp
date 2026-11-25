// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the CheckHelpers module class: the alias table loadModuleEx
// reads from [/settings/check helpers/alias], what it registers with the core
// and what a reload does to both - and the helper commands themselves, which
// wrap other commands run through the core. The stub core answers those
// queries and records the submissions check_and_forward makes.

#include "CheckHelpers.h"

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

typedef PB::Commands::QueryRequestMessage::Request request_type;
typedef PB::Commands::QueryResponseMessage::Response response_type;

request_type make_request(const std::string &command, const std::vector<std::string> &args) {
  request_type request;
  request.set_command(command);
  for (const std::string &a : args) request.add_arguments(a);
  return request;
}

std::string message_of(const response_type &r) { return r.lines_size() > 0 ? r.lines(0).message() : std::string(); }
// The helpers that wrap a command answer into the response the command
// already filled, so an error they add after it is the last line.
std::string last_message(const response_type &r) { return r.lines_size() > 0 ? r.lines(r.lines_size() - 1).message() : std::string(); }
std::string all_lines(const response_type &r) {
  std::string ret;
  for (const auto &l : r.lines()) ret += "[" + l.message() + "]";
  return ret;
}

// A query answer with one line carrying the given perfdata (alias, value,
// unit, maximum).
PB::Commands::QueryResponseMessage answer_with_perf(const std::string &command, const PB::Common::ResultCode code,
                                                    const std::vector<std::pair<std::string, double> > &values, const std::string &unit = "") {
  PB::Commands::QueryResponseMessage reply;
  response_type *p = reply.add_payload();
  p->set_command(command);
  p->set_result(code);
  PB::Commands::QueryResponseMessage::Response::Line *line = p->add_lines();
  line->set_message(command + " says hello");
  for (const auto &v : values) {
    PB::Common::PerformanceData *perf = line->add_perf();
    perf->set_alias(v.first);
    perf->mutable_float_value()->set_value(v.second);
    perf->mutable_float_value()->set_unit(unit);
    perf->mutable_float_value()->mutable_maximum()->set_value(v.second * 2);
    perf->mutable_float_value()->mutable_minimum()->set_value(1);
  }
  return reply;
}

class CheckHelpersModule : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }

  void SetUp() override {
    core().reset();
    module_.set_id(42);
  }
  void TearDown() override { core().reset(); }

  void configure_aliases(const std::vector<std::pair<std::string, std::string> > &aliases) { core().set_keys("alias", aliases); }

  bool load() { return module_.loadModuleEx("", NSCAPI::dontStart); }

  // Dispatch a command name through the alias fallback and return the message.
  // An alias that is found is forwarded to the core; the stub fails a command
  // it was given no answer for, so that reads "Failed to execute", distinct
  // from the "No alias found" answer an unknown name gets.
  std::string resolve(const std::string &command) {
    PB::Commands::QueryRequestMessage request_message;
    PB::Commands::QueryRequestMessage::Request request;
    request.set_command(command);
    PB::Commands::QueryResponseMessage::Response response;
    module_.query_fallback(request, &response, request_message);
    return response.lines_size() > 0 ? response.lines(0).message() : "";
  }

  typedef void (CheckHelpers::*simple_fn)(const request_type &, response_type *);
  typedef void (CheckHelpers::*wrapping_fn)(const request_type &, response_type *, const PB::Commands::QueryRequestMessage &);

  response_type run(simple_fn fn, const std::string &command, const std::vector<std::string> &args = {}) {
    response_type response;
    (module_.*fn)(make_request(command, args), &response);
    return response;
  }
  response_type run(wrapping_fn fn, const std::string &command, const std::vector<std::string> &args = {},
                    const PB::Commands::QueryRequestMessage &request_message = {}) {
    response_type response;
    (module_.*fn)(make_request(command, args), &response, request_message);
    return response;
  }

  CheckHelpers module_;
};

}  // namespace

TEST_F(CheckHelpersModule, LoadRegistersEveryAliasWithTheCore) {
  configure_aliases({{"my_ok", "check_ok message=fine"}, {"my_warn", "check_warning"}});
  ASSERT_TRUE(load());

  EXPECT_TRUE(core().has_command("my_ok"));
  EXPECT_TRUE(core().has_command("my_warn"));
}

TEST_F(CheckHelpersModule, AnUnknownCommandIsReportedAsNoAlias) {
  configure_aliases({{"my_ok", "check_ok message=fine"}});
  ASSERT_TRUE(load());

  EXPECT_EQ(resolve("no_such_alias"), "No alias found matching: no_such_alias");
}

// A reload runs loadModuleEx again on the live module. The alias table used to
// be added to in place, so an alias removed from the ini kept resolving until
// the service was restarted.
TEST_F(CheckHelpersModule, ReloadDropsAnAliasRemovedFromTheConfiguration) {
  configure_aliases({{"my_ok", "check_ok message=fine"}, {"my_gone", "check_ok message=gone"}});
  ASSERT_TRUE(load());
  ASSERT_NE(resolve("my_gone"), "No alias found matching: my_gone");

  configure_aliases({{"my_ok", "check_ok message=fine"}});
  ASSERT_TRUE(load());

  EXPECT_EQ(resolve("my_gone"), "No alias found matching: my_gone");
  EXPECT_NE(resolve("my_ok"), "No alias found matching: my_ok");
  // Retracted from the core as well, so the name no longer routes here.
  EXPECT_FALSE(core().has_command("my_gone"));
  EXPECT_TRUE(core().has_command("my_ok"));
}

TEST_F(CheckHelpersModule, ReloadPicksUpAnAliasAddedToTheConfiguration) {
  configure_aliases({{"my_ok", "check_ok message=fine"}});
  ASSERT_TRUE(load());
  ASSERT_EQ(resolve("my_new"), "No alias found matching: my_new");

  configure_aliases({{"my_ok", "check_ok message=fine"}, {"my_new", "check_ok message=new"}});
  ASSERT_TRUE(load());

  EXPECT_NE(resolve("my_new"), "No alias found matching: my_new");
  EXPECT_TRUE(core().has_command("my_new"));
}

TEST_F(CheckHelpersModule, AnAliasRunsItsCommandWithTheArgumentsSubstituted) {
  configure_aliases({{"my_cpu", "check_cpu \"warn=load > $ARG1$\" \"crit=load > %ARG2%\""}});
  core().set_query_result("check_cpu", PB::Common::ResultCode::WARNING, "CPU high");
  ASSERT_TRUE(load());

  PB::Commands::QueryRequestMessage request_message;
  response_type response;
  module_.query_fallback(make_request("my_cpu", {"80", "90"}), &response, request_message);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::WARNING);
  EXPECT_EQ(message_of(response), "CPU high");
  ASSERT_EQ(core().queries().size(), 1u);
  const request_type &q = core().queries()[0].payload(0);
  EXPECT_EQ(q.command(), "check_cpu");
  ASSERT_EQ(q.arguments_size(), 2);
  EXPECT_EQ(q.arguments(0), "warn=load > 80");
  EXPECT_EQ(q.arguments(1), "crit=load > 90");
}

// The caller may only fill the placeholders: an argument the template does
// not mention is never passed on to the command.
TEST_F(CheckHelpersModule, AnAliasDoesNotPassOnExtraArguments) {
  configure_aliases({{"my_cpu", "check_cpu time=5m"}});
  core().set_query_result("check_cpu", PB::Common::ResultCode::OK, "fine");
  ASSERT_TRUE(load());

  PB::Commands::QueryRequestMessage request_message;
  response_type response;
  module_.query_fallback(make_request("my_cpu", {"crit=load > 0"}), &response, request_message);

  ASSERT_EQ(core().queries().size(), 1u);
  ASSERT_EQ(core().queries()[0].payload(0).arguments_size(), 1);
  EXPECT_EQ(core().queries()[0].payload(0).arguments(0), "time=5m");
}

TEST_F(CheckHelpersModule, AnAliasWhoseCommandFailsIsReported) {
  configure_aliases({{"my_cpu", "check_cpu"}});
  ASSERT_TRUE(load());

  EXPECT_EQ(resolve("my_cpu"), "Failed to execute: my_cpu");
}

TEST_F(CheckHelpersModule, AnAliasWhoseCommandAnswersTwiceIsRejected) {
  configure_aliases({{"my_cpu", "check_cpu"}});
  PB::Commands::QueryResponseMessage two;
  two.add_payload()->set_command("check_cpu");
  two.add_payload()->set_command("check_cpu");
  core().set_query_response("check_cpu", two);
  ASSERT_TRUE(load());

  EXPECT_EQ(resolve("my_cpu"), "Invalid response from command: my_cpu");
}

// ============================================================================
// check_ok / check_warning / check_critical / check_version
// ============================================================================

TEST_F(CheckHelpersModule, FixedStatusChecksReturnTheirStatusAndMessage) {
  ASSERT_TRUE(load());

  response_type r = run(&CheckHelpers::check_ok, "check_ok", {"message=all good"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK);
  EXPECT_EQ(message_of(r), "all good");
  r = run(&CheckHelpers::check_warning, "check_warning");
  EXPECT_EQ(r.result(), PB::Common::ResultCode::WARNING);
  EXPECT_EQ(message_of(r), "No message");
  r = run(&CheckHelpers::check_critical, "check_critical", {"message=bad"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::CRITICAL);
  EXPECT_EQ(message_of(r), "bad");
}

TEST_F(CheckHelpersModule, FixedStatusChecksRejectAnUnknownOption) {
  ASSERT_TRUE(load());

  const response_type r = run(&CheckHelpers::check_ok, "check_ok", {"bogus=1"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(message_of(r).find("bogus"), std::string::npos) << message_of(r);
}

TEST_F(CheckHelpersModule, CheckVersionReportsTheCoreVersion) {
  core().set_application_version("9.8.7");
  ASSERT_TRUE(load());

  const response_type r = run(&CheckHelpers::check_version, "check_version");
  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK);
  EXPECT_EQ(message_of(r), "9.8.7");
}

// ============================================================================
// check_always_* and check_negate
// ============================================================================

TEST_F(CheckHelpersModule, CheckAlwaysOverridesTheStatusButKeepsTheMessage) {
  core().set_query_result("check_cpu", PB::Common::ResultCode::CRITICAL, "CPU on fire");
  ASSERT_TRUE(load());

  response_type r = run(&CheckHelpers::check_always_ok, "check_always_ok", {"check_cpu", "time=5m"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK);
  EXPECT_EQ(message_of(r), "CPU on fire");
  ASSERT_EQ(core().queries().size(), 1u);
  EXPECT_EQ(core().queries()[0].payload(0).command(), "check_cpu");

  r = run(&CheckHelpers::check_always_warning, "check_always_warning", {"check_cpu"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::WARNING);
  r = run(&CheckHelpers::check_always_critical, "check_always_critical", {"check_cpu"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::CRITICAL);
}

TEST_F(CheckHelpersModule, CheckAlwaysNeedsACommand) {
  ASSERT_TRUE(load());
  EXPECT_EQ(message_of(run(&CheckHelpers::check_always_ok, "check_always_ok")), "Needs at least one command");
}

// A command that cannot be run is UNKNOWN whatever status was asked for.
TEST_F(CheckHelpersModule, CheckAlwaysOfAFailingCommandIsUnknown) {
  ASSERT_TRUE(load());

  const response_type r = run(&CheckHelpers::check_always_ok, "check_always_ok", {"check_nothing"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(message_of(r), "Failed to execute: check_nothing");
}

TEST_F(CheckHelpersModule, CheckNegateMapsEachStatus) {
  core().set_query_result("check_cpu", PB::Common::ResultCode::CRITICAL, "CPU on fire");
  ASSERT_TRUE(load());

  response_type r = run(&CheckHelpers::check_negate, "check_negate", {"command=check_cpu", "critical=ok"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK);
  EXPECT_EQ(message_of(r), "CPU on fire");
  r = run(&CheckHelpers::check_negate, "check_negate", {"command=check_cpu", "ok=critical"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::CRITICAL) << "an unmapped status is kept";
}

TEST_F(CheckHelpersModule, CheckNegateNeedsACommand) {
  ASSERT_TRUE(load());

  const response_type r = run(&CheckHelpers::check_negate, "check_negate", {"ok=critical"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(message_of(r).find("Missing command"), std::string::npos) << message_of(r);
}

TEST_F(CheckHelpersModule, CheckNegatePassesTheArgumentsOn) {
  core().set_query_result("check_cpu", PB::Common::ResultCode::OK, "fine");
  ASSERT_TRUE(load());

  run(&CheckHelpers::check_negate, "check_negate", {"command=check_cpu", "arguments=time=5m", "arguments=warn=load>80"});
  ASSERT_EQ(core().queries().size(), 1u);
  const request_type &q = core().queries()[0].payload(0);
  ASSERT_EQ(q.arguments_size(), 2);
  EXPECT_EQ(q.arguments(0), "time=5m");
  EXPECT_EQ(q.arguments(1), "warn=load>80");
}

// ============================================================================
// check_multi
// ============================================================================

TEST_F(CheckHelpersModule, CheckMultiJoinsTheMessagesAndKeepsTheWorstStatus) {
  core().set_query_result("check_cpu", PB::Common::ResultCode::OK, "cpu fine");
  core().set_query_result("check_memory", PB::Common::ResultCode::WARNING, "memory high");
  core().set_query_result("check_disk", PB::Common::ResultCode::CRITICAL, "disk full");
  ASSERT_TRUE(load());

  const response_type r = run(&CheckHelpers::check_multi, "check_multi",
                              {"command=check_cpu time=5m", "command=check_memory", "command=check_disk", "prefix=[", "suffix=]", "separator= | "});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::CRITICAL);
  ASSERT_EQ(r.lines_size(), 3);
  EXPECT_EQ(r.lines(0).message(), "[cpu fine");
  EXPECT_EQ(r.lines(1).message(), " | memory high");
  EXPECT_EQ(r.lines(2).message(), " | disk full]");
  ASSERT_EQ(core().queries().size(), 3u);
  EXPECT_EQ(core().queries()[0].payload(0).arguments(0), "time=5m");
}

TEST_F(CheckHelpersModule, CheckMultiWarningDoesNotHideAnUnknown) {
  core().set_query_result("check_cpu", PB::Common::ResultCode::UNKNOWN, "cpu?");
  core().set_query_result("check_memory", PB::Common::ResultCode::WARNING, "memory high");
  ASSERT_TRUE(load());

  EXPECT_EQ(run(&CheckHelpers::check_multi, "check_multi", {"command=check_cpu", "command=check_memory"}).result(), PB::Common::ResultCode::UNKNOWN);
}

TEST_F(CheckHelpersModule, CheckMultiNeedsACommand) {
  ASSERT_TRUE(load());
  EXPECT_NE(message_of(run(&CheckHelpers::check_multi, "check_multi")).find("Missing command"), std::string::npos);
}

TEST_F(CheckHelpersModule, CheckMultiStopsAtAFailingCommand) {
  core().set_query_result("check_cpu", PB::Common::ResultCode::OK, "cpu fine");
  ASSERT_TRUE(load());

  const response_type r = run(&CheckHelpers::check_multi, "check_multi", {"command=check_cpu", "command=check_nothing"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(message_of(r), "cpu fine") << "what ran before is kept";
  EXPECT_EQ(last_message(r), "Failed to execute command: check_nothing");
}

// One request must not fan out into unbounded work.
TEST_F(CheckHelpersModule, CheckMultiCapsTheNumberOfCommands) {
  ASSERT_TRUE(load());

  std::vector<std::string> args(129, "command=check_ok");
  const response_type r = run(&CheckHelpers::check_multi, "check_multi", args);
  EXPECT_EQ(message_of(r), "Too many commands (129): check_multi runs at most 128 commands per call");
  EXPECT_TRUE(core().queries().empty());
}

// ============================================================================
// check_and_forward
// ============================================================================

TEST_F(CheckHelpersModule, CheckAndForwardSubmitsTheResult) {
  core().set_query_result("check_cpu", PB::Common::ResultCode::WARNING, "CPU high");
  ASSERT_TRUE(load());

  const response_type r = run(&CheckHelpers::check_and_forward, "check_and_forward", {"command=check_cpu", "channel=GRAPHITE", "arguments=time=5m"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK) << message_of(r);
  EXPECT_EQ(message_of(r), "Message submitted: GRAPHITE");
  ASSERT_EQ(core().submissions().size(), 1u);
  const PB::Commands::SubmitRequestMessage &s = core().submissions()[0];
  EXPECT_EQ(s.channel(), "GRAPHITE");
  ASSERT_EQ(s.payload_size(), 1);
  EXPECT_EQ(s.payload(0).alias(), "check_cpu") << "named after the command by default";
  EXPECT_EQ(s.payload(0).result(), PB::Common::ResultCode::WARNING);
}

TEST_F(CheckHelpersModule, CheckAndForwardDefaultsToNscaAndHonoursTheLegacyTarget) {
  core().set_query_result("check_cpu", PB::Common::ResultCode::OK, "fine");
  ASSERT_TRUE(load());

  run(&CheckHelpers::check_and_forward, "check_and_forward", {"command=check_cpu", "alias=cpu"});
  run(&CheckHelpers::check_and_forward, "check_and_forward", {"command=check_cpu", "target=NRDP"});
  ASSERT_EQ(core().submissions().size(), 2u);
  EXPECT_EQ(core().submissions()[0].channel(), "NSCA");
  EXPECT_EQ(core().submissions()[0].payload(0).alias(), "cpu");
  EXPECT_EQ(core().submissions()[1].channel(), "NRDP");
}

TEST_F(CheckHelpersModule, CheckAndForwardNeedsACommand) {
  ASSERT_TRUE(load());
  EXPECT_NE(message_of(run(&CheckHelpers::check_and_forward, "check_and_forward", {"channel=NSCA"})).find("Missing command"), std::string::npos);
}

TEST_F(CheckHelpersModule, CheckAndForwardOfAFailingCommandSubmitsNothing) {
  ASSERT_TRUE(load());

  EXPECT_EQ(message_of(run(&CheckHelpers::check_and_forward, "check_and_forward", {"command=check_nothing"})), "Failed to execute: check_nothing");
  EXPECT_TRUE(core().submissions().empty());
}

// A permission denial is not the check's result and must not be forwarded.
TEST_F(CheckHelpersModule, CheckAndForwardNeverForwardsADenial) {
  PB::Commands::QueryResponseMessage denied;
  PB::Common::KeyValue *kv = denied.mutable_header()->add_metadata();
  kv->set_key("nscp.query_denied");
  kv->set_value("true");
  denied.add_payload()->set_result(PB::Common::ResultCode::UNKNOWN);
  core().set_query_response("check_cpu", denied);
  ASSERT_TRUE(load());

  EXPECT_EQ(message_of(run(&CheckHelpers::check_and_forward, "check_and_forward", {"command=check_cpu"})),
            "Permission denied: not allowed to run check_cpu, nothing was submitted");
  EXPECT_TRUE(core().submissions().empty());
}

// The wrapped command runs on behalf of whoever called the helper.
TEST_F(CheckHelpersModule, TheCallerIdentityIsForwarded) {
  core().set_query_result("check_cpu", PB::Common::ResultCode::OK, "fine");
  ASSERT_TRUE(load());
  PB::Commands::QueryRequestMessage request_message;
  PB::Common::KeyValue *kv = request_message.mutable_header()->add_metadata();
  kv->set_key("nscp.caller_plugin_id");
  kv->set_value("7");
  kv = request_message.mutable_header()->add_metadata();
  kv->set_key("nscp.principal");
  kv->set_value("alice");

  run(&CheckHelpers::check_and_forward, "check_and_forward", {"command=check_cpu"}, request_message);
  ASSERT_EQ(core().queries().size(), 1u);
  std::string principal;
  for (const PB::Common::KeyValue &m : core().queries()[0].header().metadata()) {
    if (m.key() == "nscp.principal") principal = m.value();
  }
  EXPECT_EQ(principal, "alice");
}

// ============================================================================
// check_timeout
// ============================================================================

// The wrapped command runs on a worker thread; one that answers in time is
// returned as it is, with the status overridden on request.
TEST_F(CheckHelpersModule, CheckTimeoutReturnsACommandThatAnswersInTime) {
  core().set_query_result("check_cpu", PB::Common::ResultCode::WARNING, "CPU high");
  ASSERT_TRUE(load());

  response_type r = run(&CheckHelpers::check_timeout, "check_timeout", {"command=check_cpu", "timeout=30"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::WARNING);
  EXPECT_EQ(message_of(r), "CPU high");
  r = run(&CheckHelpers::check_timeout, "check_timeout", {"command=check_cpu", "return=ok"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK);
}

TEST_F(CheckHelpersModule, CheckTimeoutReportsAFailingCommand) {
  ASSERT_TRUE(load());

  EXPECT_EQ(message_of(run(&CheckHelpers::check_timeout, "check_timeout", {"command=check_nothing"})), "Failed to execute: check_nothing");
  EXPECT_NE(message_of(run(&CheckHelpers::check_timeout, "check_timeout", {})).find("Missing command"), std::string::npos);
}

// ============================================================================
// filter_perf / render_perf / xform_perf
// ============================================================================

TEST_F(CheckHelpersModule, FilterPerfSortsAndLimitsThePerfdata) {
  core().set_query_response("check_proc", answer_with_perf("check_proc", PB::Common::ResultCode::OK, {{"a", 1}, {"b", 30}, {"c", 20}}));
  ASSERT_TRUE(load());

  const response_type r = run(&CheckHelpers::filter_perf, "filter_perf", {"command=check_proc", "sort=normal", "limit=2"});
  ASSERT_EQ(r.lines_size(), 1);
  ASSERT_EQ(r.lines(0).perf_size(), 2);
  EXPECT_EQ(r.lines(0).perf(0).alias(), "b");
  EXPECT_EQ(r.lines(0).perf(1).alias(), "c");
  EXPECT_EQ(r.lines(0).message(), "check_proc says hello\n");
}

TEST_F(CheckHelpersModule, FilterPerfNeedsACommand) {
  ASSERT_TRUE(load());
  EXPECT_NE(message_of(run(&CheckHelpers::filter_perf, "filter_perf", {"sort=normal"})).find("Missing command"), std::string::npos);
}

TEST_F(CheckHelpersModule, RenderPerfTurnsPerfdataIntoRecords) {
  core().set_query_response("check_proc", answer_with_perf("check_proc", PB::Common::ResultCode::OK, {{"a", 1}, {"b", 30}}));
  ASSERT_TRUE(load());

  const response_type r =
      run(&CheckHelpers::render_perf, "render_perf", {"command=check_proc", "remove-perf", "top-syntax=%(list)", "detail-syntax=%(key)=%(value)"});
  // The command's own line stays (its perfdata stripped by remove-perf) and
  // the rendering follows it.
  ASSERT_EQ(r.lines_size(), 2) << all_lines(r);
  EXPECT_EQ(r.lines(0).message(), "check_proc says hello");
  EXPECT_EQ(r.lines(0).perf_size(), 0);
  EXPECT_EQ(r.lines(1).message(), "a=1, b=30");
}

TEST_F(CheckHelpersModule, XformPerfExtractsAField) {
  core().set_query_response("check_proc", answer_with_perf("check_proc", PB::Common::ResultCode::OK, {{"used", 10}}));
  ASSERT_TRUE(load());

  const response_type r = run(&CheckHelpers::xform_perf, "xform_perf", {"command=check_proc", "mode=extract", "field=max", "replace=used=max"});
  ASSERT_EQ(r.lines_size(), 1);
  bool found = false;
  for (const PB::Common::PerformanceData &p : r.lines(0).perf()) {
    if (p.alias() != "max") continue;
    found = true;
    EXPECT_EQ(p.float_value().value(), 20);
  }
  EXPECT_TRUE(found);
}

TEST_F(CheckHelpersModule, XformPerfMinmaxFillsInPercentBounds) {
  PB::Commands::QueryResponseMessage reply = answer_with_perf("check_proc", PB::Common::ResultCode::OK, {{"load", 40}}, "%");
  reply.mutable_payload(0)->mutable_lines(0)->mutable_perf(0)->mutable_float_value()->clear_maximum();
  core().set_query_response("check_proc", reply);
  ASSERT_TRUE(load());

  const response_type r = run(&CheckHelpers::xform_perf, "xform_perf", {"command=check_proc", "mode=minmax"});
  ASSERT_EQ(r.lines(0).perf_size(), 1);
  EXPECT_EQ(r.lines(0).perf(0).float_value().maximum().value(), 100);
  EXPECT_EQ(r.lines(0).perf(0).float_value().minimum().value(), 0);
}

TEST_F(CheckHelpersModule, XformPerfRejectsAnUnknownModeOrABadReplace) {
  core().set_query_response("check_proc", answer_with_perf("check_proc", PB::Common::ResultCode::OK, {{"used", 10}}));
  ASSERT_TRUE(load());

  EXPECT_NE(last_message(run(&CheckHelpers::xform_perf, "xform_perf", {"command=check_proc", "mode=shuffle"})).find("Invalid mode specified"),
            std::string::npos);
  EXPECT_NE(
      last_message(run(&CheckHelpers::xform_perf, "xform_perf", {"command=check_proc", "mode=extract", "replace=nope"})).find("Invalid syntax replace string"),
      std::string::npos);
  EXPECT_NE(message_of(run(&CheckHelpers::xform_perf, "xform_perf", {"mode=minmax"})).find("Missing command"), std::string::npos);
}

TEST_F(CheckHelpersModule, UnloadIsSafe) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(module_.unloadModule());
}
