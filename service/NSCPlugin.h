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

#include <NSCAPI.h>
//#include <NSCHelper.h>
#include <sstream>
#include <dll/dll.hpp>

/**
 * @ingroup NSClient++
 * Exception class for the NSCPlugin class.
 * When an unexpected error occurs in NSCPlugin this exception is thrown.
 *
 * @version 1.0
 * first version
 *
 * @date 02-12-2005
 *
 * @author mickem
 *
 * @par license
 * This code is absolutely free to use and modify. The code is provided "as is" with
 * no expressed or implied warranty. The author accepts no liability if it causes
 * any damage to your computer, causes your pet to fall ill, increases baldness
 * or makes your car start emitting strange noises when you start it up.
 * This code has no bugs, just undocumented features!
 * 
 * @todo 
 * Add status codes to make error type simpler to parse out.
 *
 * @bug 
 *
 */
class NSPluginException {
public:
	std::wstring file_;	// DLL filename (for which the exception was thrown)
	std::wstring error_;	// An error message (human readable format)
	/**
	 * @param file DLL filename (for which the exception is thrown)
	 * @param error An error message (human readable format)
	 */
	NSPluginException(dll::dll &module, std::wstring error) : error_(error) {
		file_ = getModule(module.get_file());
	}
	std::wstring getModule(std::wstring file) {
		if (file.empty())
			return _T("");
		std::wstring ret = file;
		std::wstring::size_type pos = ret.find_last_of(_T("\\"));
		if (pos != std::wstring::npos && ++pos < ret.length()) {
			ret = ret.substr(pos);
		}
		return ret;
	}


};

/**
 * @ingroup NSClient++
 * NSCPlugin is a wrapper class to wrap all DLL calls and make things simple and clean inside the actual application.<br>
 * Things tend to be one-to-one by which I mean that a call to a function here should call the corresponding function in the plug in (if loaded).
 * If things are "broken" NSPluginException is called to indicate this. Error states are returned for normal "conditions".
 *
 *
 * @version 1.0
 * first version
 *
 * @date 02-12-2005
 *
 * @author mickem
 *
 * @par license
 * This code is absolutely free to use and modify. The code is provided "as is" with
 * no expressed or implied warranty. The author accepts no liability if it causes
 * any damage to your computer, causes your pet to fall ill, increases baldness
 * or makes your car start emitting strange noises when you start it up.
 * This code has no bugs, just undocumented features!
 * 
 * @todo 
 * getVersion() is not implemented as of yet.
 *
 * @bug 
 *
 */
class NSCPlugin {
private:
	bool bLoaded_;			// Status of plug in
	dll::dll module_;
	bool broken_;
	static unsigned int last_plugin_id_;
	unsigned int plugin_id_;

	typedef int (*lpModuleHelperInit)(unsigned int, NSCModuleHelper::lpNSAPILoader f);
	typedef int (*lpLoadModule)(int);
	typedef int (*lpGetName)(wchar_t*,unsigned int);
	typedef int (*lpGetDescription)(wchar_t*,unsigned int);
	typedef int (*lpGetVersion)(int*,int*,int*);
	typedef int (*lpHasCommandHandler)();
	typedef int (*lpHasMessageHandler)();
	typedef NSCAPI::nagiosReturn (*lpHandleCommand)(const wchar_t*,const char*,const unsigned int,char**,unsigned int*);
	typedef int (*lpDeleteBuffer)(char**);
	typedef int (*lpCommandLineExec)(const unsigned int,wchar_t**);
	typedef int (*lpHandleMessage)(int,const wchar_t*,const int,const wchar_t*);
	typedef int (*lpUnLoadModule)();
	typedef int (*lpGetConfigurationMeta)(int, wchar_t*);
	typedef void (*lpShowTray)();
	typedef void (*lpHideTray)();


	lpModuleHelperInit fModuleHelperInit;
	lpLoadModule fLoadModule;
	lpGetName fGetName;
	lpGetVersion fGetVersion;
	lpGetDescription fGetDescription;
	lpHasCommandHandler fHasCommandHandler;
	lpHasMessageHandler fHasMessageHandler;
	lpHandleCommand fHandleCommand;
	lpDeleteBuffer fDeleteBuffer;
	lpHandleMessage fHandleMessage;
	lpUnLoadModule fUnLoadModule;
	lpGetConfigurationMeta fGetConfigurationMeta;
	lpCommandLineExec fCommandLineExec;
	lpShowTray fShowTray;
	lpHideTray fHideTray;

public:
	NSCPlugin(const boost::filesystem::wpath file);
	NSCPlugin(NSCPlugin &other);
	virtual ~NSCPlugin(void);

	std::wstring getName(void);
	std::wstring getDescription();
	void load_dll(void);
	bool load_plugin(NSCAPI::moduleLoadMode mode);
	void setBroken(bool broken);
	bool isBroken();
	bool getVersion(int *major, int *minor, int *revision);
	bool hasCommandHandler(void);
	bool hasMessageHandler(void);
	NSCAPI::nagiosReturn handleCommand(const wchar_t *command, const char* dataBuffer, const unsigned int dataBuffer_len, char** returnBuffer, unsigned int *returnBuffer_len);
	NSCAPI::nagiosReturn handleCommand(const wchar_t* command, std::string &request, std::string &reply);
	void deleteBuffer(char**buffer);
	void handleMessage(int msgType, const wchar_t* file, const int line, const wchar_t *message);
	void unload(void);
	std::wstring getCongifurationMeta();
	int commandLineExec(const unsigned int argLen, wchar_t **arguments);
	void showTray();
	void hideTray();

	std::wstring getFilename() {
		std::wstring file = module_.get_file();
		if (file.empty())
			return _T("");
		std::wstring::size_type pos = file.find_last_of(_T("\\"));
		if (pos != std::wstring::npos && ++pos < file.length()) {
			return file.substr(pos);
		}
		return file;
	}
	std::wstring getModule() {
		std::wstring file = getFilename();
		std::wstring::size_type pos = file.find_last_of(_T("."));
		if (pos != std::wstring::npos) {
			file = file.substr(0, pos);
		}
		return file;
	}
	bool getLastIsMsgPlugin() {
		return lastIsMsgPlugin_;
	}
	bool isLoaded() const {
		return bLoaded_;
	}
	unsigned int get_id() const { return plugin_id_; }

private:
	bool lastIsMsgPlugin_;
	bool getName_(wchar_t* buf, unsigned int buflen);
	bool getDescription_(wchar_t* buf, unsigned int buflen);
	void loadRemoteProcs_(void);
	bool getConfigurationMeta_(wchar_t* buf, unsigned int buflen);
};




