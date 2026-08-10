// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "handler_impl.hpp"

check_mk::packet handler_impl::process() {
  boost::optional<scripts::command_definition<lua::lua_traits> > cmd = scripts_->find_command("check_mk", "s_callback");
  if (!cmd) {
    NSC_LOG_ERROR_STD("No check_mk callback found!");
    return check_mk::packet();
  }

  // This runs on the socket io pool (10 threads by default), so several
  // check_mk connections can be in here at once - and a LUAScript query or a
  // scheduler tick may be executing on the same lua_State on yet another
  // thread. Lua has no internal locking, so concurrent pcall (let alone the
  // full collection below) corrupts the interpreter heap. Hold the GIL across
  // the whole sequence, including prep_function's pushes. See lua::lua_gil.
  lua::lua_gil::guard gil;
  lua::lua_wrapper instance(lua::lua_runtime::prep_function(cmd->information, cmd->function));
  int args = 1;
  if (cmd->function.object_ref != 0) {
    args = 2;
  }
  auto data = check_mk::check_mk_packet_wrapper::wrap(instance.L);
  if (instance.pcall(args, 0, 0) != 0) {
    NSC_LOG_ERROR_STD("Failed to process check_mk result: " + instance.pop_string());
    return check_mk::packet();
  }
  check_mk::packet packet = data->packet;
  instance.gc(LUA_GCCOLLECT, 0);
  return packet;
}