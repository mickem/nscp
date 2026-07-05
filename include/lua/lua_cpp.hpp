// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/noncopyable.hpp>

extern "C" {
#include <lua.h>

#include "lauxlib.h"
}

#include <NSCAPI.h>

#include <list>
#include <string>
#include <vector>

namespace lua {
class Lua_State : boost::noncopyable {
  lua_State *L;

 public:
  Lua_State();
  ~Lua_State();

  // implicitly act as a lua_State pointer
  inline operator lua_State *() const { return L; }
  lua_State *get_state() const { return L; }
};

const std::string internal_user_instance_prefix = "nscp.internal.";

namespace internal {
/// __gc handler for userdata produced by `push_user_object_instance<T>`.
/// The userdata holds a `T*`; on garbage-collection we delete the
/// heap-allocated T to balance the `new T()` from push_user_object_instance.
/// Without this hook every Lua state shutdown leaks one T per call
/// site — LSan caught the CoreData / RegistryData drops on a clean
/// nscp test exit.
template <class T>
int destroy_user_object_instance_gc(lua_State *L) {
  T **ptr = reinterpret_cast<T **>(luaL_checkudata(L, 1, (internal_user_instance_prefix + T::tag).c_str()));
  if (ptr != nullptr && *ptr != nullptr) {
    delete *ptr;
    *ptr = nullptr;
  }
  return 0;
}
}  // namespace internal

class lua_wrapper {
 public:
  lua_State *L;

 public:
  lua_wrapper(lua_State *L) : L(L) {}
  inline operator lua_State *() const { return L; }
  lua_State *get_state() const { return L; }

  int append_path(const std::string &path);

  //////////////////////////////////////////////////////////////////////////
  /// get_xxx
  std::string get_string(int pos = -1);
  std::string get_raw_string(int pos = -1);
  bool get_string(std::string &str, int pos = -1);
  bool get_raw_string(std::string &str, int pos = -1);
  int get_int(int pos = -1);
  bool get_boolean(int pos = -1);
  NSCAPI::nagiosReturn get_code(int pos = -1);
  std::list<std::string> get_array(const int pos = -1);
  void *get_raw_userdata(std::string id);
  template <class T>
  T get_userdata(std::string id) {
    return reinterpret_cast<T>(get_raw_userdata(id));
  }
  void set_raw_userdata(std::string id, void *data);
  template <class T>
  void set_userdata(std::string id, T data) {
    set_raw_userdata(id, reinterpret_cast<void *>(data));
  }
  void remove_userdata(std::string id);

  void new_userdata(size_t size);

  template <class T>
  T *checkudata(int pos, std::string tag) {
    return reinterpret_cast<T *>(luaL_checkudata(L, pos, tag.c_str()));
  }

  // `gc_fn` is registered as the `__gc` metamethod when non-null.
  // Pass `&internal::destroy_user_object_instance_gc<T>` (or use the
  // templated overload below) whenever `push_user_object_instance<T>`
  // is paired with this class — otherwise each T allocation leaks at
  // Lua state shutdown.
  void setup_class(const std::string name, const luaL_Reg *ctors, const luaL_Reg *functions, lua_CFunction gc_fn = nullptr);

  /// Convenience overload: same as setup_class(T::tag, ctors, functions,
  /// &destroy_user_object_instance_gc<T>). Use this for any T that's
  /// instantiated via push_user_object_instance<T>.
  template <class T>
  void setup_class(const luaL_Reg *ctors, const luaL_Reg *functions) {
    setup_class(T::tag, ctors, functions, &internal::destroy_user_object_instance_gc<T>);
  }

  void setup_global_function(const std::string name, const lua_CFunction function);
  void setup_functions(const std::string name, const luaL_Reg *functions);

  template <class T>
  T *push_user_object_instance() {
    T **ptr = this->newuserdata<T *>();
    *ptr = new T();
    luaL_getmetatable(L, (internal_user_instance_prefix + T::tag).c_str());
    lua_setmetatable(L, -2);
    return *ptr;
  }
  template <class T>
  T *get_user_object_instance(int pos = 1) {
    T **ptr = this->checkudata<T *>(pos, internal_user_instance_prefix + T::tag);
    return *ptr;
  }
  template <class T>
  int destroy_user_object_instance() {
    T **ptr = this->checkudata<T *>(1, internal_user_instance_prefix + T::tag);
    delete *ptr;
    return 0;
  }
  template <class T>
  T *newuserdata() {
    return reinterpret_cast<T *>(lua_newuserdata(L, sizeof(T)));
  }

  //////////////////////////////////////////////////////////////////////////
  /// pop_xxx
  bool pop_boolean();
  std::string pop_string();
  std::string pop_raw_string();
  bool pop_string(std::string &str);
  bool pop_raw_string(std::string &str);
  bool pop_function_ref(int &funref);
  bool pop_instance_ref(int &funref);
  int pop_int();
  NSCAPI::nagiosReturn pop_code();
  std::list<std::string> pop_array();

  //////////////////////////////////////////////////////////////////////////
  // Converters
  NSCAPI::nagiosReturn string_to_code(std::string str);
  std::string code_to_string(NSCAPI::nagiosReturn code);

  ////////////////////////////////////////////////////////////////////////////
  // Misc
  int getglobal(const std::string &name) { return lua_getglobal(L, name.c_str()); }
  inline void pop(int count = 1) { lua_pop(L, count); }
  int type(int pos = -1);
  std::string get_type_as_string(int pos = -1);

  inline bool is_string(int pos = -1) { return type(pos) == LUA_TSTRING; }
  inline bool is_function(int pos = -1) { return type(pos) == LUA_TFUNCTION; }
  inline bool is_number(int pos = -1) { return type(pos) == LUA_TNUMBER; }
  inline bool is_nil(int pos = -1) { return type(pos) == LUA_TNIL; }
  inline bool is_boolean(int pos = -1) { return type(pos) == LUA_TBOOLEAN; }
  inline bool is_table(int pos = -1) { return type(pos) == LUA_TTABLE; }

  //////////////////////////////////////////////////////////////////////////
  // push_xxx
  void push_code(NSCAPI::nagiosReturn code);
  void push_string(std::string s);
  void push_boolean(bool b);
  void push_int(int b);
  void push_raw_string(std::string s);
  void push_array(const std::list<std::string> &arr);
  void push_array(const std::vector<std::string> &arr);
  int size();
  bool empty();
  void log_stack();
  int error(std::string s);
  typedef std::pair<std::string, int> stack_trace;
  stack_trace get_stack_trace(int level = 1);
  std::string dump_stack();

  void openlibs();
  int loadfile(std::string script);
  int pcall(int nargs, int nresults, int errfunc);
  std::string op_string(int pos, std::string def = "");

  std::string check_string(int pos);
  std::list<std::string> check_array(int pos);
  bool check_bool(int pos);
  int op_int(int pos, int def = 0);
  int checkint(int pos);
  int gc(int what, int data);
};

class lua_exception : public std::exception {
  std::string error_;

 public:
  lua_exception(std::string error) noexcept : error_(error) {}
  lua_exception(const lua_exception &other) noexcept : error_(other.error_) {}
  ~lua_exception() throw() {}
  const char *what() const throw() { return error_.c_str(); }
};

std::string w2s(std::wstring s);
std::wstring s2w(std::string s);
}  // namespace lua