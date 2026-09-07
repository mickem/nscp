// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The passive-result cache behind /api/v2/results.
//
// Unlike the event store next door this one is keyed: only one result is kept
// per key, and the cache mode decides which one survives a collision - the
// newest (`last`) or the most severe (`worst`). Paired with a draining poll,
// `worst` is what stops a CRITICAL that recovered between two polls from
// going unseen, so those two behaviours are the heart of the contract. Around
// them sit the switch that keeps the whole thing off until an operator asks
// for it, and the two bounds - a key cap and an age limit - that keep a
// misbehaving producer from growing the daemon without end.

#include "result_store.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

// A fixed "now" so the age arithmetic in the tests is exact rather than
// whatever the wall clock happened to say.
const std::int64_t kNow = 1'700'000'000;

result_store::result_entry make(const std::string &host, const std::string &command, const int status = 0) {
  result_store::result_entry e;
  e.key = host + "/" + command;
  e.channel = "WEB";
  e.host = host;
  e.source = host;
  e.command = command;
  e.status = status;
  e.message = command + " says hello";
  return e;
}

result_store::filter no_filter() { return result_store::filter(); }

}  // namespace

TEST(ResultStore, IsDisabledUntilItIsSwitchedOn) {
  // Nothing accumulates behind an operator's back: an install that never
  // turned the cache on must not be holding check results in memory.
  result_store store;
  EXPECT_FALSE(store.is_enabled());
  store.submit(make("srv1", "check_disk"), kNow);
  EXPECT_EQ(store.size(), 0u);
  EXPECT_TRUE(store.list(no_filter(), kNow).empty());
}

TEST(ResultStore, AcceptsResultsOnceEnabled) {
  result_store store;
  store.set_enabled(true);
  store.submit(make("srv1", "check_disk"), kNow);
  EXPECT_EQ(store.size(), 1u);
}

TEST(ResultStore, SwitchingItOffEmptiesIt) {
  // Otherwise the endpoint would keep serving results from a cache the
  // operator has just turned off.
  result_store store;
  store.set_enabled(true);
  store.submit(make("srv1", "check_disk"), kNow);
  ASSERT_EQ(store.size(), 1u);

  store.set_enabled(false);
  EXPECT_EQ(store.size(), 0u);
  EXPECT_FALSE(store.is_enabled());
}

TEST(ResultStore, TheDefaultModeIsLast) {
  result_store store;
  EXPECT_EQ(store.mode(), result_store::mode_last);
}

TEST(ResultStore, StartsEmpty) {
  result_store store;
  EXPECT_EQ(store.size(), 0u);
  EXPECT_TRUE(store.list().empty());
}

TEST(ResultStore, KeepsWhatItIsGiven) {
  result_store store;
  store.set_enabled(true);
  store.submit(make("srv1", "check_disk", 1), kNow);

  const result_store::result_list results = store.list(no_filter(), kNow);
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].key, "srv1/check_disk");
  EXPECT_EQ(results[0].host, "srv1");
  EXPECT_EQ(results[0].command, "check_disk");
  EXPECT_EQ(results[0].status, 1);
  EXPECT_EQ(results[0].message, "check_disk says hello");
  // Stamped by the store, so a consumer always has them.
  EXPECT_EQ(results[0].first_seen, kNow);
  EXPECT_EQ(results[0].last_seen, kNow);
  EXPECT_EQ(results[0].count, 1u);
}

TEST(ResultStore, ARepeatResultReplacesRatherThanAccumulates) {
  // This is what makes the store a cache: a check reporting every minute
  // must not grow the store by an entry a minute.
  result_store store;
  store.set_enabled(true);
  store.submit(make("srv1", "check_disk", 0), kNow);
  store.submit(make("srv1", "check_disk", 2), kNow + 60);

  const result_store::result_list results = store.list(no_filter(), kNow + 60);
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].status, 2);
}

TEST(ResultStore, ARepeatResultKeepsTheKeysHistory) {
  result_store store;
  store.set_enabled(true);
  store.submit(make("srv1", "check_disk"), kNow);
  store.submit(make("srv1", "check_disk"), kNow + 60);
  store.submit(make("srv1", "check_disk"), kNow + 120);

  result_store::result_entry entry;
  ASSERT_TRUE(store.get("srv1/check_disk", entry, kNow + 120));
  EXPECT_EQ(entry.count, 3u);
  // first_seen is the key's, not this submission's - "how long has this
  // check been reporting" has to survive an update.
  EXPECT_EQ(entry.first_seen, kNow);
  EXPECT_EQ(entry.last_seen, kNow + 120);
}

TEST(ResultStore, DifferentKeysAreKeptSideBySide) {
  result_store store;
  store.set_enabled(true);
  store.submit(make("srv1", "check_disk"), kNow);
  store.submit(make("srv2", "check_disk"), kNow);
  store.submit(make("srv1", "check_cpu"), kNow);

  EXPECT_EQ(store.list(no_filter(), kNow).size(), 3u);
  EXPECT_EQ(store.size(), 3u);
}

TEST(ResultStore, ResultsAreListedInKeyOrder) {
  // A client paging through the list must see a stable order even while
  // results keep arriving, so it is sorted by key rather than by arrival.
  result_store store;
  store.set_enabled(true);
  store.submit(make("srv2", "check_disk"), kNow);
  store.submit(make("srv1", "check_disk"), kNow + 1);
  store.submit(make("srv3", "check_disk"), kNow + 2);

  const result_store::result_list results = store.list(no_filter(), kNow + 2);
  ASSERT_EQ(results.size(), 3u);
  EXPECT_EQ(results[0].key, "srv1/check_disk");
  EXPECT_EQ(results[1].key, "srv2/check_disk");
  EXPECT_EQ(results[2].key, "srv3/check_disk");
}

TEST(ResultStore, GetFindsOneResultAndMissesTheRest) {
  result_store store;
  store.set_enabled(true);
  store.submit(make("srv1", "check_disk"), kNow);

  result_store::result_entry entry;
  EXPECT_TRUE(store.get("srv1/check_disk", entry, kNow));
  EXPECT_EQ(entry.command, "check_disk");
  EXPECT_FALSE(store.get("srv1/check_cpu", entry, kNow));
  // Keys are matched exactly, not as a prefix.
  EXPECT_FALSE(store.get("srv1", entry, kNow));
}

TEST(ResultStore, ListingDoesNotDrain) {
  result_store store;
  store.set_enabled(true);
  store.submit(make("srv1", "check_disk"), kNow);

  EXPECT_EQ(store.list(no_filter(), kNow).size(), 1u);
  EXPECT_EQ(store.list(no_filter(), kNow).size(), 1u);
}

TEST(ResultStore, RemoveDropsOneResult) {
  result_store store;
  store.set_enabled(true);
  store.submit(make("srv1", "check_disk"), kNow);
  store.submit(make("srv2", "check_disk"), kNow);

  EXPECT_TRUE(store.remove("srv1/check_disk"));
  EXPECT_FALSE(store.remove("srv1/check_disk"));
  EXPECT_EQ(store.size(), 1u);
}

TEST(ResultStore, ClearDropsEverythingAndSaysHowMuch) {
  result_store store;
  store.set_enabled(true);
  store.submit(make("srv1", "check_disk"), kNow);
  store.submit(make("srv2", "check_disk"), kNow);

  EXPECT_EQ(store.clear(), 2u);
  EXPECT_EQ(store.size(), 0u);
  EXPECT_EQ(store.clear(), 0u);
}

TEST(ResultStore, TheLeastRecentlyUpdatedKeyIsEvictedOnceTheCapIsReached) {
  // The cap only bites when keys keep changing; when it does, the entry that
  // has gone longest without an update is the one to lose.
  result_store store;
  store.set_enabled(true);
  store.set_max_entries(2);
  store.submit(make("srv1", "check_disk"), kNow);
  store.submit(make("srv2", "check_disk"), kNow + 1);
  // srv1 reports again, so srv2 is now the stalest.
  store.submit(make("srv1", "check_disk"), kNow + 2);
  store.submit(make("srv3", "check_disk"), kNow + 3);

  const result_store::result_list results = store.list(no_filter(), kNow + 3);
  ASSERT_EQ(results.size(), 2u);
  EXPECT_EQ(results[0].key, "srv1/check_disk");
  EXPECT_EQ(results[1].key, "srv3/check_disk");
}

TEST(ResultStore, LoweringTheCapTrimsWhatIsAlreadyCached) {
  result_store store;
  store.set_enabled(true);
  for (int i = 0; i < 5; ++i) store.submit(make("srv" + std::to_string(i), "check_disk"), kNow + i);
  ASSERT_EQ(store.size(), 5u);

  // Settings are applied while the daemon runs, so a smaller cap has to take
  // effect at once rather than only for results that arrive later.
  store.set_max_entries(2);
  const result_store::result_list results = store.list(no_filter(), kNow + 5);
  ASSERT_EQ(results.size(), 2u);
  EXPECT_EQ(results[0].key, "srv3/check_disk");
  EXPECT_EQ(results[1].key, "srv4/check_disk");
}

TEST(ResultStore, ACapOfZeroIsClampedToOne) {
  result_store store;
  store.set_enabled(true);
  store.set_max_entries(0);
  store.submit(make("srv1", "check_disk"), kNow);
  store.submit(make("srv2", "check_disk"), kNow + 1);

  const result_store::result_list results = store.list(no_filter(), kNow + 1);
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].key, "srv2/check_disk");
}

TEST(ResultStore, TheDefaultCapIsAThousand) {
  result_store store;
  store.set_enabled(true);
  for (int i = 0; i < 1005; ++i) store.submit(make("srv" + std::to_string(i), "check_disk"), kNow + i);
  EXPECT_EQ(store.size(), 1000u);
}

TEST(ResultStore, ResultsNeverExpireByDefault) {
  result_store store;
  store.set_enabled(true);
  store.submit(make("srv1", "check_disk"), kNow);

  // A year later it is very stale, but staleness is the caller's call to
  // make from `age` - the store still has it.
  EXPECT_EQ(store.list(no_filter(), kNow + 31'536'000).size(), 1u);
}

TEST(ResultStore, AResultOlderThanTheMaxAgeIsNotListed) {
  result_store store;
  store.set_enabled(true);
  store.set_max_age(60);
  store.submit(make("srv1", "check_disk"), kNow);

  EXPECT_EQ(store.list(no_filter(), kNow + 60).size(), 1u);
  EXPECT_EQ(store.list(no_filter(), kNow + 61).size(), 0u);
  result_store::result_entry entry;
  EXPECT_FALSE(store.get("srv1/check_disk", entry, kNow + 61));
}

TEST(ResultStore, AnExpiredResultIsDroppedWhenTheNextOneArrives) {
  // Expiry is lazy, so nothing has to run a timer - but it must actually
  // free the memory, not merely hide the entry from readers forever.
  result_store store;
  store.set_enabled(true);
  store.set_max_age(60);
  store.submit(make("srv1", "check_disk"), kNow);
  store.submit(make("srv2", "check_disk"), kNow + 61);

  const result_store::result_list results = store.list(no_filter(), kNow + 61);
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].key, "srv2/check_disk");
}

TEST(ResultStore, AKeyThatKeepsReportingDoesNotExpire) {
  result_store store;
  store.set_enabled(true);
  store.set_max_age(60);
  store.submit(make("srv1", "check_disk"), kNow);
  store.submit(make("srv1", "check_disk"), kNow + 50);

  EXPECT_EQ(store.list(no_filter(), kNow + 100).size(), 1u);
}

TEST(ResultStore, AnEmptyFilterMatchesEverything) {
  result_store store;
  store.set_enabled(true);
  store.submit(make("srv1", "check_disk", 0), kNow);
  store.submit(make("srv2", "check_cpu", 2), kNow);

  EXPECT_EQ(store.list(no_filter(), kNow).size(), 2u);
}

TEST(ResultStore, FilteringByHostIsExactAndCaseInsensitive) {
  result_store store;
  store.set_enabled(true);
  store.submit(make("srv1", "check_disk"), kNow);
  store.submit(make("srv10", "check_disk"), kNow);

  result_store::filter f;
  // Exact, so `srv1` must not also return `srv10`...
  f.host = "srv1";
  const result_store::result_list exact = store.list(f, kNow);
  ASSERT_EQ(exact.size(), 1u);
  EXPECT_EQ(exact[0].host, "srv1");

  // ...but hostnames are not case sensitive, and a monitoring system that
  // spells one differently should still find it.
  f.host = "SRV1";
  EXPECT_EQ(store.list(f, kNow).size(), 1u);
}

TEST(ResultStore, FilteringByCommandAliasAndChannel) {
  result_store store;
  store.set_enabled(true);
  result_store::result_entry aliased = make("srv1", "check_drivesize");
  aliased.key = "srv1/disk";
  aliased.alias = "disk";
  store.submit(aliased, kNow);
  store.submit(make("srv1", "check_cpu"), kNow);

  result_store::filter by_command;
  by_command.command = "check_cpu";
  ASSERT_EQ(store.list(by_command, kNow).size(), 1u);
  EXPECT_EQ(store.list(by_command, kNow)[0].command, "check_cpu");

  result_store::filter by_alias;
  by_alias.alias = "disk";
  ASSERT_EQ(store.list(by_alias, kNow).size(), 1u);
  EXPECT_EQ(store.list(by_alias, kNow)[0].key, "srv1/disk");

  result_store::filter by_channel;
  by_channel.channel = "WEB";
  EXPECT_EQ(store.list(by_channel, kNow).size(), 2u);
  by_channel.channel = "NSCA";
  EXPECT_EQ(store.list(by_channel, kNow).size(), 0u);
}

TEST(ResultStore, FilteringByStatusAcceptsSeveralStates) {
  result_store store;
  store.set_enabled(true);
  store.submit(make("srv1", "check_disk", 0), kNow);
  store.submit(make("srv2", "check_disk", 1), kNow);
  store.submit(make("srv3", "check_disk", 2), kNow);

  result_store::filter f;
  f.statuses.push_back(1);
  f.statuses.push_back(2);
  const result_store::result_list problems = store.list(f, kNow);
  ASSERT_EQ(problems.size(), 2u);
  EXPECT_EQ(problems[0].host, "srv2");
  EXPECT_EQ(problems[1].host, "srv3");
}

TEST(ResultStore, FiltersAreCombinedWithAnd) {
  result_store store;
  store.set_enabled(true);
  store.submit(make("srv1", "check_disk", 2), kNow);
  store.submit(make("srv1", "check_cpu", 0), kNow);
  store.submit(make("srv2", "check_disk", 0), kNow);

  result_store::filter f;
  f.host = "srv1";
  f.statuses.push_back(2);
  const result_store::result_list results = store.list(f, kNow);
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].key, "srv1/check_disk");
}

TEST(ResultKeyFormatter, DefaultsToHostAndAliasOrCommand) {
  result_key_formatter formatter;
  result_store::result_entry entry = make("srv1", "check_drivesize");
  EXPECT_EQ(formatter.format(entry), "srv1/check_drivesize");

  entry.alias = "disk";
  EXPECT_EQ(formatter.format(entry), "srv1/disk");
}

TEST(ResultKeyFormatter, AnEmptyExpressionMeansTheDefault) {
  result_key_formatter formatter;
  std::string error;
  EXPECT_TRUE(formatter.parse("", error));
  EXPECT_TRUE(error.empty());
  EXPECT_EQ(formatter.format(make("srv1", "check_disk")), "srv1/check_disk");
}

TEST(ResultKeyFormatter, ExpandsEveryDocumentedVariable) {
  result_key_formatter formatter;
  std::string error;
  ASSERT_TRUE(formatter.parse("${channel}:${source}:${host}:${command}:${alias}", error));

  result_store::result_entry entry = make("srv1", "check_drivesize");
  entry.channel = "WEB";
  entry.source = "agent-1";
  entry.alias = "disk";
  EXPECT_EQ(formatter.format(entry), "WEB:agent-1:srv1:check_drivesize:disk");
}

TEST(ResultKeyFormatter, LiteralTextIsKept) {
  result_key_formatter formatter;
  std::string error;
  ASSERT_TRUE(formatter.parse("nscp/${host}/passive", error));
  EXPECT_EQ(formatter.format(make("srv1", "check_disk")), "nscp/srv1/passive");
}

TEST(ResultKeyFormatter, AnUnknownVariableFallsBackToTheDefault) {
  // Silently expanding an unknown variable to nothing would file every
  // result under the same key and quietly lose all but the last.
  result_key_formatter formatter;
  std::string error;
  EXPECT_FALSE(formatter.parse("${nonesuch}", error));
  EXPECT_NE(error.find("nonesuch"), std::string::npos);
  EXPECT_EQ(formatter.format(make("srv1", "check_disk")), "srv1/check_disk");
}

TEST(ResultKeyFormatter, AConstantExpressionIsAcceptedAsWritten) {
  // Not useful, but it is what the operator asked for, and the store's own
  // "empty key" guard is what catches the genuinely broken case.
  result_key_formatter formatter;
  std::string error;
  ASSERT_TRUE(formatter.parse("constant", error));
  EXPECT_EQ(formatter.format(make("srv1", "check_disk")), "constant");
}

// --- cache mode -------------------------------------------------------------

TEST(ResultStore, LastModeLetsARecoveryReplaceTheProblem) {
  result_store store;
  store.set_enabled(true);
  store.set_mode(result_store::mode_last);
  store.submit(make("srv1", "check_disk", 2), kNow);
  store.submit(make("srv1", "check_disk", 0), kNow + 60);

  result_store::result_entry entry;
  ASSERT_TRUE(store.get("srv1/check_disk", entry, kNow + 60));
  EXPECT_EQ(entry.status, 0);
}

TEST(ResultStore, WorstModeHoldsTheProblemThroughARecovery) {
  // The reason the mode exists: a CRITICAL that recovers before the next poll
  // must still be reported, not quietly overwritten by the OK behind it.
  result_store store;
  store.set_enabled(true);
  store.set_mode(result_store::mode_worst);
  store.submit(make("srv1", "check_disk", 2), kNow);
  store.submit(make("srv1", "check_disk", 0), kNow + 60);

  result_store::result_entry entry;
  ASSERT_TRUE(store.get("srv1/check_disk", entry, kNow + 60));
  EXPECT_EQ(entry.status, 2);
}

TEST(ResultStore, WorstModeStillEscalates) {
  result_store store;
  store.set_enabled(true);
  store.set_mode(result_store::mode_worst);
  store.submit(make("srv1", "check_disk", 1), kNow);
  store.submit(make("srv1", "check_disk", 2), kNow + 60);

  result_store::result_entry entry;
  ASSERT_TRUE(store.get("srv1/check_disk", entry, kNow + 60));
  EXPECT_EQ(entry.status, 2);
}

TEST(ResultStore, WorstModeRanksUnknownAboveCritical) {
  // Matches how the rest of NSClient++ aggregates a worst-of (a plain numeric
  // max over the Nagios status), so the two never disagree.
  result_store store;
  store.set_enabled(true);
  store.set_mode(result_store::mode_worst);
  store.submit(make("srv1", "check_disk", 2), kNow);
  store.submit(make("srv1", "check_disk", 3), kNow + 60);

  result_store::result_entry entry;
  ASSERT_TRUE(store.get("srv1/check_disk", entry, kNow + 60));
  EXPECT_EQ(entry.status, 3);
}

TEST(ResultStore, WorstModeTakesTheFresherOfTwoEquallySevereResults) {
  // Holding the severity does not mean holding a stale message: an equally
  // severe result still replaces, so the text stays current.
  result_store store;
  store.set_enabled(true);
  store.set_mode(result_store::mode_worst);
  result_store::result_entry first = make("srv1", "check_disk", 2);
  first.message = "old text";
  store.submit(first, kNow);
  result_store::result_entry second = make("srv1", "check_disk", 2);
  second.message = "new text";
  store.submit(second, kNow + 60);

  result_store::result_entry entry;
  ASSERT_TRUE(store.get("srv1/check_disk", entry, kNow + 60));
  EXPECT_EQ(entry.message, "new text");
  EXPECT_EQ(entry.result_seen, kNow + 60);
}

TEST(ResultStore, WorstModeCountsASuppressedResultAsAReport) {
  // The OK did not win, but the check did report - `age` and `count` have to
  // say so, or a held CRITICAL looks like a check that stopped running.
  result_store store;
  store.set_enabled(true);
  store.set_mode(result_store::mode_worst);
  store.submit(make("srv1", "check_disk", 2), kNow);
  store.submit(make("srv1", "check_disk", 0), kNow + 60);

  result_store::result_entry entry;
  ASSERT_TRUE(store.get("srv1/check_disk", entry, kNow + 60));
  EXPECT_EQ(entry.count, 2u);
  EXPECT_EQ(entry.first_seen, kNow);
  EXPECT_EQ(entry.last_seen, kNow + 60);
  // ...and the retained problem is still dated to when it actually happened.
  EXPECT_EQ(entry.result_seen, kNow);
}

TEST(ResultStore, WorstModeStartsOverAfterADrain) {
  // "Worst" means worst since the last poll, not worst ever - otherwise a
  // single CRITICAL would pin the check forever.
  result_store store;
  store.set_enabled(true);
  store.set_mode(result_store::mode_worst);
  store.submit(make("srv1", "check_disk", 2), kNow);
  ASSERT_EQ(store.drain(no_filter(), kNow).size(), 1u);

  store.submit(make("srv1", "check_disk", 0), kNow + 60);
  result_store::result_entry entry;
  ASSERT_TRUE(store.get("srv1/check_disk", entry, kNow + 60));
  EXPECT_EQ(entry.status, 0);
  EXPECT_EQ(entry.count, 1u);
}

TEST(ResultStore, InLastModeResultSeenTracksTheSubmission) {
  result_store store;
  store.set_enabled(true);
  store.submit(make("srv1", "check_disk", 2), kNow);
  store.submit(make("srv1", "check_disk", 0), kNow + 60);

  result_store::result_entry entry;
  ASSERT_TRUE(store.get("srv1/check_disk", entry, kNow + 60));
  EXPECT_EQ(entry.result_seen, kNow + 60);
  EXPECT_EQ(entry.last_seen, kNow + 60);
}

TEST(ResultStore, ParseModeAcceptsTheTwoNamesAndNothingElse) {
  result_store::cache_mode mode = result_store::mode_last;
  EXPECT_TRUE(result_store::parse_mode("worst", mode));
  EXPECT_EQ(mode, result_store::mode_worst);
  EXPECT_TRUE(result_store::parse_mode("last", mode));
  EXPECT_EQ(mode, result_store::mode_last);
  // Config files are written by hand, so be forgiving about case and space.
  EXPECT_TRUE(result_store::parse_mode("  WORST ", mode));
  EXPECT_EQ(mode, result_store::mode_worst);

  // An unrecognised value must not silently pick one: the caller logs and
  // falls back, and `mode` is left as it was.
  EXPECT_FALSE(result_store::parse_mode("newest", mode));
  EXPECT_EQ(mode, result_store::mode_worst);
  EXPECT_FALSE(result_store::parse_mode("", mode));
}

TEST(ResultStore, ModeNameRoundTrips) {
  result_store::cache_mode mode = result_store::mode_last;
  ASSERT_TRUE(result_store::parse_mode(result_store::mode_name(result_store::mode_worst), mode));
  EXPECT_EQ(mode, result_store::mode_worst);
  ASSERT_TRUE(result_store::parse_mode(result_store::mode_name(result_store::mode_last), mode));
  EXPECT_EQ(mode, result_store::mode_last);
}

// --- draining poll ----------------------------------------------------------

TEST(ResultStore, DrainReturnsWhatItRemoves) {
  result_store store;
  store.set_enabled(true);
  store.submit(make("srv1", "check_disk"), kNow);
  store.submit(make("srv2", "check_disk"), kNow);

  const result_store::result_list drained = store.drain(no_filter(), kNow);
  ASSERT_EQ(drained.size(), 2u);
  EXPECT_EQ(drained[0].key, "srv1/check_disk");
  EXPECT_EQ(drained[1].key, "srv2/check_disk");
  EXPECT_EQ(store.size(), 0u);
}

TEST(ResultStore, ASecondDrainSeesOnlyWhatArrivedSince) {
  result_store store;
  store.set_enabled(true);
  store.submit(make("srv1", "check_disk"), kNow);
  ASSERT_EQ(store.drain(no_filter(), kNow).size(), 1u);
  EXPECT_TRUE(store.drain(no_filter(), kNow).empty());

  store.submit(make("srv2", "check_disk"), kNow + 60);
  const result_store::result_list second = store.drain(no_filter(), kNow + 60);
  ASSERT_EQ(second.size(), 1u);
  EXPECT_EQ(second[0].key, "srv2/check_disk");
}

TEST(ResultStore, AFilteredDrainKeepsWhatItDidNotReturn) {
  // Otherwise a caller polling `?status=critical` would silently throw away
  // every OK it never even saw.
  result_store store;
  store.set_enabled(true);
  store.submit(make("srv1", "check_disk", 2), kNow);
  store.submit(make("srv2", "check_disk", 0), kNow);

  result_store::filter f;
  f.statuses.push_back(2);
  const result_store::result_list drained = store.drain(f, kNow);
  ASSERT_EQ(drained.size(), 1u);
  EXPECT_EQ(drained[0].key, "srv1/check_disk");

  EXPECT_EQ(store.size(), 1u);
  EXPECT_EQ(store.list(no_filter(), kNow)[0].key, "srv2/check_disk");
}

TEST(ResultStore, DrainDoesNotHandBackAnExpiredResult) {
  result_store store;
  store.set_enabled(true);
  store.set_max_age(60);
  store.submit(make("srv1", "check_disk"), kNow);

  EXPECT_TRUE(store.drain(no_filter(), kNow + 61).empty());
  EXPECT_EQ(store.size(), 0u);
}

TEST(ResultStore, DrainOnAnEmptyStoreIsEmptyRatherThanAnError) {
  result_store store;
  store.set_enabled(true);
  EXPECT_TRUE(store.drain(no_filter(), kNow).empty());
}
