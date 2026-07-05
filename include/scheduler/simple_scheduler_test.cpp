// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <atomic>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <chrono>
#include <functional>
#include <parsers/cron/cron_parser.hpp>
#include <scheduler/simple_scheduler.hpp>
#include <string>
#include <thread>

namespace {

// --- helpers ---------------------------------------------------------------

// Poll-based wait used by the threaded integration tests. Keep the
// granularity small (10 ms) and the timeout generous (a few seconds) so the
// tests stay reliable on CI workers under load.
bool wait_for(const std::function<bool()>& predicate, std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return predicate();
}

// Minimal handler implementation. Counts invocations and lets the test
// control whether the scheduler should reschedule (true) or abandon (false)
// the task after each call. `on_error` / `on_trace` are intentionally silent
// so a failing test doesn't drown the gtest output in scheduler chatter.
class counting_handler : public simple_scheduler::handler {
 public:
  std::atomic<int> calls{0};
  std::atomic<bool> reschedule_after{false};

  bool handle_schedule(simple_scheduler::task /*item*/) override {
    calls.fetch_add(1, std::memory_order_relaxed);
    return reschedule_after.load(std::memory_order_relaxed);
  }
  void on_error(const char* /*file*/, int /*line*/, std::string /*err*/) override {}
  void on_trace(const char* /*file*/, int /*line*/, std::string /*msg*/) override {}
};

}  // namespace

// --- task struct -----------------------------------------------------------

TEST(simple_scheduler_task, default_task_is_disabled) {
  const simple_scheduler::task t;
  EXPECT_TRUE(t.is_disabled());
}

TEST(simple_scheduler_task, duration_task_is_not_disabled) {
  const simple_scheduler::task t("tag", boost::posix_time::seconds(10), 0.0);
  EXPECT_FALSE(t.is_disabled());
}

TEST(simple_scheduler_task, schedule_task_is_not_disabled) {
  const simple_scheduler::task t("tag", cron_parser::parse("* * * * *"));
  EXPECT_FALSE(t.is_disabled());
}

TEST(simple_scheduler_task, get_next_duration_without_jitter_is_exact) {
  using boost::gregorian::date;
  using boost::posix_time::ptime;
  using boost::posix_time::seconds;
  const ptime base(date(2024, 1, 1), seconds(0));
  const simple_scheduler::task t("tag", seconds(30), 0.0);
  EXPECT_EQ(t.get_next(base), base + seconds(30));
}

TEST(simple_scheduler_task, get_next_duration_with_jitter_stays_within_range) {
  using boost::gregorian::date;
  using boost::posix_time::ptime;
  using boost::posix_time::seconds;
  const ptime base(date(2024, 1, 1), seconds(0));
  // jitter_factor=0.5 means the next time is sampled uniformly over the
  // last half of the window: result is in [base + 15s, base + 30s]. Run the
  // sampling enough times to catch a regression that drifts outside the
  // window but not so many that the test becomes a stress test.
  const simple_scheduler::task t("tag", seconds(30), 0.5);
  for (int i = 0; i < 100; ++i) {
    const ptime next = t.get_next(base);
    EXPECT_GE(next, base + seconds(15));
    EXPECT_LE(next, base + seconds(30));
  }
}

TEST(simple_scheduler_task, get_next_floors_to_one_second_minimum) {
  using boost::gregorian::date;
  using boost::posix_time::ptime;
  using boost::posix_time::seconds;
  const ptime base(date(2024, 1, 1), seconds(0));
  // A 1-second duration with full jitter could produce a sub-second wait
  // were the floor missing; the implementation clamps to >= 1s. We can't
  // observe the pre-clamp value so we just assert the published guarantee.
  const simple_scheduler::task t("tag", seconds(1), 1.0);
  for (int i = 0; i < 50; ++i) {
    EXPECT_GE(t.get_next(base), base + seconds(1));
  }
}

TEST(simple_scheduler_task, get_next_zero_duration_returns_now) {
  using boost::gregorian::date;
  using boost::posix_time::ptime;
  using boost::posix_time::seconds;
  const ptime base(date(2024, 1, 1), seconds(0));
  // duration==0 short-circuits the jitter branch and returns now_time
  // unmodified. The scheduler relies on this for "fire immediately" tasks.
  const simple_scheduler::task t("tag", seconds(0), 0.0);
  EXPECT_EQ(t.get_next(base), base);
}

TEST(simple_scheduler_task, get_next_uses_cron_schedule) {
  using boost::gregorian::date;
  using boost::posix_time::ptime;
  using boost::posix_time::time_duration;
  // Daily at 12:00. Starting from 10:00 the next match must be the same
  // day's 12:00.
  const cron_parser::schedule sched = cron_parser::parse("0 12 * * *");
  const simple_scheduler::task t("tag", sched);
  const ptime base(date(2024, 1, 1), time_duration(10, 0, 0));
  const ptime next = t.get_next(base);
  EXPECT_EQ(next, ptime(date(2024, 1, 1), time_duration(12, 0, 0)));
}

TEST(simple_scheduler_task, to_string_reports_disabled) {
  const simple_scheduler::task t;
  EXPECT_NE(t.to_string().find("disabled"), std::string::npos);
}

TEST(simple_scheduler_task, to_string_reports_tag_and_duration) {
  const simple_scheduler::task t("my_tag", boost::posix_time::seconds(60), 0.0);
  const std::string s = t.to_string();
  EXPECT_NE(s.find("my_tag"), std::string::npos);
  EXPECT_NE(s.find("60"), std::string::npos);
}

TEST(simple_scheduler_task, to_string_reports_cron_schedule) {
  // The cron parser doesn't support `*/5`-style step syntax; only explicit
  // integer lists ("0,15,30,45") or `*`. Use the explicit form so we know
  // exactly what to_string will round-trip to.
  const simple_scheduler::task t("cron_tag", cron_parser::parse("5 * * * *"));
  const std::string s = t.to_string();
  EXPECT_NE(s.find("cron_tag"), std::string::npos);
  EXPECT_NE(s.find("5 * * * *"), std::string::npos);
}

// --- safe_schedule_queue ---------------------------------------------------

TEST(simple_scheduler_queue, starts_empty) {
  simple_scheduler::safe_schedule_queue<simple_scheduler::schedule_instance> q;
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(q.size(), 0u);
  EXPECT_FALSE(q.top());
  EXPECT_FALSE(q.pop());
}

TEST(simple_scheduler_queue, push_then_pop_roundtrips_payload) {
  simple_scheduler::safe_schedule_queue<simple_scheduler::schedule_instance> q;
  simple_scheduler::schedule_instance instance;
  instance.tag = "hi";
  instance.schedule_id = 7;
  instance.time = boost::posix_time::ptime(boost::gregorian::date(2024, 1, 1));

  EXPECT_TRUE(q.push(instance));
  EXPECT_EQ(q.size(), 1u);

  const auto popped = q.pop();
  ASSERT_TRUE(popped);
  EXPECT_EQ(popped->tag, "hi");
  EXPECT_EQ(popped->schedule_id, 7);
  EXPECT_TRUE(q.empty());
}

TEST(simple_scheduler_queue, top_does_not_remove) {
  simple_scheduler::safe_schedule_queue<simple_scheduler::schedule_instance> q;
  simple_scheduler::schedule_instance instance;
  instance.tag = "peek";
  instance.schedule_id = 1;
  instance.time = boost::posix_time::ptime(boost::gregorian::date(2024, 1, 1));
  q.push(instance);

  EXPECT_TRUE(q.top());
  EXPECT_EQ(q.size(), 1u);
  EXPECT_TRUE(q.top());
  EXPECT_EQ(q.size(), 1u);
}

TEST(simple_scheduler_queue, earliest_time_pops_first) {
  using boost::gregorian::date;
  using boost::posix_time::ptime;
  using boost::posix_time::seconds;
  simple_scheduler::safe_schedule_queue<simple_scheduler::schedule_instance> q;
  const ptime base(date(2024, 1, 1));

  // schedule_instance's operator< inverts the time comparison so that the
  // std::priority_queue (which is a max-heap by default) returns the
  // earliest scheduled instance from top()/pop().
  simple_scheduler::schedule_instance later;
  later.time = base + seconds(60);
  later.tag = "later";
  simple_scheduler::schedule_instance sooner;
  sooner.time = base + seconds(5);
  sooner.tag = "sooner";

  q.push(later);
  q.push(sooner);

  const auto first = q.pop();
  ASSERT_TRUE(first);
  EXPECT_EQ(first->tag, "sooner");
  const auto second = q.pop();
  ASSERT_TRUE(second);
  EXPECT_EQ(second->tag, "later");
  EXPECT_FALSE(q.pop());
}

// --- scheduler bookkeeping (no threads) -----------------------------------

TEST(simple_scheduler_basic, default_thread_count_is_ten) {
  const simple_scheduler::scheduler s;
  EXPECT_EQ(s.get_threads(), 10u);
}

TEST(simple_scheduler_basic, set_threads_before_start_updates_count) {
  simple_scheduler::scheduler s;
  // start_threads() returns immediately when not running, so no threads are
  // spawned here - we just verify the bookkeeping value is what the worker
  // pool will be sized to once start() actually runs.
  s.set_threads(3);
  EXPECT_EQ(s.get_threads(), 3u);
}

TEST(simple_scheduler_basic, timezone_defaults_to_empty_and_roundtrips) {
  simple_scheduler::scheduler s;
  EXPECT_EQ(s.get_timezone(), "");
  s.set_timezone("UTC");
  EXPECT_EQ(s.get_timezone(), "UTC");
  s.set_timezone("EST-05EDT,M3.2.0,M11.1.0");
  EXPECT_EQ(s.get_timezone(), "EST-05EDT,M3.2.0,M11.1.0");
}

TEST(simple_scheduler_basic, add_duration_task_returns_monotonically_increasing_ids) {
  simple_scheduler::scheduler s;
  const int id1 = s.add_task("first", boost::posix_time::seconds(10), 0.0);
  const int id2 = s.add_task("second", boost::posix_time::seconds(10), 0.0);
  const int id3 = s.add_task("third", boost::posix_time::seconds(10), 0.0);
  EXPECT_GT(id1, 0);
  EXPECT_GT(id2, id1);
  EXPECT_GT(id3, id2);
}

TEST(simple_scheduler_basic, get_task_returns_added_task_metadata) {
  simple_scheduler::scheduler s;
  const int id = s.add_task("foo", boost::posix_time::seconds(10), 0.0);
  const auto fetched = s.get_task(id);
  ASSERT_TRUE(fetched);
  EXPECT_EQ(fetched->id, id);
  EXPECT_EQ(fetched->tag, "foo");
  EXPECT_FALSE(fetched->is_disabled());
}

TEST(simple_scheduler_basic, add_cron_task_is_retrievable) {
  simple_scheduler::scheduler s;
  const int id = s.add_task("cron_task", cron_parser::parse("* * * * *"));
  const auto fetched = s.get_task(id);
  ASSERT_TRUE(fetched);
  EXPECT_EQ(fetched->tag, "cron_task");
  EXPECT_FALSE(fetched->is_disabled());
}

TEST(simple_scheduler_basic, get_task_unknown_id_returns_none) {
  simple_scheduler::scheduler s;
  EXPECT_FALSE(s.get_task(999));
}

TEST(simple_scheduler_basic, remove_task_removes_from_lookup) {
  simple_scheduler::scheduler s;
  const int id = s.add_task("foo", boost::posix_time::seconds(10), 0.0);
  ASSERT_TRUE(s.get_task(id));
  s.remove_task(id);
  EXPECT_FALSE(s.get_task(id));
}

TEST(simple_scheduler_basic, clear_tasks_removes_every_task) {
  simple_scheduler::scheduler s;
  const int id1 = s.add_task("a", boost::posix_time::seconds(10), 0.0);
  const int id2 = s.add_task("b", boost::posix_time::seconds(10), 0.0);
  s.clear_tasks();
  EXPECT_FALSE(s.get_task(id1));
  EXPECT_FALSE(s.get_task(id2));
}

// --- scheduler lifecycle (drives the thread pool) -------------------------

TEST(simple_scheduler_running, handle_schedule_fires_for_zero_duration_task) {
  simple_scheduler::scheduler s;
  counting_handler h;
  // The handler returns false from handle_schedule so the task is abandoned
  // after the first run - otherwise a duration=0 task would re-schedule
  // itself for "now" in a tight loop and burn CPU for the duration of the
  // test.
  h.reschedule_after = false;
  s.set_handler(&h);
  s.set_threads(2);
  // Queue the task BEFORE start() so the worker threads find it in the
  // queue on their very first pop() call. Doing it the other way around
  // exposes a narrow missed-wakeup window: notify_one() fires before any
  // thread has reached cond.wait(), and the wake-up is dropped on the
  // floor.
  s.add_task("once", boost::posix_time::seconds(0), 0.0);
  s.start();

  EXPECT_TRUE(wait_for([&] { return h.calls.load() >= 1; }, std::chrono::seconds(5)));

  s.stop();
  s.unset_handler();
}

TEST(simple_scheduler_running, stop_clears_thread_count) {
  simple_scheduler::scheduler s;
  s.set_threads(2);
  s.start();
  s.stop();
  // stop() explicitly writes thread_count_ to zero after waiting for the
  // worker pool to drain. This is what callers see via get_metric_threads
  // / get_threads after shutdown.
  EXPECT_EQ(s.get_threads(), 0u);
}

TEST(simple_scheduler_running, prepare_shutdown_is_idempotent_with_stop) {
  simple_scheduler::scheduler s;
  s.set_threads(2);
  s.start();
  // prepare_shutdown sets stop_requested_ / clears running_ but does not
  // join. Calling stop() afterwards should still cleanly drain the pool
  // rather than hanging.
  s.prepare_shutdown();
  s.stop();
  EXPECT_EQ(s.get_threads(), 0u);
}

TEST(simple_scheduler_running, no_handler_does_not_crash) {
  // It's legal to start the scheduler without a handler installed (eg.
  // during module load before the handler-owning object is constructed).
  // The worker simply skips the dispatch.
  simple_scheduler::scheduler s;
  s.set_threads(1);
  s.start();
  s.add_task("orphan", boost::posix_time::seconds(0), 0.0);
  // Give the worker a chance to pick up the item and skip it.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  s.stop();
  SUCCEED();
}
