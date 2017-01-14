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

#include <check_mk/lua/lua_check_mk.hpp>

//////////////////////////////////////////////////////////////////////////
check_mk::check_mk_lua_wrapper::check_mk_lua_wrapper(lua_State *L, bool) {
	lua::lua_wrapper instance(L);
	info = instance.get_userdata<lua::script_information*>(lua::lua_traits::user_data_tag);
}
int check_mk::check_mk_lua_wrapper::client_callback(lua_State *L) {
	// void = (function)
	lua::lua_traits::function fundata;
	lua::lua_wrapper lua_instance(L);
	int count = lua_instance.size();
	if (count < 1)
		return lua_instance.error("Invalid syntax: client(<function>);");
	std::string funname;
	if (lua_instance.pop_string(funname)) {
		lua_instance.getglobal(funname);
	}
	if (!lua_instance.pop_function_ref(fundata.function_ref))
		return lua_instance.error("Invalid function");
	if (count > 1) {
		if (!lua_instance.pop_instance_ref(fundata.object_ref))
			return lua_instance.error("Invalid object");
	}
	info->register_command("check_mk", "c_callback", "", fundata);
	return lua_instance.size();
}
int check_mk::check_mk_lua_wrapper::server_callback(lua_State *L) {
	// void = (function)
	lua::lua_traits::function fundata;
	lua::lua_wrapper lua_instance(L);
	int count = lua_instance.size();
	if (count < 1)
		return lua_instance.error("Invalid syntax: server(<function>);");
	std::string funname;
	if (lua_instance.pop_string(funname)) {
		lua_instance.getglobal(funname);
	}
	if (!lua_instance.pop_function_ref(fundata.function_ref))
		return lua_instance.error("Invalid function");
	if (count > 1) {
		if (!lua_instance.pop_instance_ref(fundata.object_ref))
			return lua_instance.error("Invalid object");
	}
	info->register_command("check_mk", "s_callback", "", fundata);
	return lua_instance.size();
}
const char check_mk::check_mk_lua_wrapper::className[] = "check_mk";
const Luna<check_mk::check_mk_lua_wrapper>::FunctionType check_mk::check_mk_lua_wrapper::Functions[] = {
	{ "client_callback", &check_mk::check_mk_lua_wrapper::client_callback },
	{ "client", &check_mk::check_mk_lua_wrapper::client_callback },
	{ "server_callback", &check_mk::check_mk_lua_wrapper::server_callback },
	{ "server", &check_mk::check_mk_lua_wrapper::server_callback },
	{ 0 }
};
const Luna<check_mk::check_mk_lua_wrapper>::PropertyType check_mk::check_mk_lua_wrapper::Properties[] = { {0} };

//////////////////////////////////////////////////////////////////////////
int check_mk::check_mk_packet_wrapper::get_section(lua_State *L) {
	lua::lua_wrapper lua_instance(L);
	if (lua_instance.size() < 1)
		return lua_instance.error("Invalid syntax: get_section(id)");
	int id = lua_instance.pop_int() - 1;
	try {
		check_mk::packet::section s = packet.get_section(id);
		check_mk_section_wrapper* obj = Luna<check_mk_section_wrapper>::createNew(lua_instance);
		obj->section = s;
		return 1;
	} catch (const std::exception &e) {
		return lua_instance.error(std::string("Failed to get section: ") + e.what());
	}
}
int check_mk::check_mk_packet_wrapper::add_section(lua_State *L) {
	lua::lua_wrapper lua_instance(L);
	if (lua_instance.size() < 1)
		return lua_instance.error("Invalid syntax: get_section(s)");
	check_mk_section_wrapper *obj = Luna<check_mk_section_wrapper>::check(lua_instance, 0);
	if (!obj) {
		return 0;
	}
	try {
		packet.add_section(obj->section);
		return 0;
	} catch (const std::exception &e) {
		return lua_instance.error(std::string("Failed to get section: ") + e.what());
	}
}
int check_mk::check_mk_packet_wrapper::size_section(lua_State *L) {
	lua::lua_wrapper instance(L);
	instance.push_int(static_cast<int>(packet.section_list.size()));
	return 1;
}
const char check_mk::check_mk_packet_wrapper::className[] = "packet";
const Luna<check_mk::check_mk_packet_wrapper>::FunctionType check_mk::check_mk_packet_wrapper::Functions[] = {
	{ "get_section", &check_mk::check_mk_packet_wrapper::get_section },
	{ "add_section", &check_mk::check_mk_packet_wrapper::add_section },
	{ "size_section", &check_mk::check_mk_packet_wrapper::size_section },
	{ 0 }
};
const Luna<check_mk::check_mk_packet_wrapper>::PropertyType check_mk::check_mk_packet_wrapper::Properties[] = { {0} };

//////////////////////////////////////////////////////////////////////////
int check_mk::check_mk_section_wrapper::get_line(lua_State *L) {
	lua::lua_wrapper lua_instance(L);
	if (lua_instance.size() < 1)
		return lua_instance.error("Invalid syntax: get_line(id)");
	int id = lua_instance.pop_int() - 1;
	try {
		check_mk::packet::section::line l = section.get_line(id);
		check_mk_line_wrapper* obj = Luna<check_mk_line_wrapper>::createNew(lua_instance);
		obj->line = l;
		return 1;
	} catch (const std::exception &e) {
		return lua_instance.error(std::string("Failed to get section: ") + e.what());
	}
}
int check_mk::check_mk_section_wrapper::add_line(lua_State *L) {
	lua::lua_wrapper lua_instance(L);
	if (lua_instance.size() < 1)
		return lua_instance.error("Invalid syntax: get_section(s)");
	if (lua_instance.is_string()) {
		try {
			std::string l = lua_instance.pop_string();
			section.add_line(check_mk::packet::section::line(l));
			return 0;
		} catch (const std::exception &e) {
			return lua_instance.error(std::string("Failed to get section: ") + e.what());
		}
	} else {
		check_mk_line_wrapper *obj = Luna<check_mk_line_wrapper>::check(lua_instance, 0);
		if (!obj) {
			return 0;
		}
		try {
			section.add_line(obj->line);
			return 0;
		} catch (const std::exception &e) {
			return lua_instance.error(std::string("Failed to get section: ") + e.what());
		}
	}
}
int check_mk::check_mk_section_wrapper::get_title(lua_State *L) {
	lua::lua_wrapper lua_instance(L);
	lua_instance.push_string(section.title);
	return 1;
}
int check_mk::check_mk_section_wrapper::set_title(lua_State *L) {
	lua::lua_wrapper lua_instance(L);
	std::string title;
	if (!lua_instance.pop_string(title)) {
		return lua_instance.error("Invalid syntax: set_title(title)");
	}
	section.title = title;
	return 1;
}
int check_mk::check_mk_section_wrapper::size_line(lua_State *L) {
	lua::lua_wrapper lua_instance(L);
	lua_instance.push_int(static_cast<int>(section.lines.size()));
	return 1;
}
const char check_mk::check_mk_section_wrapper::className[] = "section";
const Luna<check_mk::check_mk_section_wrapper>::FunctionType check_mk::check_mk_section_wrapper::Functions[] = {
	{ "get_line", &check_mk::check_mk_section_wrapper::get_line },
	{ "add_line", &check_mk::check_mk_section_wrapper::add_line },
	{ "get_title", &check_mk::check_mk_section_wrapper::get_title },
	{ "set_title", &check_mk::check_mk_section_wrapper::set_title },
	{ "size_line", &check_mk::check_mk_section_wrapper::size_line },
	{ 0 }
};
const Luna<check_mk::check_mk_section_wrapper>::PropertyType check_mk::check_mk_section_wrapper::Properties[] = { {0} };

//////////////////////////////////////////////////////////////////////////
int check_mk::check_mk_line_wrapper::get_item(lua_State *L) {
	lua::lua_wrapper lua_instance(L);
	if (lua_instance.size() < 1)
		return lua_instance.error("Invalid syntax: get_line(id)");
	int id = lua_instance.pop_int() - 1;
	try {
		std::string item = line.get_item(id);
		lua_instance.push_string(item);
		return 1;
	} catch (const std::exception &e) {
		return lua_instance.error(std::string("Failed to get item: ") + e.what());
	}
}
int check_mk::check_mk_line_wrapper::get_line(lua_State *L) {
	lua::lua_wrapper lua_instance(L);
	lua_instance.push_string(line.get_line());
	return 1;
}
int check_mk::check_mk_line_wrapper::set_line(lua_State *L) {
	lua::lua_wrapper lua_instance(L);
	std::string l;
	if (!lua_instance.pop_string(l)) {
		return lua_instance.error("Invalid syntax: set_line(line)");
	}
	line.set_line(l);
	return 0;
}
int check_mk::check_mk_line_wrapper::add_item(lua_State *L) {
	lua::lua_wrapper lua_instance(L);
	std::string l;
	if (!lua_instance.pop_string(l)) {
		return lua_instance.error("Invalid syntax: add_item(line)");
	}
	line.items.push_back(l);
	return 0;
}
int check_mk::check_mk_line_wrapper::size_item(lua_State *L) {
	lua::lua_wrapper lua_instance(L);
	lua_instance.push_int(static_cast<int>(line.items.size()));
	return 1;
}
const char check_mk::check_mk_line_wrapper::className[] = "line";
const Luna<check_mk::check_mk_line_wrapper>::FunctionType check_mk::check_mk_line_wrapper::Functions[] = {
	{ "add_item", &check_mk::check_mk_line_wrapper::add_item },
	{ "get_item", &check_mk::check_mk_line_wrapper::get_item },
	{ "get_line", &check_mk::check_mk_line_wrapper::get_line },
	{ "set_line", &check_mk::check_mk_line_wrapper::set_line },
	{ "size_item", &check_mk::check_mk_line_wrapper::size_item },
	{ 0 }
};
const Luna<check_mk::check_mk_line_wrapper>::PropertyType check_mk::check_mk_line_wrapper::Properties[] = { {0} };

//////////////////////////////////////////////////////////////////////////

void check_mk::check_mk_plugin::load(lua::lua_wrapper &instance) {
	Luna<check_mk::check_mk_lua_wrapper>::Register(instance, "nscp");
	Luna<check_mk::check_mk_packet_wrapper>::Register(instance, "nscp");
	Luna<check_mk::check_mk_section_wrapper>::Register(instance, "nscp");
	Luna<check_mk::check_mk_line_wrapper>::Register(instance, "nscp");
}
void check_mk::check_mk_plugin::unload(lua::lua_wrapper &) {}