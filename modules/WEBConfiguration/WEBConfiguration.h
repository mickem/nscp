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
//#include <config.h>
#include <strEx.h>
#include <utils.h>
#include <checkHelpers.hpp>
#include <boost/thread/thread.hpp>

namespace http {
	namespace server {
		class Server;
		class Configuration;
	}
}
namespace Wt {
	class WebController;
	class Configuration;
	class HTTPStream;
}
class WEBConfiguration {
private:

public:
	WEBConfiguration();
	virtual ~WEBConfiguration();
	// Module calls
	bool loadModule(NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	std::wstring getModuleName() {
		return _T("WEBConfiguration");
	}
	std::wstring getModuleDescription() {
		return _T("WEBConfiguration Allows remote configuration and administration of NSCP.");
	}
	NSCModuleWrapper::module_version getModuleVersion() {
		NSCModuleWrapper::module_version version = {0, 0, 1 };
		return version;
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	NSCAPI::nagiosReturn handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf);
	int commandLineExec(const TCHAR* command,const unsigned int argLen,TCHAR** args);

	// Check commands
	NSCAPI::nagiosReturn writeConf(const unsigned int argLen, TCHAR **char_args, std::wstring &message);
	NSCAPI::nagiosReturn readConf(const unsigned int argLen, TCHAR **char_args, std::wstring &message);
	NSCAPI::nagiosReturn setVariable(const unsigned int argLen, TCHAR **char_args, std::wstring &message);
	NSCAPI::nagiosReturn getVariable(const unsigned int argLen, TCHAR **char_args, std::wstring &message);

private:
	typedef checkHolders::CheckConatiner<checkHolders::MaxMinBoundsDiscSize> PathConatiner;
	typedef checkHolders::CheckConatiner<checkHolders::MaxMinPercentageBoundsDiskSize> DriveConatiner;
	boost::thread **threads;
	http::server::Server *server;
	Wt::WebController *controller;
	Wt::Configuration *wtConfiguration;
	Wt::HTTPStream    *stream;
	http::server::Configuration *config;
};