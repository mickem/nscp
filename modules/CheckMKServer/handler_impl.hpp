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