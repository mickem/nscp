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

#include <nscapi/plugin.hpp>

NSC_WRAPPERS_MAIN();

class FileLogger : public nscapi::impl::simple_plugin, public nscapi::impl::simple_log_handler {
private:
	std::string file_;
	std::string format_;
	bool init_;
	int log_mask_;
	std::wstring cfg_file_;
	std::wstring cfg_root_;

public:
	FileLogger();
	virtual ~FileLogger();
	// Module calls
	bool loadModule();
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();
	std::wstring getConfigurationMeta();


	std::wstring getModuleName() {
		return _T("File logger");
	}
	nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 0, 1 };
		return version;
	}
	std::wstring getModuleDescription() {
		return _T("Writes errors and (if configured) debug info to a text file.");
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	void handleMessage(int msgType, const std::string file, int line, std::string message);
	int handleCommand(TCHAR* command, TCHAR **argument, TCHAR *returnBuffer, int returnBufferLen);
	//void writeEntry(std::wstring line);


	std::string getFileName();
	inline std::wstring get_formated_date();

};