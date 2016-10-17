/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#pragma once

#include <string>
#include <list>
#include <vector>

#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/dll_defines.hpp>

namespace nscapi {
	class NSCAPI_EXPORT core_helper {
		const nscapi::core_wrapper *core_;
		int plugin_id_;
	public:
		core_helper(const nscapi::core_wrapper *core, int plugin_id) : core_(core), plugin_id_(plugin_id) {}
		void register_command(std::string command, std::string description, std::list<std::string> aliases = std::list<std::string>());
		void unregister_command(std::string command);
		void register_alias(std::string command, std::string description, std::list<std::string> aliases = std::list<std::string>());
		void register_channel(const std::string channel);

		NSCAPI::nagiosReturn simple_query(const std::string command, const std::list<std::string> & argument, std::string & message, std::string & perf);
		bool simple_query(const std::string command, const std::list<std::string> & argument, std::string & result);
		bool simple_query(const std::string command, const std::vector<std::string> & argument, std::string & result);
		NSCAPI::nagiosReturn simple_query_from_nrpe(const std::string command, const std::string & buffer, std::string & message, std::string & perf);

		NSCAPI::nagiosReturn exec_simple_command(const std::string target, const std::string command, const std::list<std::string> &argument, std::list<std::string> & result);
		bool submit_simple_message(const std::string channel, const std::string source_id, const std::string target_id, const std::string command, const NSCAPI::nagiosReturn code, const std::string & message, const std::string & perf, std::string & response);

	private:
		const nscapi::core_wrapper* get_core();
	};
}