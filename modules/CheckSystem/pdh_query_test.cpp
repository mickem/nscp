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

#include <gtest/gtest.h>

#include <memory>
#include <win/pdh/pdh_interface.hpp>
#include <win/pdh/pdh_query.hpp>

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
  bool throw_on_add_listener = false;

  // Observables
  int listener_count = 0;
  int add_listener_calls = 0;
  int remove_listener_calls = 0;
  int open_calls = 0;
  int close_calls = 0;
  int add_counter_calls = 0;
  int remove_counter_calls = 0;

  // The "open handle count" we track to assert no leaks.
  int open_handles = 0;
  int open_counter_handles = 0;

  PDH::pdh_error PdhOpenQuery(LPCWSTR, DWORD_PTR, PDH::PDH_HQUERY *phQuery) override {
    ++open_calls;
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
    if (close_status != ERROR_SUCCESS) return {close_status};
    --open_handles;
    return {};
  }
  PDH::pdh_error PdhAddCounter(PDH::PDH_HQUERY, LPCWSTR, DWORD_PTR, PDH::PDH_HCOUNTER *phCounter) override {
    ++add_counter_calls;
    if (add_counter_status != ERROR_SUCCESS) {
      *phCounter = nullptr;
      return {add_counter_status};
    }
    ++open_counter_handles;
    *phCounter = reinterpret_cast<PDH::PDH_HCOUNTER>(static_cast<intptr_t>(0x2000 + add_counter_calls));
    return {};
  }
  PDH::pdh_error PdhAddEnglishCounter(PDH::PDH_HQUERY q, LPCWSTR p, DWORD_PTR u, PDH::PDH_HCOUNTER *c) override { return PdhAddCounter(q, p, u, c); }
  PDH::pdh_error PdhRemoveCounter(PDH::PDH_HCOUNTER) override {
    ++remove_counter_calls;
    if (remove_counter_status != ERROR_SUCCESS) return {remove_counter_status};
    --open_counter_handles;
    return {};
  }
  PDH::pdh_error PdhCollectQueryData(PDH::PDH_HQUERY) override { return {}; }

  void add_listener(PDH::subscriber *) override {
    ++add_listener_calls;
    if (throw_on_add_listener) throw PDH::pdh_exception("mock: add_listener refused");
    ++listener_count;
  }
  void remove_listener(PDH::subscriber *) override {
    ++remove_listener_calls;
    --listener_count;
  }
  bool reload() override { return true; }

  // Methods we don't exercise — return success or empty.
  PDH::pdh_error PdhLookupPerfIndexByName(LPCTSTR, LPCTSTR, DWORD *) override { return {}; }
  PDH::pdh_error PdhLookupPerfNameByIndex(LPCTSTR, DWORD, LPTSTR, LPDWORD) override { return {}; }
  PDH::pdh_error PdhExpandCounterPath(LPCTSTR, LPTSTR, LPDWORD) override { return {}; }
  PDH::pdh_error PdhGetCounterInfo(PDH::PDH_HCOUNTER, BOOLEAN, LPDWORD, PDH_COUNTER_INFO *) override { return {}; }
  PDH::pdh_error PdhGetRawCounterValue(PDH::PDH_HCOUNTER, LPDWORD, PPDH_RAW_COUNTER) override { return {}; }
  PDH::pdh_error PdhGetFormattedCounterValue(PDH::PDH_HCOUNTER, DWORD, LPDWORD, PPDH_FMT_COUNTERVALUE) override { return {}; }
  PDH::pdh_error PdhValidatePath(LPCWSTR, bool) override { return {}; }
  PDH::pdh_error PdhEnumObjects(LPCWSTR, LPCWSTR, LPWSTR, LPDWORD, DWORD, BOOL) override { return {}; }
  PDH::pdh_error PdhEnumObjectItems(LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, LPDWORD, LPWSTR, LPDWORD, DWORD, DWORD) override { return {}; }
  PDH::pdh_error PdhExpandWildCardPath(LPCTSTR, LPCTSTR, LPWSTR, LPDWORD, DWORD) override { return {}; }
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
  PDH::pdh_instance make_counter(const std::string &alias, const std::string &path) {
    PDH::pdh_object obj;
    obj.set_alias(alias);
    obj.set_counter(path);
    obj.set_type(PDH::pdh_object::type_long);
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
