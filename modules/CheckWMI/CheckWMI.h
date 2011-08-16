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
NSC_WRAPPERS_MAIN();
NSC_WRAPPERS_CLI();

#include <config.h>
#include <strEx.h>
#include <utils.h>
#include <settings/client/settings_client.hpp>

//#include <checkHelpers.hpp>
#include "WMIQuery.h"



struct target_helper {
	struct target_info {
		std::wstring hostname;
		std::wstring username;
		std::wstring password;
		std::wstring protocol;

		target_info() {}
		target_info(const target_info &other) 
			: hostname(other.hostname)
			, username(other.username)
			, password(other.password)
			, protocol(other.protocol)
		{}
		const target_info& operator=(const target_info &other) 
		{
			hostname = other.hostname;
			username = other.username;
			password = other.password;
			protocol = other.protocol;
			return *this;
		}

		std::wstring to_wstring() const {
			return _T("hostname: ") + hostname + 
				_T(", username: ") + username +
				_T(", password: ") + password +
				_T(", protocol: ") + protocol;
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
	typedef std::map<std::wstring, target_info> target_map_type;
	target_map_type targets;
	void add_target(nscapi::settings_helper::settings_impl_interface_ptr core, std::wstring key, std::wstring val);
	boost::optional<target_info> find(std::wstring alias) {
		if (alias.empty())
			return boost::optional<target_info>();
		target_map_type::const_iterator it = targets.find(alias);
		if (it == targets.end())
			return boost::optional<target_info>();
		NSC_DEBUG_MSG_STD(_T("FOund target: ") + it->second.to_wstring());
		return boost::optional<target_info>(it->second);
	}

};


class CheckWMI : public nscapi::impl::simple_command, public nscapi::impl::simple_plugin {
public:
	CheckWMI();
	virtual ~CheckWMI();
	// Module calls
	bool loadModule();
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	std::wstring getModuleName() {
		return _T("CheckWMI");
	}
	std::wstring getModuleDescription() {
		return _T("CheckWMI can check various file and disk related things.\nThe current version has commands to check Size of hard drives and directories.");
	}
	nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 0, 1 };
		return version;
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	NSCAPI::nagiosReturn handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf);
	int CheckWMI::commandLineExec(const wchar_t* command,const unsigned int argLen,wchar_t** args);

	// Check commands
	NSCAPI::nagiosReturn CheckSimpleWMI(const std::wstring &target, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf);
	NSCAPI::nagiosReturn CheckSimpleWMIValue(const std::wstring &target, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf);



private:
	target_helper targets;
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsDiscSize> PathContainer;
	typedef checkHolders::CheckContainer<checkHolders::MaxMinPercentageBoundsDiskSize> DriveContainer;
};
