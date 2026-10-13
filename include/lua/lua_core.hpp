// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <boost/thread/recursive_mutex.hpp>
#include <list>
#include <lua/lua_script.hpp>
#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <scripts/script_interface.hpp>
#include <string>
#include <utility>

namespace lua {
typedef scripts::script_information<lua_traits> script_information;

// A process-wide "GIL" that serialises all Lua execution. The Lua runtime shares
// a single lua_State across threads with no internal locking, so any handler that
// fires on another thread (a Scheduler tick, an NSCA inbox delivery, a relayed
// sub-query arriving on a server thread) would otherwise corrupt the interpreter
// if it ran while the main script is mid-Lua. Every entry into Lua holds the GIL
// for the duration of its pcall (guard) and releases it only around blocking /
// re-entrant core calls and nscp.sleep (release) - the same model as CPython's
// GIL. A recursive mutex keeps a missed release on a same-thread re-entry from
// self-deadlocking; the genuine cross-thread re-entry points (the query/submit
// family) always release explicitly.
struct lua_gil {
  static boost::recursive_mutex &mutex();
  struct guard {
    guard() { mutex().lock(); }
    ~guard() { mutex().unlock(); }
  };
  struct release {
    release() { mutex().unlock(); }
    ~release() { mutex().lock(); }
  };
};

struct lua_runtime_plugin {
  virtual void load(lua::lua_wrapper &instance) = 0;
  virtual void unload(lua::lua_wrapper &instance) = 0;
};
typedef std::shared_ptr<lua_runtime_plugin> lua_runtime_plugin_type;

struct lua_runtime : public scripts::script_runtime_interface<lua::lua_traits> {
  // The scripts folder (${scripts}): require() resolves from its lua/lib and
  // lua subfolders.
  std::string base_path;
  std::list<lua_runtime_plugin_type> plugins;

  explicit lua_runtime(std::string scripts_path) : base_path(std::move(scripts_path)) {}

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

  // The stack one call into a script runs on: a Lua thread of its own, sharing
  // the script's globals and registry, anchored in the registry for as long as
  // the call lasts. Construct and destroy it under the GIL.
  //
  // Calls used to run on the script's main lua_State. The GIL is released while
  // a handler sleeps or calls back into the core, and another thread may then
  // enter the same script - a second NRPE request, a scheduler tick - which
  // pushed its frames onto the stack the parked call was still running on. Two
  // concurrent nscp.sleep calls in one script were enough to crash the agent
  // with an access violation inside lua.dll.
  class call_state {
   public:
    explicit call_state(const lua::script_information *information) : main_(information->user_data.L) {
      L = lua_newthread(main_);
      // Pops the thread off the main stack and keeps it alive until unref.
      ref_ = luaL_ref(main_, LUA_REGISTRYINDEX);
    }
    ~call_state() { luaL_unref(main_, LUA_REGISTRYINDEX, ref_); }
    call_state(const call_state &) = delete;
    call_state &operator=(const call_state &) = delete;
    lua_State *L;

   private:
    lua_State *main_;
    int ref_;
  };

  static lua_State *prep_function(lua_State *L, const lua::lua_traits::function_type &c) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, c.function_ref);
    if (c.object_ref != 0) lua_rawgeti(L, LUA_REGISTRYINDEX, c.object_ref);
    return L;
  }
  static lua_State *prep_function(lua_State *L, const std::string &f) {
    lua_getglobal(L, f.c_str());
    return L;
  }
  void create_user_data(script_information *info);
};
}  // namespace lua