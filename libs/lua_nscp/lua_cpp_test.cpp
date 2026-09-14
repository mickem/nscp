// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Tests for the lua_wrapper accessors that the C bindings call while a Lua
// call is in flight. Lua is compiled as C, so these run between lua_pcall's
// setjmp and the C function it dispatched to: an exception thrown here would
// unwind through C frames, and an error raised here longjmps over ours. Both
// of those rule out anything clever, which is what these tests pin down.

#include <gtest/gtest.h>

#include <lua/lua_cpp.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <string>

// Normally provided by NSC_WRAP_DLL(); the wrapper's logging macros need it.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

// Runs `fn` the way Lua runs a registered binding: dispatched from inside
// lua_pcall. Returns true when the call completed, false when it raised;
// `result` is the value left on the stack, or the error message.
bool call_protected(lua_CFunction fn, std::string &result) {
  lua::Lua_State state;
  lua_State *L = state.get_state();
  lua_pushcfunction(L, fn);
  const bool ok = lua_pcall(L, 0, 1, 0) == LUA_OK;
  const char *top = lua_tostring(L, -1);
  result = top == nullptr ? std::string() : std::string(top);
  return ok;
}

}  // namespace

TEST(LuaWrapper, AnErrorMessageIsTextAndNotAFormat) {
  // The message routinely quotes something the script passed in, so it can
  // carry a %. Handing it to luaL_error as the format made it consume
  // arguments nobody pushed; the text has to arrive whole instead.
  std::string message;
  const bool ok = call_protected(
      [](lua_State *L) {
        lua::lua_wrapper instance(L);
        return instance.error("a %s %d %p b");
      },
      message);

  EXPECT_FALSE(ok) << "error() must raise: " << message;
  EXPECT_NE(message.find("a %s %d %p b"), std::string::npos) << message;
}
