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
#include <boost/optional.hpp>

#include <strEx.h>
#include <settings/client/settings_client.hpp>

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_plugin_wrapper.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/macros.hpp>

#include "WMIQuery.h"



struct target_helper {
	struct target_info {
		std::string hostname;
		std::string username;
		std::string password;
		std::string protocol;

		target_info() {}
		target_info(const target_info &other) 
			: hostname(other.hostname)
			, username(other.username)
			, password(other.password)
			, protocol(other.protocol)
		{}
		const target_info& operator=(const target_info &other) {
			hostname = other.hostname;
			username = other.username;
			password = other.password;
			protocol = other.protocol;
			return *this;
		}

		std::string to_string() const {
			return "hostname: " + hostname + 
				", username: " + username +
				", password: " + password +
				", protocol: " + protocol;
		}
		void update_from(const target_helper::target_info& other) {
			if (hostname.empty())
				hostname = other.hostname;
			if (username.empty())
				username = other.username;
			if (password.empty())
				password = other.password;
			if (protocol.empty())
				protocol = other.protocol;
		}
	};
	typedef std::map<std::string, target_info> target_map_type;
	target_map_type targets;
	void add_target(nscapi::settings_helper::settings_impl_interface_ptr core, std::string key, std::string val);
	boost::optional<target_info> find(std::string alias) {
		if (alias.empty())
			return boost::optional<target_info>();
		target_map_type::const_iterator it = targets.find(alias);
		if (it == targets.end())
			return boost::optional<target_info>();
		NSC_DEBUG_MSG_STD("Found target: " + it->second.to_string());
		return boost::optional<target_info>(it->second);
	}

};


class CheckWMI : public nscapi::impl::simple_plugin {
public:
	CheckWMI() {}
	virtual ~CheckWMI() {}
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	void check_wmi(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	NSCAPI::nagiosReturn commandLineExec(const std::string &command, const std::list<std::string> &arguments, std::string &result);

private:
	target_helper targets;
};
