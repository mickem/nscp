// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

extern "C" {
#include <lua.h>
}

#include <boost/thread/recursive_mutex.hpp>

#include <list>
#include <lua/lua_script.hpp>
#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <scripts/script_interface.hpp>
#include <string>

namespace lua {
typedef scripts::script_information<lua_traits> script_information;

// A "GIL" that serialises Lua execution. A script's lua_State is reached from
// several threads with no internal locking, so any handler that fires on
// another thread (a Scheduler tick, an NSCA inbox delivery, a relayed sub-query
// arriving on a server thread) would otherwise corrupt the interpreter while
// another is mid-Lua. Every entry into Lua holds the GIL for the duration of
// its pcall (guard) and releases it only around blocking / re-entrant core
// calls and nscp.sleep (release) - the same model as CPython's GIL. A recursive
// mutex keeps a missed release on a same-thread re-entry from self-deadlocking;
// the genuine cross-thread re-entry points (the query/submit family) always
// release explicitly.
//
// Scope: one mutex per copy of this library, and each module that embeds it
// (LUAScript, CheckMKServer, CheckMKClient) builds its own lua_States, so the
// lock always covers every state that can be reached through it. Whether two
// modules end up sharing one mutex is a link-time detail and neither answer
// changes that: a script crossing from one module to the other goes through the
// core, which releases first.
//
// What the GIL does NOT do is give an invocation a private stack - see
// lua_thread for that half.
struct lua_gil {
  static boost::recursive_mutex &mutex();
  /// How many times *this* thread currently holds the GIL. Maintained by
  /// `guard`, read by `release`.
  static unsigned &depth();
  struct guard {
    guard() {
      mutex().lock();
      ++depth();
    }
    ~guard() {
      // Defensive: a depth that has already been zeroed (by a release whose
      // destructor was skipped) must not wrap round, or the next release
      // would try to unlock UINT_MAX times and hang.
      if (depth() > 0) --depth();
      mutex().unlock();
    }
    guard(const guard &) = delete;
    guard &operator=(const guard &) = delete;
  };
  /// Drops the GIL for the duration of a blocking or re-entrant core call and
  /// takes it back on the way out.
  ///
  /// It drops it *entirely*, and that is the whole point. The mutex is
  /// recursive, so a single unlock on a thread that entered Lua twice - a
  /// script querying a command its own module serves re-enters on_query on the
  /// same thread - leaves the GIL still held: the release silently becomes a
  /// no-op and no other thread runs while this one sits in the core. Unwinding
  /// by the recorded depth makes it exact.
  ///
  /// Zeroing the depth while released keeps a guard/release pair *inside* the
  /// released region balanced, and makes a release with no guard above it a
  /// no-op rather than an unlock of a mutex this thread does not own, which is
  /// undefined behaviour.
  struct release {
    release();
    ~release();
    release(const release &) = delete;
    release &operator=(const release &) = delete;

   private:
    unsigned held_;
  };
};

/// One Lua execution stack per invocation.
///
/// Every script gets a single lua_State (`script_information::user_data.L`)
/// and that state's stack used to be where every invocation of every function
/// in the script pushed its arguments and ran its pcall. With the GIL dropped
/// around core calls (`lua_gil::release`) a second thread could enter the same
/// script while the first was parked inside the core, and the two then drove
/// one Lua stack: the parked call's locals read back as nil and the process
/// died with SIGSEGV or a Lua PANIC. Four concurrent self-querying checks were
/// enough, and it reproduced on an unmodified tree, so it long predates the
/// dispatch-concurrency work.
///
/// A coroutine made with `lua_newthread` has its own stack but shares the
/// globals, the registry and the heap with the state it came from, which is
/// exactly the split needed: the per-invocation stack stops being shared, and
/// everything still shared is only ever touched with the GIL held. It is the
/// same shape as CPython giving each OS thread its own PyThreadState.
///
/// The coroutine is anchored in the registry with `luaL_ref` for its whole
/// lifetime: nothing else refers to it once `lua_newthread`'s return value is
/// popped, and a collection during the call would free it underneath us.
/// Construction and destruction both touch the parent state's stack and the
/// registry, so they must happen with the GIL held - declare the guard first.
class lua_thread {
 public:
  explicit lua_thread(const lua::script_information *information);
  explicit lua_thread(lua_State *parent);
  ~lua_thread();
  lua_thread(const lua_thread &) = delete;
  lua_thread &operator=(const lua_thread &) = delete;

  lua_State *state() const { return state_; }
  operator lua_State *() const { return state_; }

 private:
  lua_State *parent_;
  lua_State *state_;
  int ref_;
};

struct lua_runtime_plugin {
  virtual void load(lua::lua_wrapper &instance) = 0;
  virtual void unload(lua::lua_wrapper &instance) = 0;
};
typedef std::shared_ptr<lua_runtime_plugin> lua_runtime_plugin_type;

struct lua_runtime : public scripts::script_runtime_interface<lua::lua_traits> {
  std::string base_path;
  std::list<lua_runtime_plugin_type> plugins;

  lua_runtime(std::string base_path) : base_path(base_path) {}

  virtual void register_query(const std::string &command, const std::string &description);
  virtual void register_subscription(const std::string &channel, const std::string &description);

  virtual void on_query(std::string command, script_information *information, lua::lua_traits::function_type function, bool simple,
                        const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                        const PB::Commands::QueryRequestMessage &request_message);
  virtual void on_exec(std::string command, script_information *information, lua::lua_traits::function_type function, bool simple,
                       const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response,
                       const PB::Commands::ExecuteRequestMessage &request_message);
  virtual void on_submit(std::string channel, script_information *information, lua::lua_traits::function_type function, bool simple,
                         const PB::Commands::QueryResponseMessage::Response &request, PB::Commands::SubmitResponseMessage::Response *response);
  virtual void exec_main(script_information *information, const std::vector<std::string> &opts, PB::Commands::ExecuteResponseMessage::Response *response);

  virtual void load(scripts::script_information<lua_traits> *info);
  virtual void start(scripts::script_information<lua_traits> *info);
  virtual void unload(scripts::script_information<lua_traits> *info);

  void register_plugin(lua_runtime_plugin_type plugin) { plugins.push_back(plugin); }

  // Pushes the function (and its bound self, if any) onto `thread`'s own stack.
  // The refs live in the registry, which every coroutine off the script's state
  // shares, so a per-invocation stack resolves them just as the parent does.
  static lua_State *prep_function(const lua_thread &thread, const lua::lua_traits::function_type &c) {
    lua_State *L = thread.state();
    lua_rawgeti(L, LUA_REGISTRYINDEX, c.function_ref);
    if (c.object_ref != 0) lua_rawgeti(L, LUA_REGISTRYINDEX, c.object_ref);
    return L;
  }
  static lua_State *prep_function(const lua_thread &thread, const std::string &f) {
    lua_State *L = thread.state();
    lua_getglobal(L, f.c_str());
    return L;
  }
  void create_user_data(script_information *info);
};
}  // namespace lua