#include "stdafx.h"
#include "NSClient++.h"
/**
 * Default c-tor
 * Initializes the plug in name but does not load the actual plug in.<br>
 * To load the plug in use function load() that loads an initializes the plug in.
 *
 * @param file The file (DLL) to load as a NSC plug in.
 */
NSCPlugin::NSCPlugin(const std::string file)
	: file_(file)
	,fLoadModule(NULL)
	,fGetName(NULL)
	,fHasCommandHandler(NULL)
	,fUnLoadModule(NULL)
	,fHasMessageHandler(NULL)
	,fHandleMessage(NULL)
	,bLoaded_(false)
{
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
std::string NSCPlugin::getName() {
	char *buffer = new char[1024];
	if (!getName_(buffer, 1023)) {
		throw NSPluginException(file_, "Could not get name");
	}
	std::string ret = buffer;
	delete [] buffer;
	return ret;
}
/**
 * Loads the plug in (DLL) and initializes the plug in by calling NSLoadModule
 *
 * @throws NSPluginException when exceptions occur. 
 * Exceptions include but are not limited to: DLL fails to load, DLL is not a correct plug in.
 */
void NSCPlugin::load() {
	if (isLoaded())
		throw NSPluginException(file_, "Module already loaded");
	hModule_ = LoadLibrary(file_.c_str());
	if (!hModule_)
		throw NSPluginException(file_, "Could not load library: ", GetLastError());
	loadRemoteProcs_();
	if (!fLoadModule)
		throw NSPluginException(file_, "Critical error (fLoadModule)");
	if (!fLoadModule())
		throw NSPluginException(file_, "Could not load plug in");
	bLoaded_ = true;
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
	return false;
}
/**
 * Returns true if the plug in has a command handler.
 * @return true if the plug in has a command handler.
 * @throws NSPluginException if the module is not loaded.
 */
bool NSCPlugin::hasCommandHandler() {
	if (!isLoaded())
		throw NSPluginException(file_, "Module not loaded");
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
		throw NSPluginException(file_, "Module not loaded");
	if (fHasMessageHandler())
		return true;
	return false;
}
/**
 * Allow for the plug in to handle a command from the input core.
 * 
 * Plug ins may refuse to handle the plug in (if not applicable) by returning an empty string.
 *
 * @param *command The command name (is a string encoded number for legacy commands)
 * @param argLen The length of the argument buffer.
 * @param **arguments The arguments for this command
 * @param returnBuffer Return buffer for plug in to store the result of the executed command.
 * @param returnBufferLen Size of the return buffer.
 * @return Status of execution. Could be error codes, buffer length messages etc.
 * @throws NSPluginException if the module is not loaded.
 *
 * @todo Implement return status as an enum to make it simpler for clients to see potential return stats?
 */
NSCAPI::nagiosReturn NSCPlugin::handleCommand(const char *command, const unsigned int argLen, char **arguments, char* returnMessageBuffer, unsigned int returnMessageBufferLen, char* returnPerfBuffer, unsigned int returnPerfBufferLen) {
	if (!isLoaded())
		throw NSPluginException(file_, "Library is not loaded");
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
 * @throws 
 */
void NSCPlugin::handleMessage(int msgType, const char* file, const int line, const char *message) {
	if (!fHandleMessage)
		throw NSPluginException(file_, "Library is not loaded");
	fHandleMessage(msgType, file, line, message);
}
/**
 * Unload the plug in
 * @throws NSPluginException if the module is not loaded and/or cannot be unloaded (plug in remains loaded if so).
 */
void NSCPlugin::unload() {
	if (!isLoaded())
		throw NSPluginException(file_, "Library is not loaded");
	if (!fUnLoadModule)
		throw NSPluginException(file_, "Critical error (fUnLoadModule)");
	fUnLoadModule();
	FreeLibrary(hModule_);
	hModule_ = NULL;
	bLoaded_ = false;
}
bool NSCPlugin::getName_(char* buf, unsigned int buflen) {
	if (fGetName == NULL)
		throw NSPluginException(file_, "Critical error (fGetName)");
	return fGetName(buf, buflen)?true:false;
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
		throw NSPluginException(file_, "Could not load NSLoadModule");

	fModuleHelperInit = (lpModuleHelperInit)GetProcAddress(hModule_, "NSModuleHelperInit");
	if (!fModuleHelperInit)
		throw NSPluginException(file_, "Could not load NSModuleHelperInit");

	fModuleHelperInit(NSAPILoader);

	fGetName = (lpGetName)GetProcAddress(hModule_, "NSGetModuleName");
	if (!fGetName)
		throw NSPluginException(file_, "Could not load NSGetModuleName");

	fHasCommandHandler = (lpHasCommandHandler)GetProcAddress(hModule_, "NSHasCommandHandler");
	if (!fHasCommandHandler)
		throw NSPluginException(file_, "Could not load NSHasCommandHandler");

	fHasMessageHandler = (lpHasMessageHandler)GetProcAddress(hModule_, "NSHasMessageHandler");
	if (!fHasMessageHandler)
		throw NSPluginException(file_, "Could not load NSHasMessageHandler");

	fHandleCommand = (lpHandleCommand)GetProcAddress(hModule_, "NSHandleCommand");
	if (!fHandleCommand)
		throw NSPluginException(file_, "Could not load NSHandleCommand");

	fHandleMessage = (lpHandleMessage)GetProcAddress(hModule_, "NSHandleMessage");
	if (!fHandleMessage)
		throw NSPluginException(file_, "Could not load NSHandleMessage");

	fUnLoadModule = (lpUnLoadModule)GetProcAddress(hModule_, "NSUnloadModule");
	if (!fUnLoadModule)
		throw NSPluginException(file_, "Could not load NSUnloadModule");
}


