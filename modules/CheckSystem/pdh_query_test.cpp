// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <win/pdh/pdh_enumerations.hpp>
#include <win/pdh/pdh_interface.hpp>
#include <win/pdh/pdh_object_gather.hpp>
#include <win/pdh/pdh_query.hpp>
#include <win/pdh/thread_safe_impl.hpp>

#include "pdh_counter_set.hpp"

namespace {

// Minimal mock for PDH::impl_interface. Each method has a knob to make it fail.
// Open/close are tracked so tests can assert no handle leaks.
class MockPdh : public PDH::impl_interface {
 public:
  // Knobs
  PDH_STATUS open_status = ERROR_SUCCESS;
  PDH_STATUS close_status = ERROR_SUCCESS;
  PDH_STATUS add_counter_status = ERROR_SUCCESS;
  PDH_STATUS remove_counter_status = ERROR_SUCCESS;
  PDH_STATUS formatted_value_status = ERROR_SUCCESS;
  PDH_STATUS expand_wild_card_status = ERROR_SUCCESS;
  PDH_STATUS collect_status = ERROR_SUCCESS;
  // Transient failures: the next N calls return the failing_* status, then
  // the call falls back to its permanent knob above. This is what a briefly
  // missing instance looks like (#648).
  int fail_next_collects = 0;
  PDH_STATUS failing_collect_status = PDH_NO_DATA;
  int fail_next_formatted_values = 0;
  PDH_STATUS failing_formatted_value_status = PDH_CSTATUS_NO_INSTANCE;
  bool throw_on_add_listener = false;
  // Counter paths that do not exist (yet): adding one fails as PDH does for
  // an object that is not registered.
  std::set<std::wstring> missing_paths;
  // What a successful wildcard expansion lists.
  std::list<std::wstring> expanded_paths;
  // Per-path override of formatted_value_status, for counters added after
  // it is set as well as before.
  std::map<std::wstring, PDH_STATUS> path_status;
  std::map<PDH::PDH_HCOUNTER, std::wstring> handle_paths;

  // Observables
  int listener_count = 0;
  int add_listener_calls = 0;
  int remove_listener_calls = 0;
  int open_calls = 0;
  int close_calls = 0;
  int add_counter_calls = 0;
  int add_english_counter_calls = 0;
  int remove_counter_calls = 0;
  int collect_calls = 0;
  int expand_wild_card_calls = 0;

  // The "open handle count" we track to assert no leaks.
  int open_handles = 0;
  int open_counter_handles = 0;

  // The thread-safe implementation's lock is what keeps a reload from
  // interleaving with a query, so every call a query makes has to arrive
  // with it held. Dropping a guard shows up here as an unlocked call.
  int lock_depth = 0;
  int unlocked_calls = 0;
  void lock() override { ++lock_depth; }
  void unlock() override { --lock_depth; }
  void note_call() {
    if (lock_depth == 0) ++unlocked_calls;
  }

  PDH::pdh_error PdhOpenQuery(LPCWSTR, DWORD_PTR, PDH::PDH_HQUERY *phQuery) override {
    ++open_calls;
    note_call();
    if (open_status != ERROR_SUCCESS) {
      *phQuery = nullptr;
      return {open_status};
    }
    // Return a fake non-null handle; we just need pointer identity to be tracked.
    ++open_handles;
    *phQuery = reinterpret_cast<PDH::PDH_HQUERY>(static_cast<intptr_t>(0x1000 + open_calls));
    return {};
  }
  PDH::pdh_error PdhCloseQuery(PDH::PDH_HQUERY) override {
    ++close_calls;
    note_call();
    if (close_status != ERROR_SUCCESS) return {close_status};
    --open_handles;
    return {};
  }
  PDH::pdh_error PdhAddCounter(PDH::PDH_HQUERY, LPCWSTR path, DWORD_PTR, PDH::PDH_HCOUNTER *phCounter) override {
    ++add_counter_calls;
    note_call();
    if (add_counter_status != ERROR_SUCCESS) {
      *phCounter = nullptr;
      return {add_counter_status};
    }
    if (missing_paths.count(path) > 0) {
      *phCounter = nullptr;
      return {static_cast<PDH_STATUS>(PDH_CSTATUS_NO_OBJECT)};
    }
    ++open_counter_handles;
    *phCounter = reinterpret_cast<PDH::PDH_HCOUNTER>(static_cast<intptr_t>(0x2000 + add_counter_calls));
    handle_paths[*phCounter] = path;
    return {};
  }
  // Tracked separately from PdhAddCounter so resolution-mode tests can assert
  // which API was used, but it shares add_counter_status as its failure knob so
  // existing "add fails" tests still fail through both paths.
  PDH::pdh_error PdhAddEnglishCounter(PDH::PDH_HQUERY, LPCWSTR path, DWORD_PTR, PDH::PDH_HCOUNTER *phCounter) override {
    ++add_english_counter_calls;
    note_call();
    if (add_counter_status != ERROR_SUCCESS) {
      *phCounter = nullptr;
      return {add_counter_status};
    }
    if (missing_paths.count(path) > 0) {
      *phCounter = nullptr;
      return {static_cast<PDH_STATUS>(PDH_CSTATUS_NO_OBJECT)};
    }
    ++open_counter_handles;
    *phCounter = reinterpret_cast<PDH::PDH_HCOUNTER>(static_cast<intptr_t>(0x3000 + add_english_counter_calls));
    handle_paths[*phCounter] = path;
    return {};
  }
  PDH::pdh_error PdhRemoveCounter(PDH::PDH_HCOUNTER hCounter) override {
    ++remove_counter_calls;
    note_call();
    if (remove_counter_status != ERROR_SUCCESS) return {remove_counter_status};
    --open_counter_handles;
    handle_paths.erase(hCounter);
    return {};
  }
  PDH::pdh_error PdhCollectQueryData(PDH::PDH_HQUERY) override {
    ++collect_calls;
    note_call();
    if (fail_next_collects > 0) {
      --fail_next_collects;
      return {failing_collect_status};
    }
    return {collect_status};
  }

  void add_listener(PDH::subscriber *) override {
    ++add_listener_calls;
    note_call();
    if (throw_on_add_listener) throw PDH::pdh_exception("mock: add_listener refused");
    ++listener_count;
  }
  void remove_listener(PDH::subscriber *) override {
    ++remove_listener_calls;
    note_call();
    --listener_count;
  }
  bool reload() override { return true; }

  // Methods we don't exercise — return success or empty.
  PDH::pdh_error PdhLookupPerfIndexByName(LPCTSTR, LPCTSTR, DWORD *) override { return {}; }
  PDH::pdh_error PdhLookupPerfNameByIndex(LPCTSTR, DWORD, LPTSTR, LPDWORD) override { return {}; }
  PDH::pdh_error PdhExpandCounterPath(LPCTSTR, LPTSTR, LPDWORD) override { return {}; }
  PDH::pdh_error PdhGetCounterInfo(PDH::PDH_HCOUNTER, BOOLEAN, LPDWORD, PDH_COUNTER_INFO *) override { return {}; }
  PDH::pdh_error PdhGetRawCounterValue(PDH::PDH_HCOUNTER, LPDWORD, PPDH_RAW_COUNTER) override { return {}; }
  PDH::pdh_error PdhGetFormattedCounterValue(PDH::PDH_HCOUNTER hCounter, DWORD, LPDWORD, PPDH_FMT_COUNTERVALUE) override {
    note_call();
    if (fail_next_formatted_values > 0) {
      --fail_next_formatted_values;
      return {failing_formatted_value_status};
    }
    const auto handle = handle_paths.find(hCounter);
    if (handle != handle_paths.end()) {
      const auto status = path_status.find(handle->second);
      if (status != path_status.end()) return {status->second};
    }
    return {formatted_value_status};
  }
  PDH::pdh_error PdhValidatePath(LPCWSTR, bool) override { return {}; }
  PDH::pdh_error PdhEnumObjects(LPCWSTR, LPCWSTR, LPWSTR, LPDWORD, DWORD, BOOL) override { return {}; }
  PDH::pdh_error PdhEnumObjectItems(LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, LPDWORD, LPWSTR, LPDWORD, DWORD, DWORD) override { return {}; }
  // Expansion is driven by expand_wild_card_status: on success the buffer
  // gets expanded_paths as a double-zero-terminated multi-string (empty by
  // default).
  PDH::pdh_error PdhExpandWildCardPath(LPCTSTR, LPCTSTR, LPWSTR mszExpandedPathList, LPDWORD pcchPathListLength, DWORD) override {
    ++expand_wild_card_calls;
    if (expand_wild_card_status != ERROR_SUCCESS) return {expand_wild_card_status};
    std::wstring list;
    for (const std::wstring &p : expanded_paths) {
      list += p;
      list.push_back(L'\0');
    }
    list.push_back(L'\0');
    if (list.size() < 2) list.push_back(L'\0');
    if (pcchPathListLength == nullptr) return {PDH_INVALID_ARGUMENT};
    if (mszExpandedPathList == nullptr || *pcchPathListLength < list.size()) {
      *pcchPathListLength = static_cast<DWORD>(list.size());
      return {PDH_MORE_DATA};
    }
    std::copy(list.begin(), list.end(), mszExpandedPathList);
    *pcchPathListLength = static_cast<DWORD>(list.size());
    return {};
  }
};

class PdhQueryLifecycleTest : public ::testing::Test {
 protected:
  std::shared_ptr<MockPdh> mock;

  void SetUp() override {
    mock = std::make_shared<MockPdh>();
    PDH::factory::set_impl(mock);
  }
  void TearDown() override { PDH::factory::set_impl(nullptr); }

  // Build a simple non-wildcard counter; this avoids any PDH enumeration calls
  // during construction.
  PDH::pdh_instance make_counter(const std::string &alias, const std::string &path, const std::string &resolution = "auto") {
    PDH::pdh_object obj;
    obj.set_alias(alias);
    obj.set_counter(path);
    obj.set_type(PDH::pdh_object::type_long);
    obj.set_resolution(resolution);
    return PDH::factory::create(obj);
  }
};

}  // namespace

// ----------------------------------------------------------------------------
// Idempotency / destructor safety
// ----------------------------------------------------------------------------

TEST_F(PdhQueryLifecycleTest, CloseOnNeverOpenedIsNoOp) {
  PDH::PDHQuery q;
  EXPECT_NO_THROW(q.close());
  EXPECT_EQ(mock->open_calls, 0);
  EXPECT_EQ(mock->close_calls, 0);
  EXPECT_EQ(mock->add_listener_calls, 0);
  EXPECT_EQ(mock->remove_listener_calls, 0);
}

TEST_F(PdhQueryLifecycleTest, DoubleCloseDoesNotThrow) {
  PDH::PDHQuery q;
  q.open();
  EXPECT_NO_THROW(q.close());
  EXPECT_NO_THROW(q.close());
  EXPECT_EQ(mock->open_handles, 0);
  EXPECT_EQ(mock->listener_count, 0);
}

TEST_F(PdhQueryLifecycleTest, DestructorOnOpenQueryDoesNotThrow) {
  {
    PDH::PDHQuery q;
    q.open();
    EXPECT_TRUE(q.is_open());
    // ~PDHQuery should close cleanly without leaking handles or listeners.
  }
  EXPECT_EQ(mock->open_handles, 0);
  EXPECT_EQ(mock->listener_count, 0);
}

TEST_F(PdhQueryLifecycleTest, DestructorSwallowsCloseErrors) {
  // PdhCloseQuery returning an error must not propagate out of ~PDHQuery.
  PDH::PDHQuery *q = new PDH::PDHQuery();
  q->open();
  mock->close_status = PDH_INVALID_HANDLE;
  EXPECT_NO_THROW(delete q);
  // Listener still removed even though PdhCloseQuery failed.
  EXPECT_EQ(mock->listener_count, 0);
}

TEST_F(PdhQueryLifecycleTest, RemoveAllCountersOnNeverOpenedIsSafe) {
  PDH::PDHQuery q;
  q.addCounter(make_counter("a", "\\Foo\\Bar"));
  EXPECT_TRUE(q.has_counters());
  EXPECT_NO_THROW(q.removeAllCounters());
  EXPECT_FALSE(q.has_counters());
  EXPECT_EQ(mock->open_calls, 0);
  EXPECT_EQ(mock->close_calls, 0);
}

// ----------------------------------------------------------------------------
// Listener registration is balanced — even on partial failure
// ----------------------------------------------------------------------------

TEST_F(PdhQueryLifecycleTest, OpenFailureLeavesNoListenerRegistered) {
  mock->open_status = PDH_CSTATUS_NO_OBJECT;
  PDH::PDHQuery q;
  EXPECT_THROW(q.open(), PDH::pdh_exception);
  EXPECT_FALSE(q.is_open());
  EXPECT_EQ(mock->add_listener_calls, 0) << "Listener must not be registered if PdhOpenQuery failed";
  EXPECT_EQ(mock->listener_count, 0);
  // Destructor must also be clean.
  EXPECT_EQ(mock->open_handles, 0);
}

TEST_F(PdhQueryLifecycleTest, AddCounterFailureRollsBackOpenedQuery) {
  // PdhOpenQuery succeeds, but PdhAddCounter (and the english/index retries)
  // all fail. on_reload must close the handle it just opened and leave hQuery_
  // null, AND not register the listener.
  PDH::PDHQuery q;
  q.addCounter(make_counter("a", "\\Foo\\Bar"));
  mock->add_counter_status = PDH_CSTATUS_NO_COUNTER;

  EXPECT_THROW(q.open(), PDH::pdh_exception);
  EXPECT_FALSE(q.is_open());
  EXPECT_EQ(mock->open_handles, 0) << "Opened query must be closed when addToQuery fails";
  EXPECT_EQ(mock->listener_count, 0) << "Listener must not be registered on partial open";
  EXPECT_EQ(mock->add_listener_calls, 0);
}

TEST_F(PdhQueryLifecycleTest, AddListenerFailureRollsBackOpenedQuery) {
  // PdhOpenQuery succeeds and counters add fine, but add_listener throws.
  // open() must tear down the query and propagate.
  PDH::PDHQuery q;
  mock->throw_on_add_listener = true;

  EXPECT_THROW(q.open(), PDH::pdh_exception);
  EXPECT_FALSE(q.is_open());
  EXPECT_EQ(mock->open_handles, 0);
  EXPECT_EQ(mock->listener_count, 0);
}

TEST_F(PdhQueryLifecycleTest, NormalOpenCloseBalancesListenerAndHandle) {
  PDH::PDHQuery q;
  q.addCounter(make_counter("a", "\\Foo\\Bar"));
  q.addCounter(make_counter("b", "\\Foo\\Baz"));

  q.open();
  EXPECT_TRUE(q.is_open());
  EXPECT_EQ(mock->listener_count, 1);
  EXPECT_EQ(mock->open_handles, 1);
  EXPECT_EQ(mock->open_counter_handles, 2);

  q.close();
  EXPECT_FALSE(q.is_open());
  EXPECT_EQ(mock->listener_count, 0);
  EXPECT_EQ(mock->open_handles, 0);
  EXPECT_EQ(mock->open_counter_handles, 0);
}

// ----------------------------------------------------------------------------
// State consistency after errors
// ----------------------------------------------------------------------------

TEST_F(PdhQueryLifecycleTest, OnUnloadNullsHandleBeforeThrowing) {
  // If PdhCloseQuery fails, hQuery_ should already be cleared so that a
  // subsequent close() / destructor does not try to double-close.
  PDH::PDHQuery q;
  q.open();
  mock->close_status = PDH_INVALID_HANDLE;

  EXPECT_THROW(q.close(), PDH::pdh_exception);
  EXPECT_FALSE(q.is_open()) << "hQuery_ must be cleared even when PdhCloseQuery fails";
  EXPECT_EQ(mock->listener_count, 0) << "Listener must still have been removed";

  // And calling close() again must NOT call PdhCloseQuery a second time.
  int close_calls_before = mock->close_calls;
  mock->close_status = ERROR_SUCCESS;
  EXPECT_NO_THROW(q.close());
  EXPECT_EQ(mock->close_calls, close_calls_before);
}

TEST_F(PdhQueryLifecycleTest, ReopenAfterCloseWorks) {
  PDH::PDHQuery q;
  q.addCounter(make_counter("a", "\\Foo\\Bar"));
  q.open();
  q.close();
  // After close() counters are cleared; re-adding and re-opening must succeed.
  q.addCounter(make_counter("a", "\\Foo\\Bar"));
  EXPECT_NO_THROW(q.open());
  EXPECT_TRUE(q.is_open());
  EXPECT_EQ(mock->listener_count, 1);
  EXPECT_EQ(mock->open_handles, 1);
}

TEST_F(PdhQueryLifecycleTest, OpenWhileAlreadyOpenThrows) {
  PDH::PDHQuery q;
  q.open();
  EXPECT_THROW(q.open(), PDH::pdh_exception);
  // Original handle/listener still valid — second open() must not have
  // perturbed state.
  EXPECT_EQ(mock->open_handles, 1);
  EXPECT_EQ(mock->listener_count, 1);
}

// ----------------------------------------------------------------------------
// Counter-name resolution mode (auto / english / index)
// ----------------------------------------------------------------------------

TEST_F(PdhQueryLifecycleTest, AutoResolutionPrefersLocalizedApi) {
  // Default resolution: the localized PdhAddCounter succeeds, so the English
  // API is never consulted.
  PDH::PDHQuery q;
  q.addCounter(make_counter("a", "\\Foo\\Bar"));
  q.open();
  EXPECT_EQ(mock->add_counter_calls, 1);
  EXPECT_EQ(mock->add_english_counter_calls, 0);
  EXPECT_EQ(mock->open_counter_handles, 1);
}

TEST_F(PdhQueryLifecycleTest, EnglishResolutionForcesEnglishApi) {
  // resolution=english must go straight to PdhAddEnglishCounter and never touch
  // the localized PdhAddCounter, regardless of locale.
  PDH::PDHQuery q;
  q.addCounter(make_counter("a", "\\Foo\\Bar", "english"));
  q.open();
  EXPECT_EQ(mock->add_english_counter_calls, 1);
  EXPECT_EQ(mock->add_counter_calls, 0);
  EXPECT_EQ(mock->open_counter_handles, 1);
}

TEST_F(PdhQueryLifecycleTest, IndexResolutionUsesLocalizedApi) {
  // resolution=index expands numeric indexes (a no-op for this digit-free path)
  // then adds via the localized PdhAddCounter — the English API is not used.
  PDH::PDHQuery q;
  q.addCounter(make_counter("a", "\\Foo\\Bar", "index"));
  q.open();
  EXPECT_EQ(mock->add_counter_calls, 1);
  EXPECT_EQ(mock->add_english_counter_calls, 0);
  EXPECT_EQ(mock->open_counter_handles, 1);
}

TEST_F(PdhQueryLifecycleTest, InvalidResolutionValueThrows) {
  PDH::pdh_object obj;
  obj.set_counter("\\Foo\\Bar");
  EXPECT_THROW(obj.set_resolution("nonsense"), PDH::pdh_exception);
}

// ----------------------------------------------------------------------------
// gatherData tolerance for uncomputable counters
// ----------------------------------------------------------------------------

TEST_F(PdhQueryLifecycleTest, GatherDataSkipsNegativeDenominatorWhenIgnoringErrors) {
  // Some counters simply cannot be computed at times (HTTP Service Request
  // Queues\MaxQueueItemAge on an idle queue persists past the retry). With
  // ignore_errors the counter must be skipped for this tick - readers fall
  // back to their default - not fail the whole gather, which the object
  // gathers then misreport as "the object does not exist on this host".
  PDH::PDHQuery q;
  q.addCounter(make_counter("age", "\\HTTP Service Request Queues\\MaxQueueItemAge"));
  q.open();
  mock->formatted_value_status = PDH_CALC_NEGATIVE_DENOMINATOR;
  EXPECT_NO_THROW(q.gatherData(true));
  q.close();
}

TEST_F(PdhQueryLifecycleTest, GatherDataStillThrowsOnNegativeDenominatorByDefault) {
  // Without ignore_errors the historical contract stays: a counter that
  // remains uncomputable after the retry raises, so single-counter callers
  // hear about it instead of silently reading a default.
  PDH::PDHQuery q;
  q.addCounter(make_counter("age", "\\HTTP Service Request Queues\\MaxQueueItemAge"));
  q.open();
  mock->formatted_value_status = PDH_CALC_NEGATIVE_DENOMINATOR;
  EXPECT_THROW(q.gatherData(false), PDH::pdh_exception);
  q.close();
}

// ----------------------------------------------------------------------------
// Wildcard expansion of an object that currently has no instances
// ----------------------------------------------------------------------------

// PDH_CSTATUS_NO_INSTANCE means the object resolved but has nothing to list
// right now (W3SVC_W3WP once the idle worker has spun down). The expansion
// reports it through its flag, not as an error: the IIS checks must answer
// "No IIS worker processes running", never "counters not available - is the
// role installed?". The collector, on the other hand, must keep seeing a
// failed counter so its boot-time retry (#634) is not cut short.

namespace {
// A $INSTANCE$ counter as the collector and the gather build it.
PDH::pdh_object wildcard_object(const std::string &alias, const std::string &path) {
  PDH::pdh_object obj;
  obj.set_alias(alias);
  obj.set_counter(path);
  obj.set_instances("true");
  obj.set_strategy_static();
  obj.set_type("double");
  obj.set_resolution("auto");
  return obj;
}
}  // namespace

TEST_F(PdhQueryLifecycleTest, ExpandWildCardWithNoInstancesIsEmptyNotError) {
  mock->expand_wild_card_status = PDH_CSTATUS_NO_INSTANCE;
  std::string err;
  bool no_instances = false;
  const std::list<std::string> paths = PDH::Enumerations::expand_wild_card_path("\\W3SVC_W3WP(*)\\Active Requests", err, no_instances);
  EXPECT_TRUE(paths.empty());
  EXPECT_EQ(err, "") << "an empty instance set must not surface as an expansion error";
  EXPECT_TRUE(no_instances);
  EXPECT_EQ(mock->expand_wild_card_calls, 1) << "no locale fallback is attempted for an object that resolved";
  EXPECT_EQ(mock->open_calls, 0);
  EXPECT_EQ(mock->add_counter_calls, 0);
  EXPECT_EQ(mock->add_english_counter_calls, 0);
}

TEST_F(PdhQueryLifecycleTest, ExpandWildCardWithNoInstancesOnLocalizedHostIsEmptyNotError) {
  // A localized host: the English path does not expand directly, so the
  // code resolves it through a temporary query, and the English-name add is
  // where the empty object first shows (NO_INSTANCE). That must come out the
  // same as the direct case, with the temporary query released.
  mock->expand_wild_card_status = PDH_CSTATUS_NO_COUNTER;
  mock->add_counter_status = PDH_CSTATUS_NO_INSTANCE;
  std::string err;
  bool no_instances = false;
  const std::list<std::string> paths = PDH::Enumerations::expand_wild_card_path("\\W3SVC_W3WP(*)\\Active Requests", err, no_instances);
  EXPECT_TRUE(paths.empty());
  EXPECT_EQ(err, "");
  EXPECT_TRUE(no_instances);
  EXPECT_EQ(mock->expand_wild_card_calls, 1);
  EXPECT_EQ(mock->open_calls, 1) << "the fallback goes through one temporary query";
  EXPECT_EQ(mock->add_english_counter_calls, 1);
  EXPECT_EQ(mock->close_calls, 1);
  EXPECT_EQ(mock->open_handles, 0) << "the temporary query must be released";
  EXPECT_EQ(mock->open_counter_handles, 0);
}

TEST_F(PdhQueryLifecycleTest, ExpandWildCardStillReportsMissingCounter) {
  // Guard the boundary: an object PDH cannot resolve at all stays an error
  // after the English-name fallback (which the mock also refuses), and the
  // temporary query of that fallback is released.
  mock->expand_wild_card_status = PDH_CSTATUS_NO_OBJECT;
  mock->add_counter_status = PDH_CSTATUS_NO_OBJECT;
  std::string err;
  bool no_instances = false;
  const std::list<std::string> paths = PDH::Enumerations::expand_wild_card_path("\\NoSuchObject(*)\\Counter", err, no_instances);
  EXPECT_TRUE(paths.empty());
  EXPECT_NE(err, "");
  EXPECT_FALSE(no_instances);
  EXPECT_EQ(mock->expand_wild_card_calls, 1);
  EXPECT_EQ(mock->open_calls, 1) << "the is_not_found() fallback must still try the English name";
  EXPECT_EQ(mock->add_english_counter_calls, 1);
  EXPECT_EQ(mock->close_calls, 1);
  EXPECT_EQ(mock->open_handles, 0);
  EXPECT_EQ(mock->open_counter_handles, 0);
}

TEST_F(PdhQueryLifecycleTest, ExpandWildCardTwoArgumentFormReportsNoInstancesAsError) {
  // Callers without the flag cannot tell an empty object from a missing one
  // and must keep getting the PDH status text.
  mock->expand_wild_card_status = PDH_CSTATUS_NO_INSTANCE;
  std::string err;
  const std::list<std::string> paths = PDH::Enumerations::expand_wild_card_path("\\W3SVC_W3WP(*)\\Active Requests", err);
  EXPECT_TRUE(paths.empty());
  EXPECT_NE(err.find("0x800007D1"), std::string::npos) << err;
}

TEST_F(PdhQueryLifecycleTest, FactoryCreateOnEmptyObjectThrowsNoInstance) {
  // The collector treats a counter it cannot create as not-yet-available and
  // retries with back-off; a childless container would instead count as
  // success and leave the counter publishing nothing for good.
  mock->expand_wild_card_status = PDH_CSTATUS_NO_INSTANCE;
  const PDH::pdh_object obj = wildcard_object("workers", "\\W3SVC_W3WP($INSTANCE$)\\Active Requests");
  EXPECT_THROW(PDH::factory::create(obj), PDH::pdh_no_instance_exception);
  try {
    PDH::factory::create(obj);
    FAIL() << "expected pdh_no_instance_exception";
  } catch (const PDH::pdh_exception &e) {
    // Caught through the base, as the collector's catch (std::exception) does.
    EXPECT_NE(e.reason().find("no instances"), std::string::npos) << e.reason();
    EXPECT_NE(e.reason().find("W3SVC_W3WP"), std::string::npos) << e.reason();
  }
}

TEST_F(PdhQueryLifecycleTest, FactoryCreateOnMissingObjectThrowsPlainPdhException) {
  mock->expand_wild_card_status = PDH_CSTATUS_NO_OBJECT;
  mock->add_counter_status = PDH_CSTATUS_NO_OBJECT;
  const PDH::pdh_object obj = wildcard_object("nothing", "\\NoSuchObject($INSTANCE$)\\Counter");
  EXPECT_THROW(PDH::factory::create(obj), PDH::pdh_exception);
  try {
    PDH::factory::create(obj);
  } catch (const PDH::pdh_no_instance_exception &) {
    FAIL() << "a missing object is not an empty one";
  } catch (const PDH::pdh_exception &) {
  }
}

TEST_F(PdhQueryLifecycleTest, GatherObjectInstancesWithNoInstancesIsEmptyMap) {
  mock->expand_wild_card_status = PDH_CSTATUS_NO_INSTANCE;
  PDH::object_instance_values out;
  EXPECT_NO_THROW(out = PDH::gather_object_instances("W3SVC_W3WP", {"Active Requests", "Total HTTP Requests Served"}, false));
  EXPECT_TRUE(out.empty());
  // Nothing to sample, so no query is opened and nothing is collected.
  EXPECT_EQ(mock->open_calls, 0);
  EXPECT_EQ(mock->collect_calls, 0);
  EXPECT_EQ(mock->open_handles, 0);
  EXPECT_EQ(mock->listener_count, 0);
}

TEST_F(PdhQueryLifecycleTest, GatherObjectInstancesOnMissingObjectStillThrows) {
  // The role-not-installed contract of the IIS checks rides on this throw.
  mock->expand_wild_card_status = PDH_CSTATUS_NO_OBJECT;
  mock->add_counter_status = PDH_CSTATUS_NO_OBJECT;
  EXPECT_THROW(PDH::gather_object_instances("W3SVC_W3WP", {"Active Requests"}, false), PDH::pdh_exception);
  EXPECT_EQ(mock->open_handles, 0);
}

TEST_F(PdhQueryLifecycleTest, CollectOnQueryWithoutCountersIsANoOp) {
  // PdhCollectQueryData on a counter-less query fails with PDH_NO_DATA; an
  // empty query has nothing to sample, so neither collect() nor gatherData()
  // asks PDH.
  PDH::PDHQuery q;
  q.open();
  EXPECT_NO_THROW(q.collect());
  EXPECT_NO_THROW(q.gatherData(false));
  EXPECT_EQ(mock->collect_calls, 0);
  q.close();
  EXPECT_EQ(mock->open_handles, 0);
}

// ----------------------------------------------------------------------------
// PDH_NO_DATA from PdhCollectQueryData (#648)
// ----------------------------------------------------------------------------

TEST_F(PdhQueryLifecycleTest, CollectWithNoDataThrowsNoDataExceptionNamingTheCounter) {
  // PdhAddCounter accepts a path whose instance does not exist right now; it
  // only surfaces as PDH_NO_DATA from the collect. The report has to say which
  // counter it was about - it used to read "PdhCollectQueryData failed: : ...".
  PDH::PDHQuery q;
  q.addCounter(make_counter("queue", "\\PhysicalDisk(4 C: F:)\\Avg. Disk Queue Length"));
  q.open();
  mock->collect_status = PDH_NO_DATA;
  try {
    q.gatherData(false);
    FAIL() << "gatherData did not throw";
  } catch (const PDH::pdh_no_data_exception &e) {
    const std::string what = e.what();
    EXPECT_NE(what.find("\\PhysicalDisk(4 C: F:)\\Avg. Disk Queue Length"), std::string::npos) << what;
    EXPECT_NE(what.find("0x800007D5"), std::string::npos) << what;
    EXPECT_EQ(what.find(": :"), std::string::npos) << what;
  }
  q.close();
  EXPECT_EQ(mock->open_handles, 0);
}

TEST_F(PdhQueryLifecycleTest, CollectWithNoDataIsStillAPdhException) {
  // Every caller that treats a failed collect as a failure keeps doing so;
  // only gatherData() singles the no-data case out, to take one more sample
  // when its caller asks for that (retry_missing).
  PDH::PDHQuery q;
  q.addCounter(make_counter("queue", "\\PhysicalDisk(_Total)\\Avg. Disk Queue Length"));
  q.open();
  mock->collect_status = PDH_NO_DATA;
  EXPECT_THROW(q.collect(), PDH::pdh_exception);
  q.close();
}

TEST_F(PdhQueryLifecycleTest, OtherCollectFailuresAreNotReportedAsNoData) {
  PDH::PDHQuery q;
  q.addCounter(make_counter("queue", "\\PhysicalDisk(_Total)\\Avg. Disk Queue Length"));
  q.open();
  mock->collect_status = PDH_INVALID_HANDLE;
  try {
    q.collect();
    FAIL() << "collect did not throw";
  } catch (const PDH::pdh_no_data_exception &) {
    FAIL() << "PDH_INVALID_HANDLE reported as no data";
  } catch (const PDH::pdh_exception &e) {
    const std::string what = e.what();
    EXPECT_NE(what.find("\\PhysicalDisk(_Total)\\Avg. Disk Queue Length"), std::string::npos) << what;
  }
  q.close();
}

TEST_F(PdhQueryLifecycleTest, CollectFailureListsOnlyTheFirstFewCounters) {
  // A wildcard can expand to hundreds of instances; the message names a few.
  PDH::PDHQuery q;
  for (int i = 0; i < 5; ++i) {
    q.addCounter(make_counter("c" + std::to_string(i), "\\Foo\\Bar" + std::to_string(i)));
  }
  q.open();
  mock->collect_status = PDH_NO_DATA;
  try {
    q.collect();
    FAIL() << "collect did not throw";
  } catch (const PDH::pdh_no_data_exception &e) {
    const std::string what = e.what();
    EXPECT_NE(what.find("\\Foo\\Bar0, \\Foo\\Bar1, \\Foo\\Bar2 and 2 more"), std::string::npos) << what;
    EXPECT_EQ(what.find("\\Foo\\Bar3"), std::string::npos) << what;
  }
  q.close();
}

TEST_F(PdhQueryLifecycleTest, GatherDataRetriesOnceWhenTheQueryHadNoData) {
  // The instance was missing for one sample and is back on the next.
  PDH::PDHQuery q;
  q.addCounter(make_counter("queue", "\\PhysicalDisk(4 C: F:)\\Avg. Disk Queue Length"));
  q.open();
  mock->fail_next_collects = 1;
  EXPECT_NO_THROW(q.gatherData(false, true));
  EXPECT_EQ(mock->collect_calls, 2);
  EXPECT_EQ(mock->fail_next_collects, 0);
  q.close();
}

TEST_F(PdhQueryLifecycleTest, GatherDataRetriesNoDataOnlyOnce) {
  PDH::PDHQuery q;
  q.addCounter(make_counter("queue", "\\PhysicalDisk(4 C: F:)\\Avg. Disk Queue Length"));
  q.open();
  mock->fail_next_collects = 2;
  EXPECT_THROW(q.gatherData(false, true), PDH::pdh_no_data_exception);
  EXPECT_EQ(mock->collect_calls, 2);
  q.close();
}

TEST_F(PdhQueryLifecycleTest, GatherDataDoesNotRetryMissingInstancesByDefault) {
  // The retry is opt-in: the 1 Hz collector cannot spend the second, and the
  // other callers read counters whose instances do not come and go.
  PDH::PDHQuery q;
  q.addCounter(make_counter("queue", "\\PhysicalDisk(4 C: F:)\\Avg. Disk Queue Length"));
  q.open();
  mock->fail_next_collects = 1;
  EXPECT_THROW(q.gatherData(), PDH::pdh_no_data_exception);
  EXPECT_EQ(mock->collect_calls, 1);
  mock->collect_calls = 0;
  mock->fail_next_formatted_values = 1;
  EXPECT_THROW(q.gatherData(), PDH::pdh_exception);
  EXPECT_EQ(mock->collect_calls, 1);
  q.close();
}

TEST_F(PdhQueryLifecycleTest, GatherDataRetriesACounterWhoseInstanceIsMissing) {
  // PdhCollectQueryData succeeds for the query, but one counter's instance is
  // gone for this sample: that surfaces per counter, as NO_INSTANCE.
  PDH::PDHQuery q;
  q.addCounter(make_counter("queue", "\\PhysicalDisk(4 C: F:)\\Avg. Disk Queue Length"));
  q.open();
  mock->fail_next_formatted_values = 1;
  EXPECT_NO_THROW(q.gatherData(false, true));
  EXPECT_EQ(mock->collect_calls, 2);
  q.close();
}

TEST_F(PdhQueryLifecycleTest, GatherDataReportsACounterWhoseInstanceStaysMissing) {
  PDH::PDHQuery q;
  q.addCounter(make_counter("queue", "\\PhysicalDisk(4 C: F:)\\Avg. Disk Queue Length"));
  q.open();
  mock->formatted_value_status = PDH_CSTATUS_NO_INSTANCE;
  try {
    q.gatherData(false, true);
    FAIL() << "gatherData did not throw";
  } catch (const PDH::pdh_exception &e) {
    const std::string what = e.what();
    EXPECT_NE(what.find("\\PhysicalDisk(4 C: F:)\\Avg. Disk Queue Length"), std::string::npos) << what;
  }
  EXPECT_EQ(mock->collect_calls, 2);
  q.close();
}

TEST_F(PdhQueryLifecycleTest, GatherDataTakesOneExtraSampleForAllMissingInstances) {
  // Several counters missing their instance cost one extra second, not one
  // each.
  PDH::PDHQuery q;
  for (int i = 0; i < 3; ++i) {
    q.addCounter(make_counter("c" + std::to_string(i), "\\PhysicalDisk(" + std::to_string(i) + ")\\Avg. Disk Queue Length"));
  }
  q.open();
  mock->formatted_value_status = PDH_CSTATUS_NO_INSTANCE;
  EXPECT_NO_THROW(q.gatherData(true, true));
  EXPECT_EQ(mock->collect_calls, 2);
  q.close();
}

TEST_F(PdhQueryLifecycleTest, NoDataResampleIsTheOnlyExtraSample) {
  // The PDH_NO_DATA resample already gave every counter its second sample,
  // so one that then reads INVALID_DATA is not given a third.
  PDH::PDHQuery q;
  q.addCounter(make_counter("a", "\\PhysicalDisk(1)\\% Disk Time"));
  q.addCounter(make_counter("b", "\\PhysicalDisk(2)\\% Disk Time"));
  q.open();
  mock->fail_next_collects = 1;
  mock->fail_next_formatted_values = 1;
  mock->failing_formatted_value_status = PDH_INVALID_DATA;
  EXPECT_NO_THROW(q.gatherData(false, true));
  EXPECT_EQ(mock->collect_calls, 2);
  q.close();
}

// ----------------------------------------------------------------------------
// $INSTANCE$ counters whose instances come and go
// ----------------------------------------------------------------------------

// An RDS host with User Profile Disks attaches a PhysicalDisk per signed-in
// user and detaches it at sign-out. The collector expands the default
// disk_queue_length counter once at setup; a departed disk's counter then
// returns INVALID_DATA for good, which used to cost a one-second retry on
// every 1 Hz tick, and a disk attached later was never collected.

namespace {
const wchar_t *const disk_c = L"\\PhysicalDisk(0 C:)\\% Disk Time";
const wchar_t *const disk_1 = L"\\PhysicalDisk(1)\\% Disk Time";
const wchar_t *const disk_2 = L"\\PhysicalDisk(2)\\% Disk Time";
const wchar_t *const disk_3 = L"\\PhysicalDisk(3)\\% Disk Time";

std::list<std::string> instance_names(const PDH::pdh_instance &parent) {
  std::list<std::string> ret;
  for (const PDH::pdh_instance &child : parent->get_instances()) ret.push_back(child->get_name());
  return ret;
}

PDH::PDHQuery::instance_changes reexpand(PDH::PDHQuery &q) {
  std::list<std::string> errors;
  const PDH::PDHQuery::expansion_list expansions = q.expand_instances(errors);
  for (const std::string &e : errors) ADD_FAILURE() << "unexpected expansion error: " << e;
  return q.apply_instances(expansions);
}
}  // namespace

TEST_F(PdhQueryLifecycleTest, VanishedInstanceCostsNoSleepAndIsDroppedOnReexpansion) {
  mock->expanded_paths = {disk_c, disk_1, disk_2};
  const PDH::pdh_instance disks = PDH::factory::create(wildcard_object("disk", "\\PhysicalDisk($INSTANCE$)\\% Disk Time"));
  ASSERT_EQ(instance_names(disks), (std::list<std::string>{"disk_0 C:", "disk_1", "disk_2"}));
  PDH::PDHQuery q;
  q.addCounter(disks);
  q.open();
  EXPECT_TRUE(q.has_instance_counters());
  q.gatherData(false);

  // Two users sign out.
  mock->path_status[disk_1] = PDH_INVALID_DATA;
  mock->path_status[disk_2] = PDH_INVALID_DATA;

  // The first gather gives them a second sample - one, shared by both.
  int before = mock->collect_calls;
  EXPECT_NO_THROW(q.gatherData(false));
  EXPECT_EQ(mock->collect_calls - before, 2) << "one re-collect for every counter without data, not one each";

  // After that they cost nothing: no retry, so no sleep.
  for (int tick = 0; tick < 3; ++tick) {
    before = mock->collect_calls;
    const auto start = std::chrono::steady_clock::now();
    EXPECT_NO_THROW(q.gatherData(false));
    EXPECT_EQ(mock->collect_calls - before, 1) << "a counter that had no data on the previous gather is not retried";
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::milliseconds(500));
  }
  EXPECT_TRUE(q.has_stale_counters());

  // The next expansion no longer lists them.
  mock->expanded_paths = {disk_c};
  const PDH::PDHQuery::instance_changes changes = reexpand(q);
  EXPECT_TRUE(changes.added.empty());
  EXPECT_TRUE(changes.reopened.empty());
  ASSERT_EQ(changes.removed.size(), 2u);
  EXPECT_EQ(changes.removed.front(), std::make_pair(std::string("disk"), std::string("disk_1")));
  EXPECT_EQ(changes.removed.back(), std::make_pair(std::string("disk"), std::string("disk_2")));
  EXPECT_EQ(instance_names(disks), (std::list<std::string>{"disk_0 C:"}));
  EXPECT_EQ(q.counters_.size(), 1u);
  EXPECT_EQ(mock->open_counter_handles, 1) << "the departed instances' counters are removed from the query";
  EXPECT_FALSE(q.has_stale_counters());

  q.close();
  EXPECT_EQ(mock->open_counter_handles, 0);
  EXPECT_EQ(mock->open_handles, 0);
  EXPECT_EQ(mock->unlocked_calls, 0);
}

TEST_F(PdhQueryLifecycleTest, InstanceThatAppearsIsAddedOnReexpansion) {
  mock->expanded_paths = {disk_c};
  const PDH::pdh_instance disks = PDH::factory::create(wildcard_object("disk", "\\PhysicalDisk($INSTANCE$)\\% Disk Time"));
  PDH::PDHQuery q;
  q.addCounter(disks);
  q.open();
  q.gatherData(false);
  const PDH::pdh_instance kept = disks->get_instances().front();

  // A user signs in.
  mock->expanded_paths = {disk_c, disk_3};
  const PDH::PDHQuery::instance_changes changes = reexpand(q);
  EXPECT_TRUE(changes.removed.empty());
  ASSERT_EQ(changes.added.size(), 1u);
  EXPECT_EQ(changes.added.front(), std::make_pair(std::string("disk"), std::string("disk_3")));
  EXPECT_EQ(instance_names(disks), (std::list<std::string>{"disk_0 C:", "disk_3"}));
  EXPECT_EQ(disks->get_instances().front(), kept) << "an instance still listed keeps its collector, and an rrd counter its history";
  EXPECT_EQ(q.counters_.size(), 2u);
  EXPECT_EQ(mock->open_counter_handles, 2) << "the new instance is added to the open query";

  // Its first gather has no previous sample to compute a rate from; that
  // must not stall the tick either - the next tick is its second sample.
  mock->path_status[disk_3] = PDH_INVALID_DATA;
  int before = mock->collect_calls;
  EXPECT_NO_THROW(q.gatherData(false));
  EXPECT_EQ(mock->collect_calls - before, 1);
  mock->path_status.clear();
  EXPECT_NO_THROW(q.gatherData(false));
  EXPECT_FALSE(q.has_stale_counters());

  // Expanding again with nothing changed changes nothing.
  const PDH::PDHQuery::instance_changes again = reexpand(q);
  EXPECT_TRUE(again.added.empty());
  EXPECT_TRUE(again.removed.empty());
  EXPECT_TRUE(again.reopened.empty());
  EXPECT_EQ(q.counters_.size(), 2u);

  q.close();
  EXPECT_EQ(mock->open_counter_handles, 0);
  EXPECT_EQ(mock->unlocked_calls, 0);
}

TEST_F(PdhQueryLifecycleTest, StaleInstanceThatIsStillListedIsReadded) {
  // Disk numbers are reused: the next user to sign in can get the number of
  // the one who left, so the instance is listed again under the same path.
  // Its counter is bound afresh rather than left on the old handle.
  mock->expanded_paths = {disk_c, disk_1};
  const PDH::pdh_instance disks = PDH::factory::create(wildcard_object("disk", "\\PhysicalDisk($INSTANCE$)\\% Disk Time"));
  PDH::PDHQuery q;
  q.addCounter(disks);
  q.open();
  const PDH::pdh_instance reused = disks->get_instances().back();
  mock->path_status[disk_1] = PDH_INVALID_DATA;
  for (int tick = 0; tick < PDH::PDHQuery::stale_after_ticks; ++tick) q.gatherData(false);
  ASSERT_TRUE(q.has_stale_counters());

  const int adds_before = mock->add_counter_calls;
  const PDH::PDHQuery::instance_changes changes = reexpand(q);
  EXPECT_TRUE(changes.added.empty());
  EXPECT_TRUE(changes.removed.empty());
  ASSERT_EQ(changes.reopened.size(), 1u);
  EXPECT_EQ(changes.reopened.front().second, "disk_1");
  EXPECT_EQ(mock->add_counter_calls - adds_before, 1);
  EXPECT_EQ(disks->get_instances().back(), reused);
  EXPECT_EQ(q.counters_.size(), 2u);
  EXPECT_EQ(mock->open_counter_handles, 2);
  EXPECT_FALSE(q.has_stale_counters());
  q.close();
  EXPECT_EQ(mock->open_counter_handles, 0);
}

TEST_F(PdhQueryLifecycleTest, RejectedWildcardAddIsNotKeptForReexpansion) {
  // The collector retries a predefined counter PDH rejects every minute
  // (#634). Each rejected attempt must leave nothing behind for re-expansion
  // to find, and the one that succeeds registers the counter once.
  mock->expanded_paths = {disk_c, disk_1};
  const PDH::pdh_object disk_object = wildcard_object("disk", "\\PhysicalDisk($INSTANCE$)\\% Disk Time");
  PDH::PDHQuery q;
  q.open();
  mock->add_counter_status = PDH_CSTATUS_NO_OBJECT;
  for (int attempt = 0; attempt < 3; ++attempt) {
    EXPECT_THROW(q.addCounter(PDH::factory::create(disk_object)), PDH::pdh_exception);
  }
  EXPECT_FALSE(q.has_instance_counters());
  EXPECT_TRUE(q.counters_.empty());

  mock->add_counter_status = ERROR_SUCCESS;
  const PDH::pdh_instance disks = PDH::factory::create(disk_object);
  q.addCounter(disks);
  ASSERT_EQ(q.containers_.size(), 1u);
  EXPECT_EQ(q.counters_.size(), 2u);

  mock->expanded_paths = {disk_c};
  EXPECT_EQ(reexpand(q).removed.size(), 1u);
  EXPECT_EQ(instance_names(disks), (std::list<std::string>{"disk_0 C:"}));
  q.close();
  EXPECT_EQ(mock->open_counter_handles, 0);
  EXPECT_EQ(mock->unlocked_calls, 0);
}

TEST_F(PdhQueryLifecycleTest, FailedReexpansionKeepsTheCurrentInstances) {
  mock->expanded_paths = {disk_c, disk_1};
  const PDH::pdh_instance disks = PDH::factory::create(wildcard_object("disk", "\\PhysicalDisk($INSTANCE$)\\% Disk Time"));
  PDH::PDHQuery q;
  q.addCounter(disks);
  q.open();

  // A failed enumeration says nothing about which instances are gone.
  mock->expand_wild_card_status = PDH_CSTATUS_NO_OBJECT;
  mock->add_counter_status = PDH_CSTATUS_NO_OBJECT;
  std::list<std::string> errors;
  const PDH::PDHQuery::expansion_list expansions = q.expand_instances(errors);
  EXPECT_TRUE(expansions.empty());
  ASSERT_EQ(errors.size(), 1u);
  EXPECT_EQ(errors.front().rfind("disk: ", 0), 0u) << errors.front();
  q.apply_instances(expansions);
  EXPECT_EQ(instance_names(disks), (std::list<std::string>{"disk_0 C:", "disk_1"}));
  EXPECT_EQ(q.counters_.size(), 2u);

  // An object that resolves with no instances at all has lost all of them.
  mock->expand_wild_card_status = PDH_CSTATUS_NO_INSTANCE;
  mock->add_counter_status = ERROR_SUCCESS;
  const PDH::PDHQuery::instance_changes changes = reexpand(q);
  EXPECT_EQ(changes.removed.size(), 2u);
  EXPECT_TRUE(disks->get_instances().empty());
  EXPECT_TRUE(q.counters_.empty());
  EXPECT_EQ(mock->open_counter_handles, 0);
  EXPECT_NO_THROW(q.gatherData(false)) << "a query whose instances are all gone samples nothing";

  // And picks them up again when they come back.
  mock->expand_wild_card_status = ERROR_SUCCESS;
  mock->expanded_paths = {disk_c};
  EXPECT_EQ(reexpand(q).added.size(), 1u);
  EXPECT_EQ(mock->open_counter_handles, 1);
  q.close();
  EXPECT_EQ(mock->open_counter_handles, 0);
  EXPECT_EQ(mock->open_handles, 0);
}

// ----------------------------------------------------------------------------
// Reload safety
// ----------------------------------------------------------------------------

TEST_F(PdhQueryLifecycleTest, EveryQueryCallIsMadeUnderTheImplementationLock) {
  PDH::PDHQuery q;
  q.addCounter(make_counter("a", "\\Foo\\Bar"));
  q.addCounter(make_counter("b", "\\Foo\\Baz", "english"));
  q.open();
  q.gatherData(false);
  q.close();
  EXPECT_EQ(mock->add_counter_calls, 1);
  EXPECT_EQ(mock->add_english_counter_calls, 1);
  EXPECT_GT(mock->collect_calls, 0);
  EXPECT_EQ(mock->unlocked_calls, 0) << "a query call made without the lock races a concurrent reload";
  EXPECT_EQ(mock->lock_depth, 0) << "every lock taken was released";
}

TEST_F(PdhQueryLifecycleTest, QueryLeftClosedByAFailedReloadReopensOnNextGather) {
  // What ThreadedSafePDH::reload() does to a subscriber, with the reopen
  // failing: the query is closed but still subscribed. It must recover on
  // its own once PDH works again, not fail every sample until the next reload.
  PDH::PDHQuery q;
  q.addCounter(make_counter("a", "\\Foo\\Bar"));
  q.open();
  q.on_unload();
  mock->open_status = PDH_CSTATUS_NO_OBJECT;
  EXPECT_THROW(q.on_reload(), PDH::pdh_exception);
  EXPECT_FALSE(q.is_open());

  mock->open_status = ERROR_SUCCESS;
  EXPECT_NO_THROW(q.gatherData(false));
  EXPECT_TRUE(q.is_open());
  EXPECT_EQ(mock->open_handles, 1);
  EXPECT_EQ(mock->open_counter_handles, 1);
  q.close();
  EXPECT_EQ(mock->open_handles, 0);
  EXPECT_EQ(mock->open_counter_handles, 0);
  EXPECT_EQ(mock->listener_count, 0);
}

TEST_F(PdhQueryLifecycleTest, ACounterThatFailedToRemoveDoesNotWedgeTheReopen) {
  // A reload closes every query (on_unload) and reopens it; on_unload carries
  // on past a counter whose remove fails. That counter used to keep its
  // handle, so re-adding it refused ("already opened") and the reopened
  // query failed on every sample from then on.
  PDH::PDHQuery q;
  q.addCounter(make_counter("a", "\\Foo\\Bar"));
  q.open();
  mock->remove_counter_status = PDH_INVALID_HANDLE;
  q.on_unload();
  mock->remove_counter_status = ERROR_SUCCESS;

  EXPECT_NO_THROW(q.gatherData(false));
  EXPECT_TRUE(q.is_open());
  q.close();
  EXPECT_EQ(mock->open_handles, 0);
}

TEST_F(PdhQueryLifecycleTest, CloseWithACounterThatFailsToRemoveDoesNotThrowFromADestructor) {
  // close() clears the counters after on_unload() has given up on one; the
  // counter's destructor then removed the stale handle again, and a throw
  // out of a destructor aborts the agent.
  PDH::PDHQuery q;
  q.addCounter(make_counter("a", "\\Foo\\Bar"));
  q.open();
  mock->remove_counter_status = PDH_INVALID_HANDLE;
  EXPECT_NO_THROW(q.close());
  EXPECT_FALSE(q.is_open());
}

namespace {
// Counts its callbacks and can be told to fail one of them.
struct recording_subscriber : PDH::subscriber {
  std::string name;
  int unloads = 0;
  int reloads = 0;
  bool fail_unload = false;
  bool fail_reload = false;
  explicit recording_subscriber(std::string name) : name(std::move(name)) {}
  void on_unload() override {
    ++unloads;
    if (fail_unload) throw PDH::pdh_exception(name + " unload failed");
  }
  void on_reload() override {
    ++reloads;
    if (fail_reload) throw PDH::pdh_exception(name + " reload failed");
  }
};
}  // namespace

TEST(ThreadedSafePdh, ReloadCallsEverySubscriberBackAndReportsEveryFailure) {
  // Stopping at the first failure left the subscribers already unloaded
  // closed, with nothing to reopen them, and dropped the other errors.
  PDH::ThreadedSafePDH pdh;
  recording_subscriber a("a"), b("b"), c("c");
  a.fail_unload = true;
  c.fail_reload = true;
  pdh.add_listener(&a);
  pdh.add_listener(&b);
  pdh.add_listener(&c);
  try {
    pdh.reload();
    ADD_FAILURE() << "expected the callback failures to be reported";
  } catch (const PDH::pdh_exception &e) {
    EXPECT_NE(e.reason().find("a unload failed"), std::string::npos) << e.reason();
    EXPECT_NE(e.reason().find("c reload failed"), std::string::npos) << e.reason();
  }
  for (const recording_subscriber *s : {&a, &b, &c}) {
    EXPECT_EQ(s->unloads, 1) << s->name;
    EXPECT_EQ(s->reloads, 1) << s->name;
  }
  pdh.remove_listener(&a);
  pdh.remove_listener(&b);
  pdh.remove_listener(&c);

  // With nothing failing, a reload is quiet.
  recording_subscriber d("d");
  pdh.add_listener(&d);
  EXPECT_NO_THROW(pdh.reload());
  EXPECT_EQ(d.unloads, 1);
  EXPECT_EQ(d.reloads, 1);
  pdh.remove_listener(&d);
}

TEST(ThreadedSafePdh, InstancesShareTheLockAndTheSubscribers) {
  // CheckSystem replaces the factory's instance on every module load, and a
  // query stays bound to the one it opened on, while the proc table is
  // static. A reload through the newer instance has to exclude, and call
  // back, the queries on the older one.
  PDH::ThreadedSafePDH older, newer;
  recording_subscriber query("query");
  older.add_listener(&query);
  EXPECT_NO_THROW(newer.reload());
  EXPECT_EQ(query.unloads, 1);
  EXPECT_EQ(query.reloads, 1);
  older.remove_listener(&query);

  older.lock();
  std::atomic<bool> acquired{false};
  std::thread other([&] {
    newer.lock();
    acquired = true;
    newer.unlock();
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_FALSE(acquired) << "the newer instance's lock did not exclude a holder of the older one's";
  older.unlock();
  other.join();
  EXPECT_TRUE(acquired);
}

// ----------------------------------------------------------------------------
// Predefined counters: one missing counter must not take the others down, and
// must be picked up once it exists (#634)
// ----------------------------------------------------------------------------

namespace {
PDH::pdh_object make_config(const std::string &alias, const std::string &path) {
  PDH::pdh_object obj;
  obj.set_alias(alias);
  obj.set_counter(path);
  obj.set_type(PDH::pdh_object::type_long);
  obj.set_strategy_static();
  return obj;
}
}  // namespace

TEST_F(PdhQueryLifecycleTest, CounterSetStartsWithEveryCounterUnresolved) {
  pdh_counters::counter_set set;
  set.configure({make_config("a", "\\A\\Value"), make_config("b", "\\B\\Value")});
  EXPECT_EQ(set.unresolved().size(), 2u);
  EXPECT_FALSE(set.complete());
}

TEST_F(PdhQueryLifecycleTest, CounterSetMissingCounterDoesNotFailTheOthers) {
  mock->missing_paths.insert(L"\\Service\\Jobs");
  pdh_counters::counter_set set;
  set.configure({make_config("cpu", "\\Processor(_Total)\\% Processor Time"), make_config("jobs", "\\Service\\Jobs")});

  PDH::PDHQuery q;
  q.open();
  const std::list<pdh_counters::resolved_counter> added = set.retry(q);
  ASSERT_EQ(added.size(), 1u);
  EXPECT_EQ(added.front().config.alias, "cpu");
  ASSERT_EQ(set.unresolved().size(), 1u);
  EXPECT_EQ(set.unresolved().front().config.alias, "jobs");
  EXPECT_FALSE(set.unresolved().front().error.empty());

  // The query stayed open with the counter that exists in it, and the one PDH
  // rejected left no handle behind.
  EXPECT_TRUE(q.is_open());
  EXPECT_EQ(mock->open_counter_handles, 1);
  q.close();
  EXPECT_EQ(mock->open_counter_handles, 0);
  EXPECT_EQ(mock->open_handles, 0);
}

TEST_F(PdhQueryLifecycleTest, CounterSetAddsALateCounterWithoutRebuildingTheQuery) {
  mock->missing_paths.insert(L"\\Service\\Jobs");
  pdh_counters::counter_set set;
  set.configure({make_config("cpu", "\\Processor(_Total)\\% Processor Time"), make_config("jobs", "\\Service\\Jobs")});
  PDH::PDHQuery q;
  q.open();
  ASSERT_EQ(set.retry(q).size(), 1u);

  // Still missing: a retry changes nothing.
  EXPECT_TRUE(set.retry(q).empty());
  EXPECT_EQ(set.unresolved().size(), 1u);

  // The service registered its counter.
  mock->missing_paths.clear();
  const std::list<pdh_counters::resolved_counter> added = set.retry(q);
  ASSERT_EQ(added.size(), 1u);
  EXPECT_EQ(added.front().config.alias, "jobs");
  EXPECT_TRUE(set.complete());
  // The counter already in the query keeps its handle: the query was neither
  // reopened nor stripped, so its previous sample survives.
  EXPECT_EQ(mock->open_calls, 1);
  EXPECT_EQ(mock->remove_counter_calls, 0);
  EXPECT_EQ(mock->open_counter_handles, 2);
  q.close();
}

TEST_F(PdhQueryLifecycleTest, CounterSetRetriesAWildcardItCannotExpandYet) {
  mock->expand_wild_card_status = PDH_INVALID_PATH;
  pdh_counters::counter_set set;
  set.configure({make_config("jobs", "\\Service($INSTANCE$)\\Jobs")});
  PDH::PDHQuery q;
  q.open();
  EXPECT_TRUE(set.retry(q).empty());
  ASSERT_EQ(set.unresolved().size(), 1u);
  EXPECT_EQ(set.unresolved().front().config.alias, "jobs");
  q.close();
}

TEST_F(PdhQueryLifecycleTest, AddCounterToAnOpenQueryAddsItNow) {
  PDH::PDHQuery q;
  q.open();
  q.addCounter(make_counter("a", "\\A\\Value"));
  EXPECT_EQ(mock->open_counter_handles, 1);
  EXPECT_TRUE(q.has_counters());
  q.close();
  EXPECT_EQ(mock->open_counter_handles, 0);
}

TEST_F(PdhQueryLifecycleTest, AddCounterRejectedByAnOpenQueryIsNotKept) {
  mock->missing_paths.insert(L"\\Service\\Jobs");
  PDH::PDHQuery q;
  q.open();
  EXPECT_THROW(q.addCounter(make_counter("jobs", "\\Service\\Jobs")), PDH::pdh_exception);
  EXPECT_FALSE(q.has_counters());
  EXPECT_EQ(mock->open_counter_handles, 0);
  // A PDH reload re-adds what the query holds: the rejected counter is not in
  // it, even once it would be accepted.
  mock->missing_paths.clear();
  q.on_unload();
  q.on_reload();
  EXPECT_EQ(mock->open_counter_handles, 0);
  q.close();
}

// A reload that cannot reopen a query leaves it closed but still subscribed.
static void leave_closed_by_a_failed_reload(PDH::PDHQuery &q, MockPdh &mock) {
  q.on_unload();
  mock.open_status = PDH_INVALID_HANDLE;
  EXPECT_THROW(q.on_reload(), PDH::pdh_exception);
  mock.open_status = ERROR_SUCCESS;
  ASSERT_FALSE(q.is_open());
}

TEST_F(PdhQueryLifecycleTest, AddCounterReopensAQueryAFailedReloadLeftClosed) {
  PDH::PDHQuery q;
  q.open();
  leave_closed_by_a_failed_reload(q, *mock);
  q.addCounter(make_counter("a", "\\A\\Value"));
  EXPECT_TRUE(q.is_open());
  EXPECT_EQ(mock->open_counter_handles, 1) << "the counter must have been added to PDH, not just kept";
  EXPECT_EQ(mock->add_listener_calls, 1);
  q.close();
}

TEST_F(PdhQueryLifecycleTest, AddCounterKeepsNothingWhenAFailedReloadQueryCannotReopen) {
  PDH::PDHQuery q;
  q.open();
  leave_closed_by_a_failed_reload(q, *mock);
  mock->open_status = PDH_INVALID_HANDLE;
  EXPECT_THROW(q.addCounter(make_counter("a", "\\A\\Value")), PDH::pdh_exception);
  EXPECT_FALSE(q.has_counters()) << "an unvalidated counter would fail every later reload";
  mock->open_status = ERROR_SUCCESS;
  q.close();
}

TEST_F(PdhQueryLifecycleTest, OpenOnAQueryAFailedReloadLeftClosedDoesNotSubscribeTwice) {
  PDH::PDHQuery q;
  q.addCounter(make_counter("a", "\\A\\Value"));
  q.open();
  leave_closed_by_a_failed_reload(q, *mock);
  EXPECT_NO_THROW(q.open());
  EXPECT_TRUE(q.is_open());
  EXPECT_EQ(mock->add_listener_calls, 1);
  EXPECT_EQ(mock->listener_count, 1);
  EXPECT_EQ(mock->open_counter_handles, 1);
  q.close();
  EXPECT_EQ(mock->listener_count, 0);
}

TEST(PdhCounterRetrySchedule, BacksOffThenSettlesOnTheSteadyCadence) {
  EXPECT_EQ(pdh_counters::retry_schedule::delay_ms(0), 1000u);
  EXPECT_EQ(pdh_counters::retry_schedule::delay_ms(1), 2000u);
  EXPECT_EQ(pdh_counters::retry_schedule::delay_ms(5), 30000u);
  EXPECT_EQ(pdh_counters::retry_schedule::delay_ms(pdh_counters::retry_schedule::fast_attempts), pdh_counters::retry_schedule::steady_delay_ms);
  EXPECT_EQ(pdh_counters::retry_schedule::delay_ms(1000), pdh_counters::retry_schedule::steady_delay_ms);
}
