// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The bounded event buffer behind /api/v2/events.
//
// It is the only thing between a chatty event producer and the daemon's
// address space, and the two consumers disagree on purpose: the polling
// endpoint lists without draining, the streaming one drains. Neither
// behaviour, nor the cap that keeps the buffer bounded, was covered.

#include "event_store.hpp"

#include <gtest/gtest.h>

#include <map>
#include <string>

namespace {

std::map<std::string, std::string> payload(const std::string &key, const std::string &value) { return {{key, value}}; }

}  // namespace

TEST(EventStore, StartsEmpty) {
  event_store store;
  EXPECT_EQ(store.size(), 0u);
  EXPECT_TRUE(store.list().empty());
}

TEST(EventStore, KeepsWhatItIsGiven) {
  event_store store;
  store.add("service-started", payload("module", "CheckDisk"));

  const event_store::event_list events = store.list();
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0].event, "service-started");
  ASSERT_EQ(events[0].data.count("module"), 1u);
  EXPECT_EQ(events[0].data.at("module"), "CheckDisk");
  // Stamped by the store, not the caller, so a consumer always has one.
  EXPECT_FALSE(events[0].date.empty());
}

TEST(EventStore, IndexesAreMonotonicAndStartAtZero) {
  // The streaming client resumes from the last index it saw, so these have to
  // be strictly increasing and must not restart.
  event_store store;
  store.add("a", {});
  store.add("b", {});
  store.add("c", {});

  const event_store::event_list events = store.list();
  ASSERT_EQ(events.size(), 3u);
  EXPECT_EQ(events[0].index, 0u);
  EXPECT_EQ(events[1].index, 1u);
  EXPECT_EQ(events[2].index, 2u);
}

TEST(EventStore, IndexesKeepClimbingAcrossADrain) {
  event_store store;
  store.add("a", {});
  store.add("b", {});
  ASSERT_EQ(store.pop_all().size(), 2u);

  store.add("c", {});
  const event_store::event_list events = store.list();
  ASSERT_EQ(events.size(), 1u);
  // Not 0: a client that resumed at index 1 must not be handed a "new" event
  // numbered below what it has already seen.
  EXPECT_EQ(events[0].index, 2u);
}

TEST(EventStore, EventsAreKeptInArrivalOrder) {
  event_store store;
  store.add("first", {});
  store.add("second", {});

  const event_store::event_list events = store.list();
  ASSERT_EQ(events.size(), 2u);
  EXPECT_EQ(events[0].event, "first");
  EXPECT_EQ(events[1].event, "second");
}

TEST(EventStore, ListingDoesNotDrain) {
  // /api/v2/events polls; two polls in a row must see the same events.
  event_store store;
  store.add("a", {});

  EXPECT_EQ(store.list().size(), 1u);
  EXPECT_EQ(store.list().size(), 1u);
  EXPECT_EQ(store.size(), 1u);
}

TEST(EventStore, PopAllDrains) {
  event_store store;
  store.add("a", {});
  store.add("b", {});

  const event_store::event_list drained = store.pop_all();
  ASSERT_EQ(drained.size(), 2u);
  EXPECT_EQ(drained[0].event, "a");
  EXPECT_EQ(drained[1].event, "b");

  EXPECT_EQ(store.size(), 0u);
  EXPECT_TRUE(store.list().empty());
}

TEST(EventStore, PopAllOnAnEmptyStoreIsEmptyRatherThanAnError) {
  event_store store;
  EXPECT_TRUE(store.pop_all().empty());
  EXPECT_EQ(store.size(), 0u);
}

TEST(EventStore, TheOldestEventIsDroppedOnceTheCapIsReached) {
  // The whole point of the cap: an event producer nobody is draining must not
  // grow the daemon without bound.
  event_store store;
  store.set_max_entries(3);
  for (int i = 0; i < 5; ++i) store.add("e" + std::to_string(i), {});

  const event_store::event_list events = store.list();
  ASSERT_EQ(events.size(), 3u);
  EXPECT_EQ(events[0].event, "e2");
  EXPECT_EQ(events[2].event, "e4");
}

TEST(EventStore, LoweringTheCapTrimsWhatIsAlreadyBuffered) {
  event_store store;
  for (int i = 0; i < 5; ++i) store.add("e" + std::to_string(i), {});
  ASSERT_EQ(store.size(), 5u);

  // Settings are applied while the daemon runs, so a smaller cap has to take
  // effect immediately rather than only for events that arrive later.
  store.set_max_entries(2);
  const event_store::event_list events = store.list();
  ASSERT_EQ(events.size(), 2u);
  EXPECT_EQ(events[0].event, "e3");
  EXPECT_EQ(events[1].event, "e4");
}

TEST(EventStore, ACapOfZeroIsClampedToOne) {
  // `max entries = 0` in a config file would otherwise mean "throw everything
  // away", which is silently useless; one event is the smallest useful buffer.
  event_store store;
  store.set_max_entries(0);
  store.add("a", {});
  store.add("b", {});

  const event_store::event_list events = store.list();
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0].event, "b");
}

TEST(EventStore, RaisingTheCapKeepsWhatIsBuffered) {
  event_store store;
  store.set_max_entries(2);
  store.add("a", {});
  store.add("b", {});
  store.set_max_entries(10);
  store.add("c", {});

  EXPECT_EQ(store.size(), 3u);
}

TEST(EventStore, TheDefaultCapIsAThousand) {
  event_store store;
  for (int i = 0; i < 1005; ++i) store.add("e" + std::to_string(i), {});

  const event_store::event_list events = store.list();
  ASSERT_EQ(events.size(), 1000u);
  EXPECT_EQ(events.front().event, "e5");
  EXPECT_EQ(events.back().event, "e1004");
}
