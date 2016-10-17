/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "handler_impl.hpp"

check_mk::packet handler_impl::process() {
	boost::optional<scripts::command_definition<lua::lua_traits> > cmd = scripts_->find_command("check_mk", "s_callback");
	if (!cmd) {
		NSC_LOG_ERROR_STD("No check_mk callback found!");
		return check_mk::packet();
	}

	lua::lua_wrapper instance(lua::lua_runtime::prep_function(cmd->information, cmd->function));
	int args = 1;
	if (cmd->function.object_ref != 0)
		args = 2;
	check_mk::check_mk_packet_wrapper* obj = Luna<check_mk::check_mk_packet_wrapper>::createNew(instance);
	if (instance.pcall(args, LUA_MULTRET, 0) != 0) {
		NSC_LOG_ERROR_STD("Failed to process check_mk result: " + instance.pop_string());
		return check_mk::packet();
	}
	check_mk::packet packet = obj->packet;
	instance.gc(LUA_GCCOLLECT, 0);
	return packet;
}