/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "NSCPlugin.h"
#include "core_api.h"
#include "NSCAPI.h"

/**
 * Default c-tor
 * Initializes the plug in name but does not load the actual plug in.<br>
 * To load the plug in use function load() that loads an initializes the plug in.
 *
 * @param file The file (DLL) to load as a NSC plug in.
 */
NSCPlugin::NSCPlugin(const unsigned int id, const boost::filesystem::path file, std::string alias)
	: module_(file)
	, loaded_(false)
	, broken_(false)
	, plugin_id_(id)
	, alias_(alias)
	, lastIsMsgPlugin_(false)
	, fModuleHelperInit(NULL)
	, fLoadModule(NULL)
	, fGetName(NULL)
	, fGetVersion(NULL)
	, fGetDescription(NULL)
	, fHasCommandHandler(NULL)
	, fHasMessageHandler(NULL)
	, fHandleCommand(NULL)
	, fHandleMessage(NULL)
	, fDeleteBuffer(NULL)
	, fUnLoadModule(NULL)
	, fCommandLineExec(NULL)
	, fHasNotificationHandler(NULL)
	, fHandleNotification(NULL)
	, fHasRoutingHandler(NULL)
	, fRouteMessage(NULL)
	, fFetchMetrics(NULL)
	, fSubmitMetrics(NULL) 
	, fOnEvent(NULL)
{}
/**
 * Default d-tor
 */
NSCPlugin::~NSCPlugin() {
	if (isLoaded()) {
		try {
			unload_plugin();
			unload_dll();
		} catch (const NSPluginException&) {
			// ...
		}
	}
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
		return "Could not get name";
	}
	std::string ret = buffer;
	delete[] buffer;
	return ret;
}
std::string NSCPlugin::getDescription() {
	char *buffer = new char[4096];
	if (!getDescription_(buffer, 4095)) {
		throw NSPluginException(get_alias_or_name(), "Could not get description");
	}
	std::string ret = buffer;
	delete[] buffer;
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
		throw NSPluginException(get_alias_or_name(), "Module already loaded");
	try {
		module_.load_library();
	} catch (dll::dll_exception &e) {
		throw NSPluginException(get_alias_or_name(), e.what());
	}
	loadRemoteProcs_();
}

bool NSCPlugin::load_plugin(NSCAPI::moduleLoadMode mode) {
	if (loaded_ && mode != NSCAPI::reloadStart)
		return true;
	if (!fLoadModule)
		throw NSPluginException(get_alias_or_name(), "Critical error (fLoadModule)");
	if (fLoadModule(plugin_id_, alias_.c_str(), mode)) {
		loaded_ = true;
		return true;
	}
	return false;
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
		throw NSPluginException(get_alias_or_name(), "Library is not loaded");
	if (!fGetVersion)
		throw NSPluginException(get_alias_or_name(), "Critical error (fGetVersion)");
	try {
		return fGetVersion(major, minor, revision) ? true : false;
	} catch (...) {
		throw NSPluginException(get_alias_or_name(), "Unhandled exception in getVersion.");
	}
}
/**
 * Returns true if the plug in has a command handler.
 * @return true if the plug in has a command handler.
 * @throws NSPluginException if the module is not loaded.
 */
bool NSCPlugin::hasCommandHandler() {
	if (!isLoaded())
		throw NSPluginException(get_alias_or_name(), "Module not loaded");
	try {
		if (fHasCommandHandler(plugin_id_))
			return true;
		return false;
	} catch (...) {
		throw NSPluginException(get_alias_or_name(), "Unhandled exception in hasCommandHandler.");
	}
}
/**
* Returns true if the plug in has a message (log) handler.
* @return true if the plug in has a message (log) handler.
* @throws NSPluginException if the module is not loaded.
*/
bool NSCPlugin::hasMessageHandler() {
	if (!isLoaded())
		throw NSPluginException(get_alias_or_name(), "Module not loaded");
	try {
		if (fHasMessageHandler(plugin_id_)) {
			lastIsMsgPlugin_ = true;
			return true;
		}
		return false;
	} catch (...) {
		throw NSPluginException(get_alias_or_name(), "Unhandled exception in hasMessageHandler.");
	}
}
bool NSCPlugin::hasNotificationHandler() {
	if (!isLoaded())
		throw NSPluginException(get_alias_or_name(), "Module not loaded");
	if (!fHasNotificationHandler)
		return false;
	try {
		if (fHasNotificationHandler(plugin_id_)) {
			return true;
		}
		return false;
	} catch (...) {
		throw NSPluginException(get_alias_or_name(), "Unhandled exception in hasMessageHandler.");
	}
}
bool NSCPlugin::has_routing_handler() {
	if (!isLoaded())
		throw NSPluginException(get_alias_or_name(), "Module not loaded");
	if (!fHasRoutingHandler)
		return false;
	try {
		if (fHasRoutingHandler(plugin_id_)) {
			return true;
		}
		return false;
	} catch (...) {
		throw NSPluginException(get_alias_or_name(), "Unhandled exception in hasMessageHandler.");
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
NSCAPI::nagiosReturn NSCPlugin::handleCommand(const char* dataBuffer, unsigned int dataBuffer_len, char** returnBuffer, unsigned int *returnBuffer_len) {
	if (!isLoaded() || !loaded_ || fHandleCommand == NULL)
		throw NSPluginException(get_alias_or_name(), "Library is not loaded");
	try {
		return fHandleCommand(plugin_id_, dataBuffer, dataBuffer_len, returnBuffer, returnBuffer_len);
	} catch (...) {
		throw NSPluginException(get_alias_or_name(), "Unhandled exception in handleCommand.");
	}
}
NSCAPI::nagiosReturn NSCPlugin::handleCommand(const std::string request, std::string &reply) {
	char *buffer = NULL;
	unsigned int len = 0;
	NSCAPI::nagiosReturn ret = handleCommand(request.c_str(), request.size(), &buffer, &len);
	if (buffer != NULL) {
		reply = std::string(buffer, len);
		deleteBuffer(&buffer);
	}
	return ret;
}

NSCAPI::nagiosReturn NSCPlugin::handle_schedule(const char* dataBuffer, const unsigned int dataBuffer_len) {
	if (!isLoaded() || fHandleSchedule == NULL)
		throw NSPluginException(get_alias_or_name(), "Library is not loaded");
	try {
		return fHandleSchedule(plugin_id_, dataBuffer, dataBuffer_len);
	} catch (...) {
		throw NSPluginException(get_alias_or_name(), "Unhandled exception in handle_schedule.");
	}
}

NSCAPI::nagiosReturn NSCPlugin::handle_schedule(const std::string &request) {
	return handle_schedule(request.c_str(), request.size());
}

NSCAPI::nagiosReturn NSCPlugin::handleNotification(const char *channel, std::string &request, std::string &reply) {
	char *buffer = NULL;
	unsigned int len = 0;
	NSCAPI::nagiosReturn ret = handleNotification(channel, request.c_str(), request.size(), &buffer, &len);
	if (buffer != NULL) {
		reply = std::string(buffer, len);
		deleteBuffer(&buffer);
	}
	return ret;
}

NSCAPI::nagiosReturn NSCPlugin::handleNotification(const char *channel, const char* dataBuffer, const unsigned int dataBuffer_len, char** returnBuffer, unsigned int *returnBuffer_len) {
	if (!isLoaded() || !loaded_ || fHandleNotification == NULL)
		throw NSPluginException(get_alias_or_name(), "Library is not loaded");
	try {
		return fHandleNotification(plugin_id_, channel, dataBuffer, dataBuffer_len, returnBuffer, returnBuffer_len);
	} catch (...) {
		throw NSPluginException(get_alias_or_name(), "Unhandled exception in handleNotification.");
	}
}

NSCAPI::nagiosReturn NSCPlugin::on_event(const std::string &request) {
	return on_event(request.c_str(), request.size());
}
NSCAPI::nagiosReturn NSCPlugin::on_event(const char* request_buffer, const unsigned int request_buffer_len) {
	if (!isLoaded() || !loaded_ || fOnEvent == NULL)
		throw NSPluginException(get_alias_or_name(), "Library is not loaded");
	try {
		return fOnEvent(plugin_id_, request_buffer, request_buffer_len);
	} catch (...) {
		throw NSPluginException(get_alias_or_name(), "Unhandled exception in OnEvent.");
	}
}
bool NSCPlugin::has_on_event() {
	if (!isLoaded())
		throw NSPluginException(get_alias_or_name(), "Module not loaded");
	return fOnEvent != NULL;
}


NSCAPI::nagiosReturn NSCPlugin::fetchMetrics(std::string &request) {
	char *buffer = NULL;
	unsigned int len = 0;
	NSCAPI::nagiosReturn ret = fetchMetrics(&buffer, &len);
	if (buffer != NULL) {
		request = std::string(buffer, len);
		deleteBuffer(&buffer);
	}
	return ret;
}

NSCAPI::nagiosReturn NSCPlugin::fetchMetrics(char** returnBuffer, unsigned int *returnBuffer_len) {
	if (!isLoaded() || !loaded_ || fFetchMetrics == NULL)
		throw NSPluginException(get_alias_or_name(), "Library is not loaded");
	try {
		return fFetchMetrics(plugin_id_, returnBuffer, returnBuffer_len);
	} catch (...) {
		throw NSPluginException(get_alias_or_name(), "Unhanded exception in fFetchMetrics.");
	}
}

NSCAPI::nagiosReturn NSCPlugin::submitMetrics(const std::string &request) {
	return submitMetrics(request.c_str(), request.size());
}

NSCAPI::nagiosReturn NSCPlugin::submitMetrics(const char* buffer, const unsigned int buffer_len) {
	if (!isLoaded() || !loaded_ || fSubmitMetrics == NULL)
		throw NSPluginException(get_alias_or_name(), "Library is not loaded");
	try {
		return fSubmitMetrics(plugin_id_, buffer, buffer_len);
	} catch (...) {
		throw NSPluginException(get_alias_or_name(), "Unhanded exception in SubmitMetrics.");
	}
}

bool NSCPlugin::route_message(const char *channel, const char* buffer, unsigned int buffer_len, char **new_channel_buffer, char **new_buffer, unsigned int *new_buffer_len) {
	if (!isLoaded() || !loaded_ || fRouteMessage == NULL)
		throw NSPluginException(get_alias_or_name(), "Library is not loaded");
	try {
		return fRouteMessage(plugin_id_, channel, buffer, buffer_len, new_channel_buffer, new_buffer, new_buffer_len);
	} catch (...) {
		throw NSPluginException(get_alias_or_name(), "Unhandled exception in route_message.");
	}
}

void NSCPlugin::deleteBuffer(char** buffer) {
	if (!isLoaded())
		throw NSPluginException(get_alias_or_name(), "Library is not loaded");
	try {
		fDeleteBuffer(buffer);
	} catch (...) {
		throw NSPluginException(get_alias_or_name(), "Unhandled exception in deleteBuffer.");
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
void NSCPlugin::handleMessage(const char* data, unsigned int len) {
	if (!fHandleMessage)
		throw NSPluginException(get_alias_or_name(), "Library is not loaded");
	try {
		fHandleMessage(plugin_id_, data, len);
	} catch (...) {
		throw NSPluginException(get_alias_or_name(), "Unhandled exception in handleMessage.");
	}
}
/**
 * Unload the plug in
 * @throws NSPluginException if the module is not loaded and/or cannot be unloaded (plug in remains loaded if so).
 */
void NSCPlugin::unload_plugin() {
	if (!isLoaded())
		return;
	loaded_ = false;
	if (!fUnLoadModule)
		throw NSPluginException(get_alias_or_name(), "Critical error (fUnLoadModule)");
	try {
		fUnLoadModule(plugin_id_);
	} catch (...) {
		throw NSPluginException(get_alias_or_name(), "Unhandled exception in fUnLoadModule.");
	}
}
void NSCPlugin::unload_dll() {
	if (isLoaded())
		return;
	fModuleHelperInit = NULL;
	fLoadModule = NULL;
	fGetName = NULL;
	fGetVersion = NULL;
	fGetDescription = NULL;
	fHasCommandHandler = NULL;
	fHasMessageHandler = NULL;
	fHandleCommand = NULL;
	fDeleteBuffer = NULL;
	fHandleMessage = NULL;
	fUnLoadModule = NULL;
	fCommandLineExec = NULL;
	fHasNotificationHandler = NULL;
	fHandleNotification = NULL;
	fHasRoutingHandler = NULL;
	fRouteMessage = NULL;
	fHandleSchedule = NULL;
	fFetchMetrics = NULL;
	fSubmitMetrics = NULL;
	fOnEvent = NULL;
	module_.unload_library();
}
bool NSCPlugin::getName_(char* buf, unsigned int buflen) {
	if (fGetName == NULL)
		return false;
	try {
		return fGetName(buf, buflen) ? true : false;
	} catch (...) {
		return false;
	}
}
bool NSCPlugin::getDescription_(char* buf, unsigned int buflen) {
	if (fGetDescription == NULL)
		throw NSPluginException(get_alias_or_name(), "Critical error (fGetDescription)");
	try {
		return fGetDescription(buf, buflen) ? true : false;
	} catch (...) {
		throw NSPluginException(get_alias_or_name(), "Unhandled exception in getDescription.");
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
		fLoadModule = (nscapi::plugin_api::lpLoadModule)module_.load_proc("NSLoadModuleEx");
		if (!fLoadModule)
			throw NSPluginException(get_alias_or_name(), "Could not load NSLoadModuleEx");

		fModuleHelperInit = (nscapi::plugin_api::lpModuleHelperInit)module_.load_proc("NSModuleHelperInit");
		if (!fModuleHelperInit)
			throw NSPluginException(get_alias_or_name(), "Could not load NSModuleHelperInit");

		try {
			fModuleHelperInit(get_id(), &NSAPILoader);
		} catch (...) {
			throw NSPluginException(get_alias_or_name(), "Unhandled exception in getDescription.");
		}

		fGetName = (nscapi::plugin_api::lpGetName)module_.load_proc("NSGetModuleName");
		if (!fGetName)
			throw NSPluginException(get_alias_or_name(), "Could not load NSGetModuleName");

		fGetVersion = (nscapi::plugin_api::lpGetVersion)module_.load_proc("NSGetModuleVersion");
		if (!fGetVersion)
			throw NSPluginException(get_alias_or_name(), "Could not load NSGetModuleVersion");

		fGetDescription = (nscapi::plugin_api::lpGetDescription)module_.load_proc("NSGetModuleDescription");
		if (!fGetDescription)
			throw NSPluginException(get_alias_or_name(), "Could not load NSGetModuleDescription");

		fHasCommandHandler = (nscapi::plugin_api::lpHasCommandHandler)module_.load_proc("NSHasCommandHandler");
		if (!fHasCommandHandler)
			throw NSPluginException(get_alias_or_name(), "Could not load NSHasCommandHandler");

		fHasMessageHandler = (nscapi::plugin_api::lpHasMessageHandler)module_.load_proc("NSHasMessageHandler");
		if (!fHasMessageHandler)
			throw NSPluginException(get_alias_or_name(), "Could not load NSHasMessageHandler");

		fHandleCommand = (nscapi::plugin_api::lpHandleCommand)module_.load_proc("NSHandleCommand");

		fDeleteBuffer = (nscapi::plugin_api::lpDeleteBuffer)module_.load_proc("NSDeleteBuffer");
		if (!fDeleteBuffer)
			throw NSPluginException(get_alias_or_name(), "Could not load NSDeleteBuffer");

		fHandleMessage = (nscapi::plugin_api::lpHandleMessage)module_.load_proc("NSHandleMessage");
		if (!fHandleMessage)
			throw NSPluginException(get_alias_or_name(), "Could not load NSHandleMessage");

		fUnLoadModule = (nscapi::plugin_api::lpUnLoadModule)module_.load_proc("NSUnloadModule");
		if (!fUnLoadModule)
			throw NSPluginException(get_alias_or_name(), "Could not load NSUnloadModule");

		fCommandLineExec = (nscapi::plugin_api::lpCommandLineExec)module_.load_proc("NSCommandLineExec");
		fHandleNotification = (nscapi::plugin_api::lpHandleNotification)module_.load_proc("NSHandleNotification");
		fHasNotificationHandler = (nscapi::plugin_api::lpHasNotificationHandler)module_.load_proc("NSHasNotificationHandler");

		fHasRoutingHandler = (nscapi::plugin_api::lpHasRoutingHandler)module_.load_proc("NSHasRoutingHandler");
		fRouteMessage = (nscapi::plugin_api::lpRouteMessage)module_.load_proc("NSRouteMessage");

		fHandleSchedule = (nscapi::plugin_api::lpHandleSchedule)module_.load_proc("NSHandleSchedule");
		fFetchMetrics = (nscapi::plugin_api::lpFetchMetrics)module_.load_proc("NSFetchMetrics");
		fSubmitMetrics = (nscapi::plugin_api::lpSubmitMetrics)module_.load_proc("NSSubmitMetrics");
		fOnEvent = (nscapi::plugin_api::lpOnEvent)module_.load_proc("NSOnEvent");
	} catch (NSPluginException &e) {
		throw e;
	} catch (dll::dll_exception &e) {
		throw NSPluginException(get_alias_or_name(), std::string("Unhanded exception when loading process: ") + e.what());
	} catch (...) {
		throw NSPluginException(get_alias_or_name(), "Unhandled exception when loading proces: <UNKNOWN>");
	}
}

int NSCPlugin::commandLineExec(bool targeted, std::string &request, std::string &reply) {
	char *buffer = NULL;
	unsigned int len = 0;
	NSCAPI::nagiosReturn ret = commandLineExec(targeted, request.c_str(), request.size(), &buffer, &len);
	if (buffer != NULL) {
		reply = std::string(buffer, len);
		deleteBuffer(&buffer);
	}
	return ret;
}

bool NSCPlugin::has_command_line_exec() {
	return isLoaded() && !loaded_ || fCommandLineExec != NULL;
}

int NSCPlugin::commandLineExec(bool targeted, const char* request, const unsigned int request_len, char** reply, unsigned int *reply_len) {
	if (!has_command_line_exec())
		throw NSPluginException(get_alias_or_name(), "Library is not loaded or modules does not support command line");
	try {
		return fCommandLineExec(plugin_id_, targeted ? NSCAPI::target_module : NSCAPI::target_any, request, request_len, reply, reply_len);
	} catch (...) {
		throw NSPluginException(get_alias_or_name(), "Unhanded exception in handleCommand.");
	}
}
boost::filesystem::path NSCPlugin::get_filename(boost::filesystem::path folder, std::string module) {
	return dll::dll_impl::fix_module_name(folder / module);
}
bool NSCPlugin::is_duplicate(boost::filesystem::path file, std::string alias) {
	if (alias.empty() && alias_.empty())
		return module_.get_file() == dll::dll_impl::fix_module_name(file);
	if (alias.empty() || alias_.empty())
		return false;
	return module_.get_file() == dll::dll_impl::fix_module_name(file) && alias == alias_;
}