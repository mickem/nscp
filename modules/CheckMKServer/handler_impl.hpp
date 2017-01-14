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

#pragma once

#include <boost/tuple/tuple.hpp>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

#include <check_mk/data.hpp>
#include <check_mk/server/server_handler.hpp>
#include <check_mk/lua/lua_check_mk.hpp>

class handler_impl : public check_mk::server::handler {
	bool allowArgs_;
	boost::shared_ptr<scripts::script_manager<lua::lua_traits> > scripts_;
public:
	handler_impl(boost::shared_ptr<scripts::script_manager<lua::lua_traits> > scripts) : allowArgs_(false), scripts_(scripts) {}

	virtual void set_allow_arguments(bool v) {
		allowArgs_ = v;
	}

	check_mk::packet process();

	virtual void log_debug(std::string module, std::string file, int line, std::string msg) const {
		if (GET_CORE()->should_log(NSCAPI::log_level::debug)) {
			GET_CORE()->log(NSCAPI::log_level::debug, file, line, msg);
		}
	}
	virtual void log_error(std::string module, std::string file, int line, std::string msg) const {
		if (GET_CORE()->should_log(NSCAPI::log_level::error)) {
			GET_CORE()->log(NSCAPI::log_level::error, file, line, msg);
		}
	}
};