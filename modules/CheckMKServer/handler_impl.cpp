// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "handler_impl.hpp"

check_mk::packet handler_impl::process() {
  boost::optional<scripts::command_definition<lua::lua_traits> > cmd = scripts_->find_command("check_mk", "s_callback");
  if (!cmd) {
    NSC_LOG_ERROR_STD("No check_mk callback found!");
    return check_mk::packet();
  }

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