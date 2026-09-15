// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The two halves of LUAScript's threading model, in isolation.
//
// `lua_gil` is what stops two threads being inside the interpreter at the same
// time, and `lua_thread` is what stops two invocations sharing one execution
// stack when the GIL is legitimately dropped around a core call. The
// integration suite (tests/plugin-threading.test.ts) proves the pair works in a
// live agent - by killing it when it does not - but it cannot say which half
// broke, and it cannot reach the depth arithmetic at all. These do both.

#include <gtest/gtest.h>

#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/thread/thread.hpp>
#include <lua/lua_core.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

// Unit-test binaries get no generated module glue, so the plugin singleton that
// NSC_WRAP_DLL() normally provides has to be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

/// True if another thread can take the GIL within `patience`.
///
/// The probe runs on another thread on purpose: the mutex is recursive, so a
/// try_lock from the thread that already holds it succeeds and would report the
/// very bug under test as fixed. It polls with try_lock rather than blocking so
/// it always returns by the deadline - a blocked probe left behind would take
/// the GIL the moment the test dropped it, and read a `taken` that no longer
/// exists.
bool another_thread_can_take_the_gil(boost::posix_time::time_duration patience) {
  bool taken = false;
  boost::thread probe([&taken, patience]() {
    const boost::system_time deadline = boost::get_system_time() + patience;
    do {
      if (lua::lua_gil::mutex().try_lock()) {
        taken = true;
        lua::lua_gil::mutex().unlock();
        return;
      }
      boost::this_thread::sleep(boost::posix_time::milliseconds(5));
    } while (boost::get_system_time() < deadline);
  });
  probe.join();
  return taken;
}

/// For the "yes it can" direction: generous, and returns as soon as it does.
bool the_gil_is_available() { return another_thread_can_take_the_gil(boost::posix_time::seconds(5)); }

/// For the "no it cannot" direction: this one always waits the whole time, so
/// keep it short. A GIL that is genuinely held stays held for far longer.
bool the_gil_is_held() { return !another_thread_can_take_the_gil(boost::posix_time::milliseconds(300)); }

class gil_test : public ::testing::Test {
 protected:
  // Every test must leave the GIL as it found it, or the next one hangs.
  void TearDown() override { EXPECT_EQ(0u, lua::lua_gil::depth()); }
};

TEST_F(gil_test, a_guard_records_its_depth) {
  EXPECT_EQ(0u, lua::lua_gil::depth());
  lua::lua_gil::guard outer;
  EXPECT_EQ(1u, lua::lua_gil::depth());
  {
    lua::lua_gil::guard inner;
    EXPECT_EQ(2u, lua::lua_gil::depth());
  }
  EXPECT_EQ(1u, lua::lua_gil::depth());
}

TEST_F(gil_test, release_unwinds_every_level_this_thread_holds) {
  // Depth 2 is what a script querying a command its own module serves reaches:
  // on_query takes the GIL, the core dispatches straight back in, and on_query
  // takes it again on the same thread. A release that unlocked once left the
  // recursive mutex still held, so the core call it wrapped ran with the
  // interpreter locked against every other thread - the release did nothing.
  lua::lua_gil::guard outer;
  lua::lua_gil::guard inner;
  ASSERT_EQ(2u, lua::lua_gil::depth());

  {
    lua::lua_gil::release unlocked;
    EXPECT_EQ(0u, lua::lua_gil::depth());
    EXPECT_TRUE(the_gil_is_available());
  }

  EXPECT_EQ(2u, lua::lua_gil::depth());
  // ...and it really is back, not merely counted back.
  EXPECT_TRUE(the_gil_is_held());
}

TEST_F(gil_test, a_guard_taken_inside_a_release_stays_balanced) {
  // A handler that runs on this thread while it is parked in the core - the
  // depth it sees is its own, and unwinding it must not reach past the release.
  lua::lua_gil::guard outer;
  {
    lua::lua_gil::release unlocked;
    {
      lua::lua_gil::guard reentrant;
      EXPECT_EQ(1u, lua::lua_gil::depth());
      {
        lua::lua_gil::release inner_release;
        EXPECT_EQ(0u, lua::lua_gil::depth());
        EXPECT_TRUE(the_gil_is_available());
      }
      EXPECT_EQ(1u, lua::lua_gil::depth());
    }
    EXPECT_EQ(0u, lua::lua_gil::depth());
  }
  EXPECT_EQ(1u, lua::lua_gil::depth());
}

TEST_F(gil_test, release_without_a_guard_is_a_no_op) {
  // Unlocking a recursive mutex this thread does not own is undefined
  // behaviour, and in practice wedges every later lock. A release that finds no
  // depth must do nothing at all instead.
  ASSERT_EQ(0u, lua::lua_gil::depth());
  { lua::lua_gil::release unlocked; }
  EXPECT_EQ(0u, lua::lua_gil::depth());
  EXPECT_TRUE(the_gil_is_available());
}

/// Number of entries in the registry table, so a per-invocation leak shows up.
int registry_entries(lua_State *L) {
  int count = 0;
  lua_pushnil(L);
  while (lua_next(L, LUA_REGISTRYINDEX) != 0) {
    ++count;
    lua_pop(L, 1);
  }
  return count;
}

class lua_thread_test : public ::testing::Test {
 protected:
  void SetUp() override {
    L = luaL_newstate();
    ASSERT_NE(nullptr, L);
    luaL_openlibs(L);
  }
  void TearDown() override { lua_close(L); }
  lua_State *L = nullptr;
};

TEST_F(lua_thread_test, gives_each_invocation_a_stack_of_its_own) {
  // The property the whole fix rests on. Before it, both of these were the
  // parent's stack and the second push landed on top of the first caller's
  // live frame.
  lua_pushinteger(L, 11);
  const int parent_top = lua_gettop(L);

  lua::lua_thread first(L);
  lua::lua_thread second(L);
  EXPECT_NE(first.state(), second.state());
  EXPECT_NE(first.state(), L);

  lua_pushstring(first.state(), "alpha");
  lua_pushstring(second.state(), "beta");

  EXPECT_EQ(1, lua_gettop(first.state()));
  EXPECT_EQ(1, lua_gettop(second.state()));
  EXPECT_STREQ("alpha", lua_tostring(first.state(), -1));
  EXPECT_STREQ("beta", lua_tostring(second.state(), -1));

  // And the parent is exactly as it was: creating a coroutine pushes onto it
  // and luaL_ref pops it again, so a parent that is itself mid-call is safe.
  EXPECT_EQ(parent_top, lua_gettop(L));
  EXPECT_EQ(11, lua_tointeger(L, -1));
}

TEST_F(lua_thread_test, shares_globals_and_the_registry_with_its_parent) {
  // The other half: sharing is what makes a coroutine the right answer rather
  // than a second lua_State. A script's globals, its `require`d modules and the
  // refs its registered functions live under all have to resolve.
  ASSERT_EQ(LUA_OK, luaL_dostring(L, "shared = 'from parent'"));

  lua::lua_thread thread(L);
  lua_getglobal(thread.state(), "shared");
  EXPECT_STREQ("from parent", lua_tostring(thread.state(), -1));

  lua_pushstring(thread.state(), "via coroutine");
  const int ref = luaL_ref(thread.state(), LUA_REGISTRYINDEX);
  lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
  EXPECT_STREQ("via coroutine", lua_tostring(L, -1));
  luaL_unref(L, LUA_REGISTRYINDEX, ref);
}

TEST_F(lua_thread_test, survives_a_collection_while_it_is_live) {
  // on_query ends every call with a full collection, and a concurrent caller's
  // coroutine is live across it. Nothing but the registry anchor keeps it
  // reachable.
  lua::lua_thread thread(L);
  lua_pushstring(thread.state(), "still here");

  lua_gc(L, LUA_GCCOLLECT, 0);

  EXPECT_EQ(1, lua_gettop(thread.state()));
  EXPECT_STREQ("still here", lua_tostring(thread.state(), -1));
}

TEST_F(lua_thread_test, releases_its_registry_anchor) {
  // One coroutine per check means a leaked anchor is a leak per check. luaL_ref
  // hands a freed slot straight back out, so a balanced unref shows up as a
  // registry that stops growing: ten invocations and a thousand must leave it
  // the same size. The constant the first round adds is luaL_ref's own
  // bookkeeping (its freelist head, plus the one slot in rotation).
  const int before = registry_entries(L);
  for (int i = 0; i < 10; i++) {
    lua::lua_thread thread(L);
    lua_pushinteger(thread.state(), i);
  }
  lua_gc(L, LUA_GCCOLLECT, 0);
  const int after_ten = registry_entries(L);

  for (int i = 0; i < 1000; i++) {
    lua::lua_thread thread(L);
    lua_pushinteger(thread.state(), i);
  }
  lua_gc(L, LUA_GCCOLLECT, 0);

  EXPECT_EQ(after_ten, registry_entries(L));
  EXPECT_LE(after_ten - before, 2);
}

}  // namespace
