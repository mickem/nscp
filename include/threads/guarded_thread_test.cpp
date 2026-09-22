// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <atomic>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/thread.hpp>
#include <stdexcept>
#include <string>
#include <threads/guarded_io_context.hpp>
#include <threads/guarded_thread.hpp>
#include <vector>

namespace {

// Collects what the guard reported, so a test can assert on the operator's
// view rather than on the fact that the process happened not to die.
struct recorder {
  std::vector<std::pair<std::string, std::string> > events;
  void operator()(const std::string &name, const std::string &detail) { events.push_back(std::make_pair(name, detail)); }
};

struct custom_error {};

}  // namespace

// --- run_guarded ---

TEST(guarded_thread, runs_the_body) {
  bool ran = false;
  recorder rec;
  threads::run_guarded("worker", [&ran]() { ran = true; }, std::ref(rec));
  EXPECT_TRUE(ran);
  EXPECT_TRUE(rec.events.empty());
}

TEST(guarded_thread, contains_a_std_exception_and_reports_its_message) {
  recorder rec;
  threads::run_guarded("worker", []() { throw std::runtime_error("the disk fell off"); }, std::ref(rec));
  ASSERT_EQ(rec.events.size(), 1u);
  EXPECT_EQ(rec.events[0].first, "worker");
  // The message is the whole point: without what() an operator has a dead
  // collector and no idea why.
  EXPECT_NE(rec.events[0].second.find("the disk fell off"), std::string::npos);
}

TEST(guarded_thread, contains_an_exception_of_unknown_type) {
  recorder rec;
  threads::run_guarded("worker", []() { throw custom_error(); }, std::ref(rec));
  ASSERT_EQ(rec.events.size(), 1u);
  EXPECT_NE(rec.events[0].second.find("unknown type"), std::string::npos);
}

TEST(guarded_thread, a_cooperative_interrupt_is_not_reported) {
  recorder rec;
  threads::run_guarded("worker", []() { throw boost::thread_interrupted(); }, std::ref(rec));
  // Shutdown, not failure. Reporting it would put a critical in the log on
  // every single stop.
  EXPECT_TRUE(rec.events.empty());
}

TEST(guarded_thread, a_stop_request_is_not_reported) {
  recorder rec;
  threads::run_guarded("worker", []() { throw threads::stop_requested(); }, std::ref(rec));
  EXPECT_TRUE(rec.events.empty());
}

TEST(guarded_thread, a_reporter_that_throws_does_not_escape) {
  // The reporter is a logger, and during an unclean shutdown the logger can
  // be gone. Throwing from there would be the terminate() the guard exists to
  // prevent.
  EXPECT_NO_THROW(threads::run_guarded(
      "worker", []() { throw std::runtime_error("boom"); }, [](const std::string &, const std::string &) { throw std::runtime_error("the logger is gone"); }));
}

// --- start_guarded_thread ---

TEST(guarded_thread, an_exception_in_a_started_thread_is_contained_and_reported) {
  recorder rec;
  boost::mutex mutex;
  const std::shared_ptr<boost::thread> thread = threads::start_guarded_thread(
      "background worker", []() { throw std::runtime_error("threw on a worker"); },
      [&rec, &mutex](const std::string &name, const std::string &detail) {
        boost::lock_guard<boost::mutex> lock(mutex);
        rec(name, detail);
      });
  ASSERT_TRUE(thread);
  thread->join();

  boost::lock_guard<boost::mutex> lock(mutex);
  ASSERT_EQ(rec.events.size(), 1u);
  EXPECT_EQ(rec.events[0].first, "background worker");
  EXPECT_NE(rec.events[0].second.find("threw on a worker"), std::string::npos);
}

TEST(guarded_thread, a_started_thread_is_joinable_like_any_other) {
  std::atomic<bool> ran(false);
  recorder rec;
  const std::shared_ptr<boost::thread> thread = threads::start_guarded_thread("background worker", [&ran]() { ran.store(true); }, std::ref(rec));
  thread->join();
  EXPECT_TRUE(ran.load());
}

// --- run_io_context_guarded ---

TEST(guarded_io_context, runs_handlers_and_returns_when_out_of_work) {
  boost::asio::io_context io;
  bool ran = false;
  boost::asio::post(io, [&ran]() { ran = true; });

  recorder rec;
  threads::run_io_context_guarded("server", io, std::ref(rec));
  EXPECT_TRUE(ran);
  EXPECT_TRUE(rec.events.empty());
}

TEST(guarded_io_context, a_handler_that_throws_does_not_end_the_event_loop) {
  boost::asio::io_context io;
  bool later_handler_ran = false;
  boost::asio::post(io, []() { throw std::runtime_error("bad request"); });
  boost::asio::post(io, [&later_handler_ran]() { later_handler_ran = true; });

  recorder rec;
  threads::run_io_context_guarded("server", io, std::ref(rec));

  // This is the whole behaviour change: the request fails, the server keeps
  // serving. Before, the throw came out of run() and took the pool thread -
  // or, on a bare thread, the process - with it.
  EXPECT_TRUE(later_handler_ran);
  ASSERT_EQ(rec.events.size(), 1u);
  EXPECT_EQ(rec.events[0].first, "server");
  EXPECT_NE(rec.events[0].second.find("bad request"), std::string::npos);
  EXPECT_NE(rec.events[0].second.find("restarting the event loop"), std::string::npos);
}

TEST(guarded_io_context, reports_every_handler_that_throws) {
  boost::asio::io_context io;
  boost::asio::post(io, []() { throw std::runtime_error("first"); });
  boost::asio::post(io, []() { throw std::runtime_error("second"); });

  recorder rec;
  threads::run_io_context_guarded("server", io, std::ref(rec));
  ASSERT_EQ(rec.events.size(), 2u);
  EXPECT_NE(rec.events[0].second.find("first"), std::string::npos);
  EXPECT_NE(rec.events[1].second.find("second"), std::string::npos);
}
