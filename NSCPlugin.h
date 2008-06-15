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
#include <NSCHelper.h>
#include <sstream>

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
	NSPluginException(std::wstring file, std::wstring error) : file_(file), error_(error) {
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
	HMODULE hModule_;		// module handle to the DLL (once it is loaded)
	std::wstring file_;		// Name of the DLL file
	bool broken_;

	typedef INT (*lpModuleHelperInit)(NSCModuleHelper::lpNSAPILoader f);
	typedef INT (*lpLoadModule)(int);
	typedef INT (*lpGetName)(TCHAR*,unsigned int);
	typedef INT (*lpGetDescription)(TCHAR*,unsigned int);
	typedef INT (*lpGetVersion)(int*,int*,int*);
	typedef INT (*lpHasCommandHandler)();
	typedef INT (*lpHasMessageHandler)();
	typedef NSCAPI::nagiosReturn (*lpHandleCommand)(const TCHAR*,const unsigned int, TCHAR**,TCHAR*,unsigned int,TCHAR *,unsigned int);
	typedef INT (*lpCommandLineExec)(const TCHAR*,const unsigned int,TCHAR**);
	typedef INT (*lpHandleMessage)(int,const TCHAR*,const int,const TCHAR*);
	typedef INT (*lpUnLoadModule)();
	typedef INT (*lpGetConfigurationMeta)(int, TCHAR*);


	lpModuleHelperInit fModuleHelperInit;
	lpLoadModule fLoadModule;
	lpGetName fGetName;
	lpGetVersion fGetVersion;
	lpGetDescription fGetDescription;
	lpHasCommandHandler fHasCommandHandler;
	lpHasMessageHandler fHasMessageHandler;
	lpHandleCommand fHandleCommand;
	lpHandleMessage fHandleMessage;
	lpUnLoadModule fUnLoadModule;
	lpGetConfigurationMeta fGetConfigurationMeta;
	lpCommandLineExec fCommandLineExec;

public:
	NSCPlugin(const std::wstring file);
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
	NSCAPI::nagiosReturn handleCommand(const TCHAR *command, const unsigned int argLen, TCHAR **arguments, TCHAR* returnMessageBuffer, unsigned int returnMessageBufferLen, TCHAR* returnPerfBuffer, unsigned int returnPerfBufferLen);
	void handleMessage(int msgType, const TCHAR* file, const int line, const TCHAR *message);
	void unload(void);
	std::wstring getCongifurationMeta();
	int commandLineExec(const TCHAR* command, const unsigned int argLen, TCHAR **arguments);
	std::wstring getFilename() {
		if (file_.empty())
			return _T("");
		std::wstring::size_type pos = file_.find_last_of(_T("\\"));
		if (pos != std::wstring::npos && ++pos < file_.length()) {
			return file_.substr(pos);
		}
		return file_;
	}
	std::wstring getModule() {
		if (file_.empty())
			return _T("");
		std::wstring ret = file_;
		std::wstring::size_type pos = ret.find_last_of(_T("\\"));
		if (pos != std::wstring::npos && ++pos < ret.length()) {
			ret = ret.substr(pos);
		}
		pos = ret.find_last_of(_T("."));
		if (pos != std::wstring::npos) {
			ret = ret.substr(0, pos);
		}
		return ret;
	}

private:
	bool isLoaded() const {
		return bLoaded_;
	}
	bool getName_(TCHAR* buf, unsigned int buflen);
	bool getDescription_(TCHAR* buf, unsigned int buflen);
	void loadRemoteProcs_(void);
	bool getConfigurationMeta_(TCHAR* buf, unsigned int buflen);
};


