/*
 * Copyright (C) 2004-2016 Michael Medin
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