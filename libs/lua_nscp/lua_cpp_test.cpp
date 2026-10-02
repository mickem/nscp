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

TEST(LuaWrapper, ANonNumericStringArgumentReadsAsZero) {
  // get_int used the throwing lexical_cast: a script passing a word where a
  // number belongs - Settings():get_int(path, key, "n/a") - threw through
  // lua_pcall's C frames. Unconvertible text reads as 0, like any other type
  // this cannot convert.
  std::string result;
  const bool ok = call_protected(
      [](lua_State *L) {
        lua::lua_wrapper instance(L);
        lua_pushstring(L, "not a number");
        instance.push_int(instance.get_int());
        return 1;
      },
      result);

  EXPECT_TRUE(ok) << "get_int must not throw: " << result;
  EXPECT_EQ(result, "0") << result;
}

TEST(LuaWrapper, ANumericStringArgumentStillConverts) {
  std::string result;
  const bool ok = call_protected(
      [](lua_State *L) {
        lua::lua_wrapper instance(L);
        lua_pushstring(L, "42");
        instance.push_int(instance.get_int());
        return 1;
      },
      result);

  EXPECT_TRUE(ok) << result;
  EXPECT_EQ(result, "42") << result;
}

namespace {
// Stands in for the Check_MK data objects: a heap object owned by a userdata
// slot, registered under the internal instance prefix the wrapper looks up.
struct probe_object {
  static const std::string tag;
  static int live;
  probe_object() { ++live; }
  ~probe_object() { --live; }
};
const std::string probe_object::tag = "probe";
int probe_object::live = 0;
}  // namespace

TEST(LuaWrapper, AnExplicitGcFollowedByAMethodRaisesInsteadOfCrashing) {
  // obj:__gc() is reachable from a script (the metatable is its own __index),
  // and the collector runs the metamethod again afterwards. The first call
  // must delete exactly once, and a method on the dead object must raise a
  // Lua error rather than dereference the emptied slot.
  probe_object::live = 0;
  std::string message;
  const bool ok = call_protected(
      [](lua_State *L) {
        lua::lua_wrapper instance(L);
        luaL_newmetatable(L, (lua::internal_user_instance_prefix + probe_object::tag).c_str());
        lua_pop(L, 1);
        instance.push_user_object_instance<probe_object>();
        if (probe_object::live != 1) return instance.error("expected one live object after push");
        instance.destroy_user_object_instance<probe_object>();
        instance.destroy_user_object_instance<probe_object>();
        if (probe_object::live != 0) return instance.error("expected the object to be deleted once");
        instance.get_user_object_instance<probe_object>();
        lua_pushstring(L, "reached the method body with a dead object");
        return 1;
      },
      message);

  EXPECT_FALSE(ok) << message;
  EXPECT_NE(message.find("probe: object was already destroyed"), std::string::npos) << message;
  EXPECT_EQ(probe_object::live, 0);
}
