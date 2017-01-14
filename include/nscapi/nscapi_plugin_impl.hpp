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

#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/command_proxy.hpp>

#include <boost/shared_ptr.hpp>

namespace Plugin {
	class Common_Header;
	class ExecuteRequestMessage;
	class ExecuteRequestMessage_Request;
	class ExecuteResponseMessage_Response;
	class QueryRequestMessage;
	class QueryRequestMessage_Request;
	class QueryResponseMessage;
	class QueryResponseMessage_Response;
	class SubmitResponseMessage_Response;
	class MetricsMessage_Response;
}

namespace nscapi {
	namespace impl {
		struct simple_plugin {
			int id_;
			nscapi::core_wrapper* get_core() const;
			inline unsigned int get_id() const {
				return id_;
			}
			inline void set_id(const unsigned int id) {
				id_ = id;
			}
			inline boost::shared_ptr<nscapi::settings_proxy> get_settings_proxy() {
				return boost::shared_ptr<nscapi::settings_proxy>(new nscapi::settings_proxy(id_, get_core()));
			}
			inline boost::shared_ptr<nscapi::command_proxy> get_command_proxy() {
				return boost::shared_ptr<nscapi::command_proxy>(new nscapi::command_proxy(id_, get_core()));
			}
			std::string get_base_path() const;
		};
	}
}