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

NSC_WRAPPERS_MAIN();
NSC_WRAPPERS_CHANNELS();

class NSCAAgent : public nscapi::impl::SimpleNotificationHandler, public nscapi::impl::simple_plugin {
private:

	std::string hostname_;
	std::wstring nscahost_;
	unsigned int nscaport_;
	unsigned int payload_length_;
	bool cacheNscaHost_;
	std::string password_;
	int encryption_method_;
	unsigned int timeout_;
	int time_delta_;

public:
	NSCAAgent();
	virtual ~NSCAAgent();
	// Module calls
	bool loadModule();
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();
	std::wstring getConfigurationMeta();

	/**
	* Return the module name.
	* @return The module name
	*/
	std::wstring getModuleName() {
#ifdef HAVE_LIBCRYPTOPP
		return _T("NSCAAgent (w/ encryption)");
#else
		return _T("NSCAAgent");
#endif
	}
	/**
	* Module version
	* @return module version
	*/
	nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 3, 0 };
		return version;
	}
	std::wstring getModuleDescription() {
		return std::wstring(_T("Passive check support (needs NSCA on nagios server).\nAvalible crypto are: ")) + getCryptos();
	}
	bool hasNotificationHandler() { return true; }

	std::wstring getCryptos();

	NSCAPI::nagiosReturn handleSimpleNotification(const std::wstring channel, const std::wstring command, NSCAPI::nagiosReturn code, std::wstring msg, std::wstring perf);


	void set_delay(std::wstring key) {
		time_delta_ = strEx::stol_as_time_sec(key, 1);
	}

};
