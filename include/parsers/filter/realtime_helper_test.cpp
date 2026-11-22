// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <list>
#include <map>
#include <memory>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/settings/filter.hpp>
#include <nscapi/test_helpers.hpp>
#include <parsers/filter/realtime_helper.hpp>
#include <string>

// Provide the NSCAPI singleton so linked code can resolve the symbol.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

// ============================================================================
// Minimal stub types to instantiate realtime_filter_helper<>
// ============================================================================

struct stub_filter_type {
  // Minimal stand-in for the filter on a real container. The earlier
  // timer/accessor tests do not exercise process_item so they do not need
  // start_match/match_post/summary, but the process_item tests below do —
  // those members are added here as no-op call counters so a test can assert
  // they were hit.
  bool build_syntax(bool, const std::string &, const std::string &, const std::string &, const std::string &, const std::string &, const std::string &) {
    return true;
  }
  bool build_engines(bool, const char *, const std::string &, const std::string &, const std::string &) { return true; }
  bool validate(std::string &) { return true; }

  struct summary_stub {
    int returnCode = 0;
  };
  summary_stub summary;

  int start_match_calls = 0;
  int match_post_calls = 0;
  int fetch_hash_calls = 0;

  void start_match() { ++start_match_calls; }
  void match_post() { ++match_post_calls; }
  void fetch_hash(bool) { ++fetch_hash_calls; }

  // Records list and message accessor present only because process_item
  // touches them in the matched/event branches; the tests in this file stay
  // on the empty-result path so these do not need to be functional.
  std::list<std::map<std::string, std::string>> records_;
  std::string message;
  // A failure the top syntax raises when it is rendered (a template function).
  std::string render_failure;
  std::string get_message() {
    if (!render_failure.empty()) evaluation_failure = render_failure;
    return message;
  }

  // An evaluation that failed on a record (a refused regular expression, a
  // unit the keyword does not take), as modern_filters reports it.
  std::string evaluation_failure;
  bool has_evaluation_errors() const { return !evaluation_failure.empty(); }
  std::string get_evaluation_failure() const { return "Filter evaluation failed: " + evaluation_failure; }
  std::string with_evaluation_failure(const std::string &m) const { return m + " (" + get_evaluation_failure() + ")"; }
};

struct stub_runtime_data {
  typedef stub_filter_type filter_type;
  typedef int transient_data_type;

  bool booted = false;
  boost::posix_time::ptime last_touch;
  // Configurable result so individual tests can drive process_item down the
  // matched / unmatched branch they want.
  modern_filter::match_result next_result{true, false};

  void boot() { booted = true; }
  void touch(boost::posix_time::ptime now) { last_touch = now; }
  bool has_changed(transient_data_type) const { return true; }
  modern_filter::match_result process_item(filter_type &, transient_data_type) { return next_result; }
};

struct stub_config_object {
  std::string alias_;
  nscapi::settings_filters::filter_object filter;

  stub_config_object(const std::string &alias) : alias_(alias), filter("${list}", "${detail}", "NSCA") {}

  std::string get_alias() const { return alias_; }
};

// Convenient type aliases for the template instantiation under test.
using helper_type = parsers::where::realtime_filter_helper<stub_runtime_data, stub_config_object>;
using container_type = helper_type::container;

// Helper: create a fixed ptime for deterministic tests.
static boost::posix_time::ptime make_time(int hour, int min, int sec) {
  return boost::posix_time::ptime(boost::gregorian::date(2026, 3, 9), boost::posix_time::time_duration(hour, min, sec));
}

// ============================================================================
// container — construction & accessor tests
// ============================================================================

TEST(RealtimeContainer, DefaultConstruction) {
  stub_runtime_data rd;
  container_type c("my_alias", "my_event", rd);

  EXPECT_EQ(c.get_alias(), "my_alias");
  EXPECT_EQ(c.get_event_name(), "my_event");
  EXPECT_EQ(c.command, "my_alias");
  EXPECT_EQ(c.severity, 0);
  EXPECT_FALSE(c.debug);
  EXPECT_FALSE(c.escape_html);
}

TEST(RealtimeContainer, SetAndGetTarget) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);

  c.set_target("my_target", "tid", "sid");
  EXPECT_EQ(c.get_target(), "my_target");
  EXPECT_EQ(c.get_target_id(), "tid");
  EXPECT_EQ(c.get_source_id(), "sid");
}

TEST(RealtimeContainer, IsEventAndIsEvents) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);

  c.set_target("event", "", "");
  EXPECT_TRUE(c.is_event());
  EXPECT_FALSE(c.is_events());

  c.set_target("events", "", "");
  EXPECT_FALSE(c.is_event());
  EXPECT_TRUE(c.is_events());

  c.set_target("other", "", "");
  EXPECT_FALSE(c.is_event());
  EXPECT_FALSE(c.is_events());
}

// ============================================================================
// container — is_silent() tests
// ============================================================================

TEST(RealtimeContainer, IsSilentWithoutSilentPeriod) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  // No silent_period set — should never be silent.
  EXPECT_FALSE(c.is_silent(make_time(12, 0, 0)));
}

TEST(RealtimeContainer, IsSilentBeforeAlertTime) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.silent_period = boost::posix_time::seconds(60);

  // Touch with alert=true at 12:00:00 — next_alert_ = 12:01:00
  c.touch(make_time(12, 0, 0), true);

  // Before 12:01:00 — should be silent.
  EXPECT_TRUE(c.is_silent(make_time(12, 0, 30)));
  // At exactly 12:01:00 — not silent (now < next_alert_ is false).
  EXPECT_FALSE(c.is_silent(make_time(12, 1, 0)));
  // After 12:01:00 — not silent.
  EXPECT_FALSE(c.is_silent(make_time(12, 1, 1)));
}

TEST(RealtimeContainer, IsSilentAfterNonAlertTouch) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.silent_period = boost::posix_time::seconds(60);

  // Touch with alert=false at 12:00:00 — next_alert_ = 12:00:00 (now)
  c.touch(make_time(12, 0, 0), false);

  // At 12:00:00 or later — not silent.
  EXPECT_FALSE(c.is_silent(make_time(12, 0, 0)));
  EXPECT_FALSE(c.is_silent(make_time(12, 0, 1)));
}

// ============================================================================
// container — touch() tests
// ============================================================================

TEST(RealtimeContainer, TouchSetsNextOkWithMaxAge) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.max_age = boost::posix_time::seconds(120);

  c.touch(make_time(10, 0, 0), false);

  // After touch, next_ok_ = 10:00:00 + 120s = 10:02:00
  // Verify via has_timedout:
  EXPECT_FALSE(c.has_timedout(make_time(10, 1, 59)));
  EXPECT_TRUE(c.has_timedout(make_time(10, 2, 0)));
}

TEST(RealtimeContainer, TouchWithAlertSetsSilentPeriod) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.silent_period = boost::posix_time::seconds(30);

  c.touch(make_time(10, 0, 0), true);

  // next_alert_ = 10:00:30
  EXPECT_TRUE(c.is_silent(make_time(10, 0, 15)));
  EXPECT_FALSE(c.is_silent(make_time(10, 0, 30)));
}

TEST(RealtimeContainer, TouchWithoutAlertResetsSilentToNow) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.silent_period = boost::posix_time::seconds(30);

  // First, trigger an alert to set next_alert_ into the future.
  c.touch(make_time(10, 0, 0), true);
  EXPECT_TRUE(c.is_silent(make_time(10, 0, 15)));

  // Now touch without alert — next_alert_ resets to now (10:00:20).
  c.touch(make_time(10, 0, 20), false);
  EXPECT_FALSE(c.is_silent(make_time(10, 0, 20)));
  EXPECT_FALSE(c.is_silent(make_time(10, 0, 25)));
}

TEST(RealtimeContainer, TouchDelegatesDataTouch) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);

  auto t = make_time(14, 30, 0);
  c.touch(t, false);

  EXPECT_EQ(c.data.last_touch, t);
}

TEST(RealtimeContainer, TouchWithoutMaxAgeDoesNotSetTimeout) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  // max_age not set

  c.touch(make_time(10, 0, 0), false);

  // has_timedout should always be false without max_age
  EXPECT_FALSE(c.has_timedout(make_time(10, 0, 0)));
  EXPECT_FALSE(c.has_timedout(make_time(23, 59, 59)));
}

TEST(RealtimeContainer, TouchWithoutSilentPeriodDoesNotAffectSilence) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  // silent_period not set

  c.touch(make_time(10, 0, 0), true);
  EXPECT_FALSE(c.is_silent(make_time(10, 0, 0)));
}

// ============================================================================
// container — has_timedout() tests
// ============================================================================

TEST(RealtimeContainer, HasTimedoutWithoutMaxAge) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  // No max_age — never times out.
  EXPECT_FALSE(c.has_timedout(make_time(0, 0, 0)));
  EXPECT_FALSE(c.has_timedout(make_time(23, 59, 59)));
}

TEST(RealtimeContainer, HasTimedoutBeforeExpiry) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.max_age = boost::posix_time::seconds(60);

  c.touch(make_time(12, 0, 0), false);

  // next_ok_ = 12:01:00
  EXPECT_FALSE(c.has_timedout(make_time(12, 0, 30)));
  EXPECT_FALSE(c.has_timedout(make_time(12, 0, 59)));
}

TEST(RealtimeContainer, HasTimedoutAtExpiry) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.max_age = boost::posix_time::seconds(60);

  c.touch(make_time(12, 0, 0), false);

  // Exactly at expiry: next_ok_ <= now
  EXPECT_TRUE(c.has_timedout(make_time(12, 1, 0)));
}

TEST(RealtimeContainer, HasTimedoutAfterExpiry) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.max_age = boost::posix_time::seconds(60);

  c.touch(make_time(12, 0, 0), false);

  EXPECT_TRUE(c.has_timedout(make_time(12, 5, 0)));
}

TEST(RealtimeContainer, HasTimedoutResetsAfterRetouch) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.max_age = boost::posix_time::seconds(60);

  c.touch(make_time(12, 0, 0), false);
  EXPECT_TRUE(c.has_timedout(make_time(12, 1, 0)));

  // Re-touch at 12:01:00 — next_ok_ = 12:02:00
  c.touch(make_time(12, 1, 0), false);
  EXPECT_FALSE(c.has_timedout(make_time(12, 1, 30)));
  EXPECT_TRUE(c.has_timedout(make_time(12, 2, 0)));
}

// ============================================================================
// container — find_minimum_timeout() tests
// ============================================================================

TEST(RealtimeContainer, FindMinimumTimeoutWithoutMaxAge) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  // No max_age

  boost::optional<boost::posix_time::ptime> minNext;
  EXPECT_FALSE(c.find_minimum_timeout(minNext));
  EXPECT_FALSE(minNext.is_initialized());
}

TEST(RealtimeContainer, FindMinimumTimeoutSetsFirstValue) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.max_age = boost::posix_time::seconds(60);
  c.touch(make_time(10, 0, 0), false);

  boost::optional<boost::posix_time::ptime> minNext;
  EXPECT_TRUE(c.find_minimum_timeout(minNext));
  ASSERT_TRUE(minNext.is_initialized());
  EXPECT_EQ(minNext.value(), make_time(10, 1, 0));
}

TEST(RealtimeContainer, FindMinimumTimeoutKeepsSmallerExisting) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.max_age = boost::posix_time::seconds(120);
  c.touch(make_time(10, 0, 0), false);

  // Pre-set minNext to an earlier time — container's next_ok_ (10:02:00)
  // is later, so should not update and return false.
  boost::optional<boost::posix_time::ptime> minNext = make_time(10, 0, 30);
  EXPECT_FALSE(c.find_minimum_timeout(minNext));
  EXPECT_EQ(minNext.value(), make_time(10, 0, 30));
}

TEST(RealtimeContainer, FindMinimumTimeoutUpdatesLargerExisting) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.max_age = boost::posix_time::seconds(30);
  c.touch(make_time(10, 0, 0), false);

  // Pre-set minNext to a later time — container's next_ok_ (10:00:30)
  // is earlier, so should update.
  boost::optional<boost::posix_time::ptime> minNext = make_time(10, 5, 0);
  EXPECT_TRUE(c.find_minimum_timeout(minNext));
  EXPECT_EQ(minNext.value(), make_time(10, 0, 30));
}

TEST(RealtimeContainer, FindMinimumTimeoutUpdatesEqualExisting) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.max_age = boost::posix_time::seconds(60);
  c.touch(make_time(10, 0, 0), false);

  // Pre-set minNext to equal value — should still update (returns true).
  boost::optional<boost::posix_time::ptime> minNext = make_time(10, 1, 0);
  EXPECT_TRUE(c.find_minimum_timeout(minNext));
  EXPECT_EQ(minNext.value(), make_time(10, 1, 0));
}

// ============================================================================
// container — combined timer scenarios
// ============================================================================

TEST(RealtimeContainer, SilentPeriodAndMaxAgeIndependent) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.max_age = boost::posix_time::seconds(120);
  c.silent_period = boost::posix_time::seconds(30);

  c.touch(make_time(10, 0, 0), true);

  // At 10:00:15: silent but not timed out
  EXPECT_TRUE(c.is_silent(make_time(10, 0, 15)));
  EXPECT_FALSE(c.has_timedout(make_time(10, 0, 15)));

  // At 10:00:45: not silent, not timed out
  EXPECT_FALSE(c.is_silent(make_time(10, 0, 45)));
  EXPECT_FALSE(c.has_timedout(make_time(10, 0, 45)));

  // At 10:02:00: not silent, timed out
  EXPECT_FALSE(c.is_silent(make_time(10, 2, 0)));
  EXPECT_TRUE(c.has_timedout(make_time(10, 2, 0)));
}

TEST(RealtimeContainer, MultipleAlertTouchesExtendSilentPeriod) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.silent_period = boost::posix_time::seconds(30);

  c.touch(make_time(10, 0, 0), true);
  // next_alert_ = 10:00:30

  // Touch again with alert at 10:00:20 — next_alert_ = 10:00:50
  c.touch(make_time(10, 0, 20), true);
  EXPECT_TRUE(c.is_silent(make_time(10, 0, 40)));
  EXPECT_FALSE(c.is_silent(make_time(10, 0, 50)));
}

// ============================================================================
// realtime_filter_helper — items list management
// ============================================================================

TEST(RealtimeFilterHelper, InitiallyEmpty) {
  helper_type helper(nullptr, 0);
  EXPECT_TRUE(helper.items.empty());
}

// ============================================================================
// process_item — match_post is invoked even when no rows matched
// ============================================================================
//
// modern_filter::match() now defers per-row warn/crit evaluation until
// match_post() runs, and modern_filter::match_post() is also where the no-rows
// force-evaluate path runs for mixed expressions. The realtime helper
// previously skipped match_post() entirely; without it the deferred verdict
// would never materialise. These tests pin the contract that the helper
// always calls start_match / match_post around the per-tick iteration,
// regardless of whether the iteration produced any matched rows.

TEST(RealtimeFilterHelperProcessItem, CallsMatchPostOnEmptyResult) {
  helper_type helper(nullptr, 0);
  stub_runtime_data rd;
  rd.next_result = modern_filter::match_result(false, false);  // no rows matched
  // process_item takes the container by `std::shared_ptr` (helper_type's
  // own container_type alias), so build it as std::shared_ptr explicitly
  // rather than via std::make_shared.
  std::shared_ptr<container_type> item(new container_type("alias", "event", rd));

  // Default target ("") is neither "event" nor "events" — process_item takes
  // the submission branch, but the early `if (!result.matched_filter) return
  // false` exits before any nscapi::core_helper call (which would dereference
  // the null core pointer).
  const bool ok = helper.process_item(item, /*data=*/0, /*is_silent=*/false);

  EXPECT_FALSE(ok);
  EXPECT_EQ(item->filter.start_match_calls, 1);
  EXPECT_EQ(item->filter.match_post_calls, 1);
  // No event target → fetch_hash should never be called.
  EXPECT_EQ(item->filter.fetch_hash_calls, 0);
}

TEST(RealtimeFilterHelperProcessItem, MatchPostRunsBeforeEarlyReturn) {
  // Regression sentinel: if a future refactor moves match_post() below the
  // early `return false` branch, this test would catch it because the call
  // counter would still be 0 after the unmatched tick.
  helper_type helper(nullptr, 0);
  stub_runtime_data rd;
  rd.next_result = modern_filter::match_result(false, false);
  std::shared_ptr<container_type> item(new container_type("a", "e", rd));

  helper.process_item(item, 0, false);
  helper.process_item(item, 0, false);

  EXPECT_EQ(item->filter.match_post_calls, 2);
  EXPECT_EQ(item->filter.start_match_calls, 2);
}

TEST(RealtimeFilterHelperProcessItem, SeverityIsAppliedAfterStartMatch) {
  // The helper sets summary.returnCode = severity *after* start_match()
  // resets it. Confirm both happen in that order by checking returnCode
  // ends up at the configured severity even though our stub start_match
  // doesn't touch it.
  helper_type helper(nullptr, 0);
  stub_runtime_data rd;
  rd.next_result = modern_filter::match_result(false, false);
  std::shared_ptr<container_type> item(new container_type("a", "e", rd));
  item->severity = 2;  // CRITICAL

  helper.process_item(item, 0, false);

  EXPECT_EQ(item->filter.summary.returnCode, 2);
}

// ============================================================================
// process_item - a record whose evaluation failed is reported, not dropped
// ============================================================================
//
// A refused regular expression is a non-match as far as the engine's verdict
// goes, so the batch used to be dropped as "nothing matched" and the line the
// filter exists to alert on never raised anything. It is now submitted as
// UNKNOWN naming the failure, unless the filter is silenced or feeds an event
// channel (which carries no status).

namespace {
nscapi::test_helpers::stub_core &stub() { return nscapi::test_helpers::stub_core::instance(); }

std::shared_ptr<container_type> failing_item(stub_runtime_data &rd, const std::string &target) {
  rd.next_result = modern_filter::match_result(false, false);  // the refused record did not match
  std::shared_ptr<container_type> item(new container_type("logwatch", "logwatch", rd));
  item->set_target(target, "", "");
  item->filter.evaluation_failure = "Refusing to match a regular expression against 2000000 bytes (in: line regexp 'x')";
  return item;
}
}  // namespace

TEST(RealtimeFilterHelperProcessItem, AnEvaluationFailureIsSubmittedAsUnknown) {
  stub().reset();
  helper_type helper(nscapi::plugin_singleton->get_core(), 0);
  stub_runtime_data rd;
  const auto item = failing_item(rd, "NSCA");

  // Submitted, but no alert: the return value is what starts the silent
  // period, and a failure on its own must not.
  EXPECT_FALSE(helper.process_item(item, 0, /*is_silent=*/false));
  EXPECT_EQ(1, item->error_count.load());
  ASSERT_EQ(1u, stub().submissions().size());
  const PB::Commands::SubmitRequestMessage &sent = stub().submissions().front();
  EXPECT_EQ("NSCA", sent.channel());
  ASSERT_EQ(1, sent.payload_size());
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, sent.payload(0).result());
  EXPECT_EQ("logwatch", sent.payload(0).command());
  EXPECT_EQ("Filter evaluation failed: Refusing to match a regular expression against 2000000 bytes (in: line regexp 'x')", sent.payload(0).lines(0).message());
}

TEST(RealtimeFilterHelperProcessItem, ASilencedFilterOnlyCountsTheFailure) {
  stub().reset();
  helper_type helper(nscapi::plugin_singleton->get_core(), 0);
  stub_runtime_data rd;
  const auto item = failing_item(rd, "NSCA");

  EXPECT_FALSE(helper.process_item(item, 0, /*is_silent=*/true));
  EXPECT_EQ(1, item->error_count.load());
  EXPECT_TRUE(stub().submissions().empty());
}

TEST(RealtimeFilterHelperProcessItem, AnEventFilterHasNoStatusToCarryTheFailure) {
  // Nothing matched, so there is nothing to emit: the failure is logged and
  // counted on the filter.
  stub().reset();
  helper_type helper(nscapi::plugin_singleton->get_core(), 0);
  stub_runtime_data rd;
  const auto item = failing_item(rd, "event");

  EXPECT_FALSE(helper.process_item(item, 0, false));
  EXPECT_EQ(1, item->error_count.load());
  EXPECT_TRUE(stub().submissions().empty());
  EXPECT_TRUE(stub().events().empty());
}

namespace {
// A batch where one record failed (the oversized line) and another matched.
std::shared_ptr<container_type> mixed_item(stub_runtime_data &rd, const std::string &target) {
  const auto item = failing_item(rd, target);
  item->data.next_result = modern_filter::match_result(true, false);
  item->filter.records_.push_back({{"line", "ERROR: disk on fire"}});
  item->filter.message = "logwatch: ERROR: disk on fire";
  return item;
}
}  // namespace

TEST(RealtimeFilterHelperProcessItem, AnEventFilterStillEmitsWhatMatchedBesideAFailure) {
  // A log batch is every line written since the last read: a line over the
  // regular-expression subject limit, written in the same burst as a matching
  // one, used to drop the whole batch - and with it the event for the line the
  // filter exists to catch.
  for (const std::string target : {"event", "events"}) {
    stub().reset();
    helper_type helper(nscapi::plugin_singleton->get_core(), 0);
    stub_runtime_data rd;
    const auto item = mixed_item(rd, target);

    EXPECT_TRUE(helper.process_item(item, 0, false)) << target;
    EXPECT_EQ(1, item->error_count.load()) << target;
    EXPECT_TRUE(stub().submissions().empty()) << target;
    ASSERT_EQ(1u, stub().events().size()) << target;
    ASSERT_EQ(1, stub().events().front().payload_size()) << target;
    const auto &data = stub().events().front().payload(0).data();
    bool found = false;
    for (const auto &kv : data) found = found || (kv.key() == "line" && kv.value() == "ERROR: disk on fire");
    EXPECT_TRUE(found) << target << ": " << stub().events().front().DebugString();
  }
}

TEST(RealtimeFilterHelperProcessItem, AStatusFilterKeepsTheVerdictTheMatchedRecordsReached) {
  stub().reset();
  helper_type helper(nscapi::plugin_singleton->get_core(), 0);
  stub_runtime_data rd;
  const auto item = mixed_item(rd, "NSCA");
  item->severity = NSCAPI::query_return_codes::returnCRIT;

  EXPECT_TRUE(helper.process_item(item, 0, false));
  ASSERT_EQ(1u, stub().submissions().size());
  const auto &payload = stub().submissions().front().payload(0);
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, payload.result());
  EXPECT_EQ("logwatch: ERROR: disk on fire (Filter evaluation failed: Refusing to match a regular expression against 2000000 bytes (in: line regexp 'x'))",
            payload.lines(0).message());
}

TEST(RealtimeFilterHelperProcessItem, AFailureAloneDoesNotStartTheSilentPeriod) {
  // An oversized line followed shortly by a matching one: the failure must not
  // silence the filter, or the matching line is suppressed.
  stub().reset();
  helper_type helper(nscapi::plugin_singleton->get_core(), 0);
  stub_runtime_data rd;
  const auto item = failing_item(rd, "NSCA");
  item->silent_period = boost::posix_time::seconds(60);
  // A container is silent until its first touch, as the module's init does.
  item->touch(make_time(11, 59, 0), false);
  const boost::posix_time::ptime t0 = make_time(12, 0, 0);

  if (helper.process_item(item, 0, item->is_silent(t0), t0)) item->touch(t0, true);
  EXPECT_FALSE(item->is_silent(make_time(12, 0, 5)));

  item->filter.evaluation_failure.clear();
  item->data.next_result = modern_filter::match_result(true, false);
  item->filter.message = "logwatch: ERROR: disk on fire";
  item->severity = NSCAPI::query_return_codes::returnCRIT;
  const boost::posix_time::ptime t1 = make_time(12, 0, 5);
  EXPECT_TRUE(helper.process_item(item, 0, item->is_silent(t1), t1));
  ASSERT_EQ(2u, stub().submissions().size());
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, stub().submissions().back().payload(0).result());
}

TEST(RealtimeFilterHelperProcessItem, APersistentFailureIsReportedOncePerInterval) {
  // A filter that fails on every batch - every second for the CheckSystem
  // filters - is counted every time, but reported when it is new and then
  // once per interval while it lasts.
  stub().reset();
  helper_type helper(nscapi::plugin_singleton->get_core(), 0);
  stub_runtime_data rd;
  const auto item = failing_item(rd, "NSCA");

  for (int s = 0; s < 10; ++s) helper.process_item(item, 0, false, make_time(12, 0, s));
  EXPECT_EQ(10, item->error_count.load());
  EXPECT_EQ(1u, stub().submissions().size());

  // The interval has passed: reported again.
  helper.process_item(item, 0, false, make_time(12, 5, 0));
  EXPECT_EQ(2u, stub().submissions().size());

  // A different failure is new, and reported at once.
  item->filter.evaluation_failure = "Invalid unit: x (in: load > 5x)";
  helper.process_item(item, 0, false, make_time(12, 5, 1));
  EXPECT_EQ(3u, stub().submissions().size());

  // A clean batch ends it: the same failure after it is new again.
  item->filter.evaluation_failure.clear();
  helper.process_item(item, 0, false, make_time(12, 5, 2));
  item->filter.evaluation_failure = "Invalid unit: x (in: load > 5x)";
  helper.process_item(item, 0, false, make_time(12, 5, 3));
  EXPECT_EQ(4u, stub().submissions().size());
}

TEST(RealtimeFilterHelperProcessItem, AThrottledFailureNeverDropsRecordsThatMatched) {
  // A filter with no thresholds and no severity: what matched reaches OK at
  // most, which cannot stand beside a failure, so the batch is UNKNOWN. A
  // repeat of the failure is throttled - but the batch is still submitted,
  // with what matched, or those lines would be lost until the interval ends.
  stub().reset();
  helper_type helper(nscapi::plugin_singleton->get_core(), 0);
  stub_runtime_data rd;
  const auto item = mixed_item(rd, "NSCA");
  item->severity = NSCAPI::query_return_codes::returnOK;

  for (int s = 0; s < 3; ++s) EXPECT_FALSE(helper.process_item(item, 0, false, make_time(12, 0, s)));
  EXPECT_EQ(3, item->error_count.load());
  ASSERT_EQ(3u, stub().submissions().size());
  for (const auto &sent : stub().submissions()) {
    EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, sent.payload(0).result());
    EXPECT_EQ("logwatch: ERROR: disk on fire (Filter evaluation failed: Refusing to match a regular expression against 2000000 bytes (in: line regexp 'x'))",
              sent.payload(0).lines(0).message());
  }
}

TEST(RealtimeFilterHelperProcessItem, AFailureThatOnlyDiffersInANumberIsTheSameFailure) {
  // The refusal names the size of the line it refused: oversized lines of
  // differing length are one failure, not a new one each batch.
  stub().reset();
  helper_type helper(nscapi::plugin_singleton->get_core(), 0);
  stub_runtime_data rd;
  const auto item = failing_item(rd, "NSCA");
  for (int s = 0; s < 5; ++s) {
    item->filter.evaluation_failure = "Refusing to match a regular expression against " + std::to_string(2000000 + s * 1234) + " bytes (in: line regexp 'x')";
    helper.process_item(item, 0, false, make_time(12, 0, s));
  }
  EXPECT_EQ(5, item->error_count.load());
  ASSERT_EQ(1u, stub().submissions().size());
  // The report itself keeps the number.
  EXPECT_EQ("Filter evaluation failed: Refusing to match a regular expression against 2000000 bytes (in: line regexp 'x')",
            stub().submissions().front().payload(0).lines(0).message());

  EXPECT_EQ(container_type::failure_key("against 2000000 bytes (in: x > 5)"), container_type::failure_key("against 17 bytes (in: x > 10)"));
  EXPECT_NE(container_type::failure_key("Invalid unit: k (in: load > 5k)"), container_type::failure_key("Invalid unit: x (in: load > 5x)"));
}

TEST(RealtimeFilterHelperProcessItem, APersistentFailureFollowsTheSilentPeriod) {
  stub().reset();
  helper_type helper(nscapi::plugin_singleton->get_core(), 0);
  stub_runtime_data rd;
  const auto item = failing_item(rd, "NSCA");
  item->silent_period = boost::posix_time::seconds(30);

  helper.process_item(item, 0, false, make_time(12, 0, 0));
  helper.process_item(item, 0, false, make_time(12, 0, 29));
  EXPECT_EQ(1u, stub().submissions().size());
  helper.process_item(item, 0, false, make_time(12, 0, 30));
  EXPECT_EQ(2u, stub().submissions().size());
}

TEST(RealtimeFilterHelperProcessItem, AFailureInTheTopSyntaxIsPartOfTheVerdict) {
  // The top syntax is rendered before the verdict is decided: a template
  // function that fails there does not go out as a clean result.
  stub().reset();
  helper_type helper(nscapi::plugin_singleton->get_core(), 0);
  stub_runtime_data rd;
  rd.next_result = modern_filter::match_result(true, false);
  std::shared_ptr<container_type> item(new container_type("logwatch", "logwatch", rd));
  item->set_target("NSCA", "", "");
  item->filter.message = "logwatch: 3 lines";
  item->filter.render_failure = "Unknown byte unit: ZB (in: %(format_bytes(size, 'ZB')))";
  item->severity = NSCAPI::query_return_codes::returnOK;

  EXPECT_FALSE(helper.process_item(item, 0, false));
  EXPECT_EQ(1, item->error_count.load());
  ASSERT_EQ(1u, stub().submissions().size());
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, stub().submissions().front().payload(0).result());

  // Beside a CRITICAL the matched records reached, it rides along.
  stub().reset();
  item->filter.evaluation_failure.clear();
  item->clear_failure();
  item->severity = NSCAPI::query_return_codes::returnCRIT;
  EXPECT_TRUE(helper.process_item(item, 0, false));
  ASSERT_EQ(1u, stub().submissions().size());
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, stub().submissions().front().payload(0).result());
  EXPECT_NE(std::string::npos, stub().submissions().front().payload(0).lines(0).message().find("Unknown byte unit: ZB"));
}

TEST(RealtimeFilterHelperProcessItem, AStatusFilterThatMatchedOnlyOkIsUnknownBesideAFailure) {
  // OK is not a verdict a failure can stand beside: the failed record might
  // have been the problem.
  stub().reset();
  helper_type helper(nscapi::plugin_singleton->get_core(), 0);
  stub_runtime_data rd;
  const auto item = mixed_item(rd, "NSCA");
  item->severity = NSCAPI::query_return_codes::returnOK;

  EXPECT_FALSE(helper.process_item(item, 0, false));
  ASSERT_EQ(1u, stub().submissions().size());
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, stub().submissions().front().payload(0).result());
}
