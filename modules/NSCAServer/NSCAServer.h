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
#include <nsca/server/server.hpp>

NSC_WRAPPERS_MAIN();

class NSCAServer : public nscapi::impl::simple_plugin {
private:
	nsca::server::nsca_connection_info info_;

public:
	NSCAServer();
	virtual ~NSCAServer() {}
	// Module calls
	bool loadModule();
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();


	static std::wstring getModuleName() {
#ifdef HAVE_LIBCRYPTOPP
		return _T("NSCA server (w/ encryption)");
#else
		return _T("NSCA server (no encryption)");
#endif
	}
	static nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 0, 1 };
		return version;
	}
	static std::wstring getModuleDescription() {
		return _T("A simple server that listens for incoming NSCA connection and handles them.\nAvalible crypto are: ") + getCryptos();
	}

	static std::wstring getCryptos();

	boost::shared_ptr<nsca::server::nsca_server> server_;
};

