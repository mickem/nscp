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
#include "StdAfx.h"
#include "NSCPlugin.h"
#include "core_api.h"

unsigned int NSCPlugin::last_plugin_id_ = 0;


/**
 * Default c-tor
 * Initializes the plug in name but does not load the actual plug in.<br>
 * To load the plug in use function load() that loads an initializes the plug in.
 *
 * @param file The file (DLL) to load as a NSC plug in.
 */
NSCPlugin::NSCPlugin(const boost::filesystem::wpath file)
	: module_(file.string())
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
	,fShowTray(NULL)
	,fHideTray(NULL)
	,bLoaded_(false)
	,lastIsMsgPlugin_(false)
	,broken_(false)
	,plugin_id_(0)
{
	plugin_id_ = ++last_plugin_id_;
}
/*
NSCPlugin::NSCPlugin(NSCPlugin &other)
	:module_()
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
	,fShowTray(NULL)
	,fHideTray(NULL)
	,bLoaded_(false)
	,lastIsMsgPlugin_(false)
	,broken_(false)
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
*/
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
	wchar_t *buffer = new wchar_t[1024];
	if (!getName_(buffer, 1023)) {
		return _T("Could not get name");
	}
	std::wstring ret = buffer;
	delete [] buffer;
	return ret;
}
std::wstring NSCPlugin::getDescription() {
	wchar_t *buffer = new wchar_t[4096];
	if (!getDescription_(buffer, 4095)) {
		throw NSPluginException(module_, _T("Could not get description"));
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
	if (module_.is_loaded())
		throw NSPluginException(module_, _T("Module already loaded"));
	try {
		module_.load_library();
	} catch (dll::dll_exception &e) {
		throw NSPluginException(module_, e.what());
	}
	loadRemoteProcs_();
	bLoaded_ = true;
}

bool NSCPlugin::load_plugin(NSCAPI::moduleLoadMode mode) {
	if (!fLoadModule)
		throw NSPluginException(module_, _T("Critical error (fLoadModule)"));
	return fLoadModule(mode);
}

void NSCPlugin::setBroken(bool broken) {
	broken_ = broken;
}
bool NSCPlugin::isBroken() {
	return broken_;
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
		throw NSPluginException(module_, _T("Library is not loaded"));
	if (!fGetVersion)
		throw NSPluginException(module_, _T("Critical error (fGetVersion)"));
	try {
		return fGetVersion(major, minor, revision)?true:false;
	} catch (...) {
		throw NSPluginException(module_, _T("Unhandled exception in getVersion."));
	}
}
/**
 * Returns true if the plug in has a command handler.
 * @return true if the plug in has a command handler.
 * @throws NSPluginException if the module is not loaded.
 */
bool NSCPlugin::hasCommandHandler() {
	if (!isLoaded())
		throw NSPluginException(module_, _T("Module not loaded"));
	try {
		if (fHasCommandHandler())
			return true;
		return false;
	} catch (...) {
		throw NSPluginException(module_, _T("Unhandled exception in hasCommandHandler."));
	}
}
/**
* Returns true if the plug in has a message (log) handler.
* @return true if the plug in has a message (log) handler.
* @throws NSPluginException if the module is not loaded.
*/
bool NSCPlugin::hasMessageHandler() {
	if (!isLoaded())
		throw NSPluginException(module_, _T("Module not loaded"));
	try {
		if (fHasMessageHandler()) {
			lastIsMsgPlugin_ = true;
			return true;
		}
		return false;
	} catch (...) {
		throw NSPluginException(module_, _T("Unhandled exception in hasMessageHandler."));
	}
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
 * @param returnPerfBufferLen Size of returnPerfBuffer
 * @return Status of execution. Could be error codes, buffer length messages etc.
 * @throws NSPluginException if the module is not loaded.
 */
NSCAPI::nagiosReturn NSCPlugin::handleCommand(const wchar_t* command, const unsigned int argLen, wchar_t **arguments, wchar_t* returnMessageBuffer, unsigned int returnMessageBufferLen, wchar_t* returnPerfBuffer, unsigned int returnPerfBufferLen) {
	if (!isLoaded())
		throw NSPluginException(module_, _T("Library is not loaded"));
	try {
		return fHandleCommand(command, argLen, arguments, returnMessageBuffer, returnMessageBufferLen, returnPerfBuffer, returnPerfBufferLen);
	} catch (...) {
		throw NSPluginException(module_, _T("Unhandled exception in handleCommand."));
	}
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
void NSCPlugin::handleMessage(int msgType, const wchar_t* file, const int line, const wchar_t *message) {
	if (!fHandleMessage)
		throw NSPluginException(module_, _T("Library is not loaded"));
	try {
		fHandleMessage(msgType, file, line, message);
	} catch (...) {
		throw NSPluginException(module_, _T("Unhandled exception in handleMessage."));
	}
}
/**
 * Unload the plug in
 * @throws NSPluginException if the module is not loaded and/or cannot be unloaded (plug in remains loaded if so).
 */
void NSCPlugin::unload() {
	if (!isLoaded())
		throw NSPluginException(module_, _T("Library is not loaded"));
	bLoaded_ = false;
	if (!fUnLoadModule)
		throw NSPluginException(module_, _T("Critical error (fUnLoadModule)"));
	try {
		fUnLoadModule();
	} catch (...) {
		throw NSPluginException(module_, _T("Unhandled exception in handleMessage."));
	}
	module_.unload_library();
}
bool NSCPlugin::getName_(wchar_t* buf, unsigned int buflen) {
	if (fGetName == NULL)
		return false;//throw NSPluginException(module_, _T("Critical error (fGetName)"));
	try {
		return fGetName(buf, buflen)?true:false;
	} catch (...) {
		return false; //throw NSPluginException(module_, _T("Unhandled exception in getName."));
	}
}
bool NSCPlugin::getDescription_(wchar_t* buf, unsigned int buflen) {
	if (fGetDescription == NULL)
		throw NSPluginException(module_, _T("Critical error (fGetDescription)"));
	try {
		return fGetDescription(buf, buflen)?true:false;
	} catch (...) {
		throw NSPluginException(module_, _T("Unhandled exception in getDescription."));
	}
}

void NSCPlugin::showTray() {
	if (fShowTray == NULL)
		throw NSPluginException(module_, _T("Critical error (ShowTray)"));
	try {
		fShowTray();
	} catch (...) {
		throw NSPluginException(module_, _T("Unhandled exception in ShowTray."));
	}
}
void NSCPlugin::hideTray() {
	if (fHideTray == NULL)
		throw NSPluginException(module_, _T("Critical error (HideTray)"));
	try {
		fHideTray();
	} catch (...) {
		throw NSPluginException(module_, _T("Unhandled exception in HideTray."));
	}
}

/**
 * Load all remote function pointers from the loaded module.
 * These pointers are cached for "speed" which might (?) be dangerous if something changes.
 * @throws NSPluginException if any of the function pointers fail to load.
 * If NSPluginException  is thrown the loaded might remain partially loaded and crashes might occur if plug in is used in this state.
 */
void NSCPlugin::loadRemoteProcs_(void) {

	try {
		fLoadModule = (lpLoadModule)module_.load_proc("NSLoadModule");
		if (!fLoadModule)
			throw NSPluginException(module_, _T("Could not load NSLoadModule"));

		fModuleHelperInit = (lpModuleHelperInit)module_.load_proc("NSModuleHelperInit");
		if (!fModuleHelperInit)
			throw NSPluginException(module_, _T("Could not load NSModuleHelperInit"));

		try {
			fModuleHelperInit(get_id(), NSAPILoader);
		} catch (...) {
			throw NSPluginException(module_, _T("Unhandled exception in getDescription."));
		}

		fGetName = (lpGetName)module_.load_proc("NSGetModuleName");
		if (!fGetName)
			throw NSPluginException(module_, _T("Could not load NSGetModuleName"));

		fGetVersion = (lpGetVersion)module_.load_proc("NSGetModuleVersion");
		if (!fGetVersion)
			throw NSPluginException(module_, _T("Could not load NSGetModuleVersion"));

		fGetDescription = (lpGetDescription)module_.load_proc("NSGetModuleDescription");
		if (!fGetDescription)
			throw NSPluginException(module_, _T("Could not load NSGetModuleDescription"));

		fHasCommandHandler = (lpHasCommandHandler)module_.load_proc("NSHasCommandHandler");
		if (!fHasCommandHandler)
			throw NSPluginException(module_, _T("Could not load NSHasCommandHandler"));

		fHasMessageHandler = (lpHasMessageHandler)module_.load_proc("NSHasMessageHandler");
		if (!fHasMessageHandler)
			throw NSPluginException(module_, _T("Could not load NSHasMessageHandler"));

		fHandleCommand = (lpHandleCommand)module_.load_proc("NSHandleCommand");
		if (!fHandleCommand)
			throw NSPluginException(module_, _T("Could not load NSHandleCommand"));

		fHandleMessage = (lpHandleMessage)module_.load_proc("NSHandleMessage");
		if (!fHandleMessage)
			throw NSPluginException(module_, _T("Could not load NSHandleMessage"));

		fUnLoadModule = (lpUnLoadModule)module_.load_proc("NSUnloadModule");
		if (!fUnLoadModule)
			throw NSPluginException(module_, _T("Could not load NSUnloadModule"));

		fGetConfigurationMeta = (lpGetConfigurationMeta)module_.load_proc("NSGetConfigurationMeta");
		fCommandLineExec = (lpCommandLineExec)module_.load_proc("NSCommandLineExec");

		fShowTray = (lpShowTray)module_.load_proc("ShowIcon");
		fHideTray = (lpHideTray)module_.load_proc("HideIcon");
	} catch (dll::dll_exception &e) {
		throw NSPluginException(module_, _T("Unhandled exception when loading proces: ") + e.what());
	} catch (...) {
		throw NSPluginException(module_, _T("Unhandled exception when loading proces: <UNKNOWN>"));
	}

}


std::wstring NSCPlugin::getCongifurationMeta() 
{
	wchar_t *buffer = new wchar_t[4097];
	if (!getConfigurationMeta_(buffer, 4096)) {
		throw NSPluginException(module_, _T("Could not get metadata"));
	}
	std::wstring ret = buffer;
	delete [] buffer;
	return ret;
}
bool NSCPlugin::getConfigurationMeta_(wchar_t* buf, unsigned int buflen) {
	if (fGetConfigurationMeta == NULL)
		throw NSPluginException(module_, _T("Critical error (getCongifurationMeta)"));
	try {
		return fGetConfigurationMeta(buflen, buf)?true:false;
	} catch (...) {
		throw NSPluginException(module_, _T("Unhandled exception in getConfigurationMeta."));
	}
}

int NSCPlugin::commandLineExec(const wchar_t* command, const unsigned int argLen, wchar_t **arguments) {
	if (fCommandLineExec== NULL)
		throw NSPluginException(module_, _T("Module does not support CommandLineExec"));
	try {
		return fCommandLineExec(command, argLen, arguments);
	} catch (...) {
		throw NSPluginException(module_, _T("Unhandled exception in commandLineExec."));
	}
}
