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
#include "stdafx.h"
#include "NSClient++.h"
#include <error.hpp>
/**
 * Default c-tor
 * Initializes the plug in name but does not load the actual plug in.<br>
 * To load the plug in use function load() that loads an initializes the plug in.
 *
 * @param file The file (DLL) to load as a NSC plug in.
 */
NSCPlugin::NSCPlugin(const std::wstring file)
	: file_(file)
	,hModule_(NULL)
	,fLoadModule(NULL)
	,fGetName(NULL)
	,fHasCommandHandler(NULL)
	,fUnLoadModule(NULL)
	,fHasMessageHandler(NULL)
	,fHandleMessage(NULL)
	,fGetDescription(NULL)
	,fGetConfigurationMeta(NULL)
	,fGetVersion(NULL)
	,fCommandLineExec(NULL)
	,bLoaded_(false)
{
}

NSCPlugin::NSCPlugin(NSCPlugin &other)
	:hModule_(NULL)
	,fLoadModule(NULL)
	,fGetName(NULL)
	,fHasCommandHandler(NULL)
	,fUnLoadModule(NULL)
	,fHasMessageHandler(NULL)
	,fHandleMessage(NULL)
	,fGetDescription(NULL)
	,fGetConfigurationMeta(NULL)
	,fGetVersion(NULL)
	,fCommandLineExec(NULL)
	,bLoaded_(false)
{
	if (other.bLoaded_) {
		file_ = other.file_;
		hModule_ = LoadLibrary(file_.c_str());
		if (!hModule_)
			throw NSPluginException(file_, _T("Could not load library: ") + error::lookup::last_error());
		loadRemoteProcs_();
		if (!fLoadModule)
			throw NSPluginException(file_, _T("Critical error (fLoadModule)"));
		bLoaded_ = other.bLoaded_;
	}
}

/**
 * Default d-tor
 */
NSCPlugin::~NSCPlugin() {
	if (isLoaded())
		unload();
}
/**
 * Returns the name of the plug in.
 *
 * @return Name of the plug in.
 *
 * @throws NSPluginException if the module is not loaded.
 */
std::wstring NSCPlugin::getName() {
	TCHAR *buffer = new TCHAR[1024];
	if (!getName_(buffer, 1023)) {
		throw NSPluginException(file_, _T("Could not get name"));
	}
	std::wstring ret = buffer;
	delete [] buffer;
	return ret;
}
std::wstring NSCPlugin::getDescription() {
	TCHAR *buffer = new TCHAR[4096];
	if (!getDescription_(buffer, 4095)) {
		throw NSPluginException(file_, _T("Could not get description"));
	}
	std::wstring ret = buffer;
	delete [] buffer;
	return ret;
}

/**
 * Loads the plug in (DLL) and initializes the plug in by calling NSLoadModule
 *
 * @throws NSPluginException when exceptions occur. 
 * Exceptions include but are not limited to: DLL fails to load, DLL is not a correct plug in.
 */
void NSCPlugin::load_dll() {
	if (isLoaded())
		throw NSPluginException(file_, _T("Module already loaded"));
	hModule_ = LoadLibrary(file_.c_str());
	if (!hModule_)
		throw NSPluginException(file_, _T("Could not load library: ") + error::lookup::last_error());
	loadRemoteProcs_();
	bLoaded_ = true;
}

void NSCPlugin::load_plugin() {
	if (!fLoadModule)
		throw NSPluginException(file_, _T("Critical error (fLoadModule)"));
	if (!fLoadModule())
		throw NSPluginException(file_, _T("Could not load plug in"));
}


/**
 * Get the plug in version.
 *
 * @bug Not implemented as of yet
 *
 * @param *major 
 * @param *minor 
 * @param *revision 
 * @return False
 */
bool NSCPlugin::getVersion(int *major, int *minor, int *revision) {
	if (!isLoaded())
		throw NSPluginException(file_, _T("Library is not loaded"));
	if (!fGetVersion)
		throw NSPluginException(file_, _T("Critical error (fGetVersion)"));
	return fGetVersion(major, minor, revision)?true:false;
}
/**
 * Returns true if the plug in has a command handler.
 * @return true if the plug in has a command handler.
 * @throws NSPluginException if the module is not loaded.
 */
bool NSCPlugin::hasCommandHandler() {
	if (!isLoaded())
		throw NSPluginException(file_, _T("Module not loaded"));
	if (fHasCommandHandler())
		return true;
	return false;
}
/**
* Returns true if the plug in has a message (log) handler.
* @return true if the plug in has a message (log) handler.
* @throws NSPluginException if the module is not loaded.
*/
bool NSCPlugin::hasMessageHandler() {
	if (!isLoaded())
		throw NSPluginException(file_, _T("Module not loaded"));
	if (fHasMessageHandler())
		return true;
	return false;
}
/**
 * Allow for the plug in to handle a command from the input core.
 * 
 * Plug ins may refuse to handle the plug in (if not applicable) by returning an empty string.
 *
 * @param command The command name (is a string encoded number for legacy commands)
 * @param argLen The length of the argument buffer.
 * @param **arguments The arguments for this command
 * @param returnMessageBuffer Return buffer for plug in to store the result of the executed command.
 * @param returnMessageBufferLen Size of returnMessageBuffer
 * @param returnPerfBuffer Return buffer for performance data
 * @param returnPerfBufferLen Sixe of returnPerfBuffer
 * @return Status of execution. Could be error codes, buffer length messages etc.
 * @throws NSPluginException if the module is not loaded.
 */
NSCAPI::nagiosReturn NSCPlugin::handleCommand(const TCHAR* command, const unsigned int argLen, TCHAR **arguments, TCHAR* returnMessageBuffer, unsigned int returnMessageBufferLen, TCHAR* returnPerfBuffer, unsigned int returnPerfBufferLen) {
	if (!isLoaded())
		throw NSPluginException(file_, _T("Library is not loaded"));
	return fHandleCommand(command, argLen, arguments, returnMessageBuffer, returnMessageBufferLen, returnPerfBuffer, returnPerfBufferLen);
}
/**
 * Handle a message from the core (or any other (or even potentially self) plug in).
 * A message may be anything really errors, log messages etc.
 *
 * @param msgType Type of message (error, warning, debug, etc.)
 * @param file The file that generated this message generally __FILE__.
 * @param line The line in the file that generated the message generally __LINE__
 * @throws NSPluginException if the module is not loaded.
 */
void NSCPlugin::handleMessage(int msgType, const TCHAR* file, const int line, const TCHAR *message) {
	if (!fHandleMessage)
		throw NSPluginException(file_, _T("Library is not loaded"));
	fHandleMessage(msgType, file, line, message);
}
/**
 * Unload the plug in
 * @throws NSPluginException if the module is not loaded and/or cannot be unloaded (plug in remains loaded if so).
 */
void NSCPlugin::unload() {
	if (!isLoaded())
		throw NSPluginException(file_, _T("Library is not loaded"));
	if (!fUnLoadModule)
		throw NSPluginException(file_, _T("Critical error (fUnLoadModule)"));
	fUnLoadModule();
	FreeLibrary(hModule_);
	hModule_ = NULL;
	bLoaded_ = false;
}
bool NSCPlugin::getName_(TCHAR* buf, unsigned int buflen) {
	if (fGetName == NULL)
		throw NSPluginException(file_, _T("Critical error (fGetName)"));
	return fGetName(buf, buflen)?true:false;
}
bool NSCPlugin::getDescription_(TCHAR* buf, unsigned int buflen) {
	if (fGetDescription == NULL)
		throw NSPluginException(file_, _T("Critical error (fGetDescription)"));
	return fGetDescription(buf, buflen)?true:false;
}
/**
 * Load all remote function pointers from the loaded module.
 * These pointers are cached for "speed" which might (?) be dangerous if something changes.
 * @throws NSPluginException if any of the function pointers fail to load.
 * If NSPluginException  is thrown the loaded might remain partially loaded and crashes might occur if plug in is used in this state.
 */
void NSCPlugin::loadRemoteProcs_(void) {

	fLoadModule = (lpLoadModule)GetProcAddress(hModule_, "NSLoadModule");
	if (!fLoadModule)
		throw NSPluginException(file_, _T("Could not load NSLoadModule"));

	fModuleHelperInit = (lpModuleHelperInit)GetProcAddress(hModule_, "NSModuleHelperInit");
	if (!fModuleHelperInit)
		throw NSPluginException(file_, _T("Could not load NSModuleHelperInit"));

	fModuleHelperInit(NSAPILoader);
	
	fGetName = (lpGetName)GetProcAddress(hModule_, "NSGetModuleName");
	if (!fGetName)
		throw NSPluginException(file_, _T("Could not load NSGetModuleName"));

	fGetVersion = (lpGetVersion)GetProcAddress(hModule_, "NSGetModuleVersion");
	if (!fGetVersion)
		throw NSPluginException(file_, _T("Could not load NSGetModuleVersion"));

	fGetDescription = (lpGetDescription)GetProcAddress(hModule_, "NSGetModuleDescription");
	if (!fGetDescription)
		throw NSPluginException(file_, _T("Could not load NSGetModuleDescription"));

	fHasCommandHandler = (lpHasCommandHandler)GetProcAddress(hModule_, "NSHasCommandHandler");
	if (!fHasCommandHandler)
		throw NSPluginException(file_, _T("Could not load NSHasCommandHandler"));

	fHasMessageHandler = (lpHasMessageHandler)GetProcAddress(hModule_, "NSHasMessageHandler");
	if (!fHasMessageHandler)
		throw NSPluginException(file_, _T("Could not load NSHasMessageHandler"));

	fHandleCommand = (lpHandleCommand)GetProcAddress(hModule_, "NSHandleCommand");
	if (!fHandleCommand)
		throw NSPluginException(file_, _T("Could not load NSHandleCommand"));

	fHandleMessage = (lpHandleMessage)GetProcAddress(hModule_, "NSHandleMessage");
	if (!fHandleMessage)
		throw NSPluginException(file_, _T("Could not load NSHandleMessage"));

	fUnLoadModule = (lpUnLoadModule)GetProcAddress(hModule_, "NSUnloadModule");
	if (!fUnLoadModule)
		throw NSPluginException(file_, _T("Could not load NSUnloadModule"));

	fGetConfigurationMeta = (lpGetConfigurationMeta)GetProcAddress(hModule_, "NSGetConfigurationMeta");
	fCommandLineExec = (lpCommandLineExec)GetProcAddress(hModule_, "NSCommandLineExec");
}


std::wstring NSCPlugin::getCongifurationMeta() 
{
	TCHAR *buffer = new TCHAR[4097];
	if (!getConfigurationMeta_(buffer, 4096)) {
		throw NSPluginException(file_, _T("Could not get metadata"));
	}
	std::wstring ret = buffer;
	delete [] buffer;
	return ret;
}
bool NSCPlugin::getConfigurationMeta_(TCHAR* buf, unsigned int buflen) {
	if (fGetConfigurationMeta == NULL)
		throw NSPluginException(file_, _T("Critical error (getCongifurationMeta)"));
	return fGetConfigurationMeta(buflen, buf)?true:false;
}

int NSCPlugin::commandLineExec(const TCHAR* command, const unsigned int argLen, TCHAR **arguments) {
	if (fCommandLineExec== NULL)
		throw NSPluginException(file_, _T("Module does not support CommandLineExec"));
	return fCommandLineExec(command, argLen, arguments);
}
