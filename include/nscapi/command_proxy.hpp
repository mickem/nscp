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


#include <nscapi/nscapi_core_wrapper.hpp>

namespace nscapi {
	class command_proxy {
	private:
		unsigned int plugin_id_;
		nscapi::core_wrapper* core_;

	public:
		command_proxy(unsigned int plugin_id, nscapi::core_wrapper* core) : plugin_id_(plugin_id), core_(core) {}
		virtual void registry_query(const std::string &request, std::string &response) {
			if (!core_->registry_query(request, response)) {
				throw "TODO: FIXME: DAMN!!!";
			}
		}

		unsigned int get_plugin_id() const { return plugin_id_; }

		virtual void err(const char* file, int line, std::string message) {
			core_->log(NSCAPI::log_level::error, file, line, message);
		}
		virtual void warn(const char* file, int line, std::string message) {
			core_->log(NSCAPI::log_level::warning, file, line, message);
		}
		virtual void info(const char* file, int line, std::string message)  {
			core_->log(NSCAPI::log_level::info, file, line, message);
		}
		virtual void debug(const char* file, int line, std::string message)  {
			core_->log(NSCAPI::log_level::debug, file, line, message);
		}
		virtual void trace(const char* file, int line, std::string message)  {
			core_->log(NSCAPI::log_level::trace, file, line, message);
		}
	};
}