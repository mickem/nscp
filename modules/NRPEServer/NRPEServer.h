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

#include <socket_helpers.hpp>
#include <nrpe/server/protocol.hpp>
#include "handler_impl.hpp"

NSC_WRAPPERS_MAIN();

class NRPEServer : public nscapi::impl::simple_plugin {
public:
	NRPEServer();
	virtual ~NRPEServer();
	// Module calls
	bool loadModule();
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();


	static std::wstring getModuleName() {
#ifdef USE_SSL
		return _T("NRPE server");
#else
		return _T("NRPE server (no SSL)");
#endif
	}
	static nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 0, 1 };
		return version;
	}
	static std::wstring getModuleDescription() {
		return _T("A simple server that listens for incoming NRPE connection and handles them.\nNRPE is preferred over NSClient as it is more flexible. You can of cource use both NSClient and NRPE.");
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	NSCAPI::nagiosReturn handleCommand(const strEx::blindstr command, const unsigned int argLen, wchar_t **char_args, std::wstring &message, std::wstring &perf);
	std::wstring getConfigurationMeta();
	
private:
	socket_helpers::connection_info info_;
	boost::shared_ptr<nrpe::server::server> server_;
	boost::shared_ptr<handler_impl> handler_;


	class NRPEExceptionn {
		std::wstring error_;
	public:
		NRPEExceptionn(std::wstring s) {
			error_ = s;
		}
		std::wstring getMessage() {
			return error_;
		}
	};
};

