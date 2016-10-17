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