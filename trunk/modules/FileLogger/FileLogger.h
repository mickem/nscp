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

class FileLogger {
private:
	std::string file_;
	std::string format_;

public:
	FileLogger();
	virtual ~FileLogger();
	// Module calls
	bool loadModule();
	bool unloadModule();
	std::string getConfigurationMeta();


	std::string getModuleName() {
		return "File logger";
	}
	NSCModuleWrapper::module_version getModuleVersion() {
		NSCModuleWrapper::module_version version = {0, 0, 1 };
		return version;
	}
	std::string getModuleDescription() {
		return "Writes errors and (if configured) debug info to a text file.";
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	void handleMessage(int msgType, char* file, int line, char* message);
	int handleCommand(char* command, char **argument, char *returnBuffer, int returnBufferLen);


	std::string getFileName();
};