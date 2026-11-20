// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "reload_request.hpp"

#include <gtest/gtest.h>

#include <string>

namespace rr = nsclient::core::reload_request;

namespace {

rr::request asked(const std::string &task, const bool delayed = false) {
  rr::request result;
  result.task = task;
  result.delayed = delayed;
  return result;
}

rr::caller from(const bool inside_any_module, const bool inside_target, const bool scheduler_running) {
  rr::caller result;
  result.inside_any_module = inside_any_module;
  result.inside_target = inside_target;
  result.scheduler_running = scheduler_running;
  return result;
}

const rr::caller outside_any_module = from(false, false, true);

}  // namespace

// --- parse -------------------------------------------------------------------

TEST(ReloadRequestParse, ABareTaskIsInstant) {
  for (const std::string task : {"settings", "service", "CheckSystem"}) {
    const rr::request request = rr::parse(task);
    EXPECT_EQ(request.task, task);
    EXPECT_FALSE(request.delayed) << task;
  }
}

TEST(ReloadRequestParse, DelayedAndDelayBothDelay) {
  for (const std::string prefix : {"delayed,", "delay,"}) {
    const rr::request request = rr::parse(prefix + "service");
    EXPECT_EQ(request.task, "service") << prefix;
    EXPECT_TRUE(request.delayed) << prefix;
  }
}

TEST(ReloadRequestParse, InstantIsStrippedAndStaysInstant) {
  const rr::request request = rr::parse("instant,CheckSystem");
  EXPECT_EQ(request.task, "CheckSystem");
  EXPECT_FALSE(request.delayed);
}

TEST(ReloadRequestParse, APrefixWithNothingAfterItIsNotAPrefix) {
  // Nothing to reload, so it stays whole and fails as an unknown module rather
  // than turning into a reload of the module named "". All three alike: the
  // instant prefix used to be measured against the wrong length and did
  // produce the empty task.
  for (const std::string text : {"delayed,", "delay,", "instant,"}) {
    const rr::request request = rr::parse(text);
    EXPECT_EQ(request.task, text);
    EXPECT_FALSE(request.delayed) << text;
  }
}

TEST(ReloadRequestParse, OnlyTheFirstPrefixIsRead) {
  const rr::request request = rr::parse("delayed,instant,service");
  EXPECT_EQ(request.task, "instant,service");
  EXPECT_TRUE(request.delayed);
}

TEST(ReloadRequestParse, APrefixIsMatchedExactly) {
  // A module may be called anything, including something that starts like a
  // prefix or holds one further in.
  for (const std::string text : {"delayedservice", "Delayed,service", "my,delayed,service", "instantly", ""}) {
    const rr::request request = rr::parse(text);
    EXPECT_EQ(request.task, text);
    EXPECT_FALSE(request.delayed) << text;
  }
}

// --- decide ------------------------------------------------------------------

TEST(ReloadRequestDecide, ADelayedRequestIsAlwaysDeferred) {
  // Wherever it came from, and whether or not the scheduler is running yet:
  // the task is queued and runs once it is.
  for (const bool inside : {false, true}) {
    for (const bool running : {false, true}) {
      EXPECT_EQ(rr::decide(asked("service", true), from(inside, inside, running)), rr::action::defer);
      EXPECT_EQ(rr::decide(asked("settings", true), from(inside, inside, running)), rr::action::defer);
    }
  }
}

TEST(ReloadRequestDecide, FromOutsideAnyModuleItRunsInline) {
  for (const std::string task : {"settings", "service", "NRPEServer"}) {
    EXPECT_EQ(rr::decide(asked(task), outside_any_module), rr::action::run_inline) << task;
    EXPECT_EQ(rr::decide(asked(task), from(false, false, false)), rr::action::run_inline) << task;
  }
}

TEST(ReloadRequestDecide, SettingsRunsInlineEvenFromInsideAModule) {
  // It touches no module, so there is nothing for the calling thread to be in
  // the way of.
  EXPECT_EQ(rr::decide(asked("settings"), from(true, true, true)), rr::action::run_inline);
  EXPECT_EQ(rr::decide(asked("settings"), from(true, true, false)), rr::action::run_inline);
}

TEST(ReloadRequestDecide, FromInsideAModuleItIsHandedToTheScheduler) {
  // Including a module other than the one the thread is inside: a Lua check
  // served over NRPE reloading NRPEServer is on an NRPE thread while
  // dispatching LUAScript.
  EXPECT_EQ(rr::decide(asked("service"), from(true, false, true)), rr::action::defer);
  EXPECT_EQ(rr::decide(asked("NRPEServer"), from(true, false, true)), rr::action::defer);
  EXPECT_EQ(rr::decide(asked("LUAScript"), from(true, true, true)), rr::action::defer);
}

TEST(ReloadRequestDecide, WithoutASchedulerTheModuleTheThreadIsInsideIsRefused) {
  EXPECT_EQ(rr::decide(asked("LUAScript"), from(true, true, false)), rr::action::refuse);
}

TEST(ReloadRequestDecide, WithoutASchedulerEveryModuleIsRefusedFromInsideOne) {
  // "service" reloads them all, the one the thread is inside among them.
  EXPECT_EQ(rr::decide(asked("service"), from(true, false, false)), rr::action::refuse);
}

TEST(ReloadRequestDecide, WithoutASchedulerAnotherModuleStillReloadsInline) {
  // What the unit-test scripts do to apply the configuration they just set.
  EXPECT_EQ(rr::decide(asked("CheckSystem"), from(true, false, false)), rr::action::run_inline);
}

TEST(ReloadRequestDecide, WhatTheFleetSyncAsksForIsDeferred) {
  // The one caller inside the core: it must never reload on its own thread.
  EXPECT_EQ(rr::decide(rr::parse("delayed,service"), outside_any_module), rr::action::defer);
}
