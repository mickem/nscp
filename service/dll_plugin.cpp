/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dll_plugin.h"

#include "core_api.h"
#include "NSCAPI.h"

#include <str/xtos.hpp>

/**
 * Default c-tor
 * Initializes the plug in name but does not load the actual plug in.<br>
 * To load the plug in use function load() that loads an initializes the plug in.
 *
 * @param file The file (DLL) to load as a NSC plug in.
 */
nsclient::core::dll_plugin::dll_plugin(const unsigned int id, const boost::filesystem::path file, std::string alias)
	: plugin_interface(id, alias)
	, module_(file)
	, loaded_(false)
	, loading_(false)
	, broken_(false)
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
{
	load_dll();
}
/**
 * Default d-tor
 */
nsclient::core::dll_plugin::~dll_plugin() {
	if (isLoaded()) {
		try {
			unload_plugin();
		} catch (const plugin_exception&) {
			// ...
		}
	}
	try {
		unload_dll();
	} catch (const plugin_exception&) {
		// ...
	}
}
/**
 * Returns the name of the plug in.
 *
 * @return Name of the plug in.
 *
 * @throws NSPluginException if the module is not loaded.
 */
std::string nsclient::core::dll_plugin::getName() {
	char *buffer = new char[1024];
	if (!getName_(buffer, 1023)) {
		return "Could not get name";
	}
	std::string ret = buffer;
	delete[] buffer;
	return ret;
}
std::string nsclient::core::dll_plugin::getDescription() {
	char *buffer = new char[4096];
	if (!getDescription_(buffer, 4095)) {
		throw plugin_exception(get_alias_or_name(), "Could not get description");
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
void nsclient::core::dll_plugin::load_dll() {
	if (module_.is_loaded())
		throw plugin_exception(get_alias_or_name(), "Module already loaded");
	try {
		module_.load_library();
	} catch (dll::dll_exception &e) {
		throw plugin_exception(get_alias_or_name(), e.what());
	}
	loadRemoteProcs_();
}

bool nsclient::core::dll_plugin::load_plugin(NSCAPI::moduleLoadMode mode) {
	if ((loaded_ || loading_) && mode != NSCAPI::reloadStart)
		return true;
	if (!fLoadModule)
		throw plugin_exception(get_alias_or_name(), "Critical error (fLoadModule)");
	loading_ = true;
	if (fLoadModule(get_id(), get_alias().c_str(), mode)) {
		loaded_ = true;
		loading_ = false;
		return true;
	}
	return false;
}

void nsclient::core::dll_plugin::setBroken(bool broken) {
	broken_ = broken;
}
bool nsclient::core::dll_plugin::isBroken() {
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
bool nsclient::core::dll_plugin::getVersion(int *major, int *minor, int *revision) {
	if (!isLoaded())
		throw plugin_exception(get_alias_or_name(), "Library is not loaded");
	if (!fGetVersion)
		throw plugin_exception(get_alias_or_name(), "Critical error (fGetVersion)");
	try {
		return fGetVersion(major, minor, revision) ? true : false;
	} catch (...) {
		throw plugin_exception(get_alias_or_name(), "Unhandled exception in getVersion.");
	}
}
/**
 * Returns true if the plug in has a command handler.
 * @return true if the plug in has a command handler.
 * @throws NSPluginException if the module is not loaded.
 */
bool nsclient::core::dll_plugin::hasCommandHandler() {
	if (!isLoaded())
		throw plugin_exception(get_alias_or_name(), "Module not loaded");
	try {
		if (fHasCommandHandler(get_id()))
			return true;
		return false;
	} catch (...) {
		throw plugin_exception(get_alias_or_name(), "Unhandled exception in hasCommandHandler.");
	}
}
/**
* Returns true if the plug in has a message (log) handler.
* @return true if the plug in has a message (log) handler.
* @throws NSPluginException if the module is not loaded.
*/
bool nsclient::core::dll_plugin::hasMessageHandler() {
	if (!isLoaded())
		throw plugin_exception(get_alias_or_name(), "Module not loaded");
	try {
		if (fHasMessageHandler(get_id())) {
			return true;
		}
		return false;
	} catch (...) {
		throw plugin_exception(get_alias_or_name(), "Unhandled exception in hasMessageHandler.");
	}
}
bool nsclient::core::dll_plugin::hasNotificationHandler() {
	if (!isLoaded())
		throw plugin_exception(get_alias_or_name(), "Module not loaded");
	if (!fHasNotificationHandler)
		return false;
	try {
		if (fHasNotificationHandler(get_id())) {
			return true;
		}
		return false;
	} catch (...) {
		throw plugin_exception(get_alias_or_name(), "Unhandled exception in hasMessageHandler.");
	}
}
bool nsclient::core::dll_plugin::has_routing_handler() {
	if (!isLoaded())
		throw plugin_exception(get_alias_or_name(), "Module not loaded");
	if (!fHasRoutingHandler)
		return false;
	try {
		if (fHasRoutingHandler(get_id())) {
			return true;
		}
		return false;
	} catch (...) {
		throw plugin_exception(get_alias_or_name(), "Unhandled exception in hasMessageHandler.");
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
NSCAPI::nagiosReturn nsclient::core::dll_plugin::handleCommand(const char* dataBuffer, unsigned int dataBuffer_len, char** returnBuffer, unsigned int *returnBuffer_len) {
	if (!isLoaded() || !loaded_ || fHandleCommand == NULL)
		throw plugin_exception(get_alias_or_name(), "Library is not loaded");
	try {
		return fHandleCommand(get_id(), dataBuffer, dataBuffer_len, returnBuffer, returnBuffer_len);
	} catch (...) {
		throw plugin_exception(get_alias_or_name(), "Unhandled exception in handleCommand.");
	}
}
NSCAPI::nagiosReturn nsclient::core::dll_plugin::handleCommand(const std::string request, std::string &reply) {
	char *buffer = NULL;
	unsigned int len = 0;
	NSCAPI::nagiosReturn ret = handleCommand(request.c_str(), request.size(), &buffer, &len);
	if (buffer != NULL) {
		reply = std::string(buffer, len);
		deleteBuffer(&buffer);
	}
	return ret;
}

NSCAPI::nagiosReturn nsclient::core::dll_plugin::handle_schedule(const char* dataBuffer, const unsigned int dataBuffer_len) {
	if (!isLoaded() || fHandleSchedule == NULL)
		throw plugin_exception(get_alias_or_name(), "Library is not loaded");
	try {
		return fHandleSchedule(get_id(), dataBuffer, dataBuffer_len);
	} catch (...) {
		throw plugin_exception(get_alias_or_name(), "Unhandled exception in handle_schedule.");
	}
}

NSCAPI::nagiosReturn nsclient::core::dll_plugin::handle_schedule(const std::string &request) {
	return handle_schedule(request.c_str(), request.size());
}

NSCAPI::nagiosReturn nsclient::core::dll_plugin::handleNotification(const char *channel, std::string &request, std::string &reply) {
	char *buffer = NULL;
	unsigned int len = 0;
	NSCAPI::nagiosReturn ret = handleNotification(channel, request.c_str(), request.size(), &buffer, &len);
	if (buffer != NULL) {
		reply = std::string(buffer, len);
		deleteBuffer(&buffer);
	}
	return ret;
}

NSCAPI::nagiosReturn nsclient::core::dll_plugin::handleNotification(const char *channel, const char* dataBuffer, const unsigned int dataBuffer_len, char** returnBuffer, unsigned int *returnBuffer_len) {
	if (!isLoaded() || !loaded_ || fHandleNotification == NULL)
		throw plugin_exception(get_alias_or_name(), "Library is not loaded");
	try {
		return fHandleNotification(get_id(), channel, dataBuffer, dataBuffer_len, returnBuffer, returnBuffer_len);
	} catch (...) {
		throw plugin_exception(get_alias_or_name(), "Unhandled exception in handleNotification.");
	}
}

NSCAPI::nagiosReturn nsclient::core::dll_plugin::on_event(const std::string &request) {
	return on_event(request.c_str(), request.size());
}
NSCAPI::nagiosReturn nsclient::core::dll_plugin::on_event(const char* request_buffer, const unsigned int request_buffer_len) {
	if (!isLoaded() || !loaded_ || fOnEvent == NULL)
		throw plugin_exception(get_alias_or_name(), "Library is not loaded");
	try {
		return fOnEvent(get_id(), request_buffer, request_buffer_len);
	} catch (...) {
		throw plugin_exception(get_alias_or_name(), "Unhandled exception in OnEvent.");
	}
}
bool nsclient::core::dll_plugin::has_on_event() {
	if (!isLoaded())
		throw plugin_exception(get_alias_or_name(), "Module not loaded");
	return fOnEvent != NULL;
}


NSCAPI::nagiosReturn nsclient::core::dll_plugin::fetchMetrics(std::string &request) {
	char *buffer = NULL;
	unsigned int len = 0;
	NSCAPI::nagiosReturn ret = fetchMetrics(&buffer, &len);
	if (buffer != NULL) {
		request = std::string(buffer, len);
		deleteBuffer(&buffer);
	}
	return ret;
}

NSCAPI::nagiosReturn nsclient::core::dll_plugin::fetchMetrics(char** returnBuffer, unsigned int *returnBuffer_len) {
	if (!isLoaded() || !loaded_ || fFetchMetrics == NULL)
		throw plugin_exception(get_alias_or_name(), "Library is not loaded");
	try {
		return fFetchMetrics(get_id(), returnBuffer, returnBuffer_len);
	} catch (...) {
		throw plugin_exception(get_alias_or_name(), "Unhanded exception in fFetchMetrics.");
	}
}

NSCAPI::nagiosReturn nsclient::core::dll_plugin::submitMetrics(const std::string &request) {
	return submitMetrics(request.c_str(), request.size());
}

NSCAPI::nagiosReturn nsclient::core::dll_plugin::submitMetrics(const char* buffer, const unsigned int buffer_len) {
	if (!isLoaded() || !loaded_ || fSubmitMetrics == NULL)
		throw plugin_exception(get_alias_or_name(), "Library is not loaded");
	try {
		return fSubmitMetrics(get_id(), buffer, buffer_len);
	} catch (...) {
		throw plugin_exception(get_alias_or_name(), "Unhanded exception in SubmitMetrics.");
	}
}

bool nsclient::core::dll_plugin::route_message(const char *channel, const char* buffer, unsigned int buffer_len, char **new_channel_buffer, char **new_buffer, unsigned int *new_buffer_len) {
	if (!isLoaded() || !loaded_ || fRouteMessage == NULL)
		throw plugin_exception(get_alias_or_name(), "Library is not loaded");
	try {
		return fRouteMessage(get_id(), channel, buffer, buffer_len, new_channel_buffer, new_buffer, new_buffer_len);
	} catch (...) {
		throw plugin_exception(get_alias_or_name(), "Unhandled exception in route_message.");
	}
}

void nsclient::core::dll_plugin::deleteBuffer(char** buffer) {
	if (!isLoaded())
		throw plugin_exception(get_alias_or_name(), "Library is not loaded");
	try {
		fDeleteBuffer(buffer);
	} catch (...) {
		throw plugin_exception(get_alias_or_name(), "Unhandled exception in deleteBuffer.");
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
void nsclient::core::dll_plugin::handleMessage(const char* data, unsigned int len) {
	if (!fHandleMessage)
		throw plugin_exception(get_alias_or_name(), "Library is not loaded");
	try {
		fHandleMessage(get_id(), data, len);
	} catch (...) {
		throw plugin_exception(get_alias_or_name(), "Unhandled exception in handleMessage.");
	}
}
/**
 * Unload the plug in
 * @throws NSPluginException if the module is not loaded and/or cannot be unloaded (plug in remains loaded if so).
 */
void nsclient::core::dll_plugin::unload_plugin() {
	if (!isLoaded())
		return;
	loaded_ = false;
	if (!fUnLoadModule)
		throw plugin_exception(get_alias_or_name(), "Critical error (fUnLoadModule)");
	try {
		fUnLoadModule(get_id());
	} catch (...) {
		throw plugin_exception(get_alias_or_name(), "Unhandled exception in fUnLoadModule.");
	}
}
void nsclient::core::dll_plugin::unload_dll() {
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
bool nsclient::core::dll_plugin::getName_(char* buf, unsigned int buflen) {
	if (fGetName == NULL)
		return false;
	try {
		return fGetName(buf, buflen) ? true : false;
	} catch (...) {
		return false;
	}
}
bool nsclient::core::dll_plugin::getDescription_(char* buf, unsigned int buflen) {
	if (fGetDescription == NULL)
		throw plugin_exception(get_alias_or_name(), "Critical error (fGetDescription)");
	try {
		return fGetDescription(buf, buflen) ? true : false;
	} catch (...) {
		throw plugin_exception(get_alias_or_name(), "Unhandled exception in getDescription.");
	}
}

/**
 * Load all remote function pointers from the loaded module.
 * These pointers are cached for "speed" which might (?) be dangerous if something changes.
 * @throws NSPluginException if any of the function pointers fail to load.
 * If NSPluginException  is thrown the loaded might remain partially loaded and crashes might occur if plug in is used in this state.
 */
void nsclient::core::dll_plugin::loadRemoteProcs_(void) {
	try {
		fLoadModule = (nscapi::plugin_api::lpLoadModule)module_.load_proc("NSLoadModuleEx");
		if (!fLoadModule)
			throw plugin_exception(get_alias_or_name(), "Could not load NSLoadModuleEx");

		fModuleHelperInit = (nscapi::plugin_api::lpModuleHelperInit)module_.load_proc("NSModuleHelperInit");
		if (!fModuleHelperInit)
			throw plugin_exception(get_alias_or_name(), "Could not load NSModuleHelperInit");

		try {
			fModuleHelperInit(get_id(), &NSAPILoader);
		} catch (...) {
			throw plugin_exception(get_alias_or_name(), "Unhandled exception in getDescription.");
		}

		fGetName = (nscapi::plugin_api::lpGetName)module_.load_proc("NSGetModuleName");
		if (!fGetName)
			throw plugin_exception(get_alias_or_name(), "Could not load NSGetModuleName");

		fGetVersion = (nscapi::plugin_api::lpGetVersion)module_.load_proc("NSGetModuleVersion");
		if (!fGetVersion)
			throw plugin_exception(get_alias_or_name(), "Could not load NSGetModuleVersion");

		fGetDescription = (nscapi::plugin_api::lpGetDescription)module_.load_proc("NSGetModuleDescription");
		if (!fGetDescription)
			throw plugin_exception(get_alias_or_name(), "Could not load NSGetModuleDescription");

		fHasCommandHandler = (nscapi::plugin_api::lpHasCommandHandler)module_.load_proc("NSHasCommandHandler");
		if (!fHasCommandHandler)
			throw plugin_exception(get_alias_or_name(), "Could not load NSHasCommandHandler");

		fHasMessageHandler = (nscapi::plugin_api::lpHasMessageHandler)module_.load_proc("NSHasMessageHandler");
		if (!fHasMessageHandler)
			throw plugin_exception(get_alias_or_name(), "Could not load NSHasMessageHandler");

		fHandleCommand = (nscapi::plugin_api::lpHandleCommand)module_.load_proc("NSHandleCommand");

		fDeleteBuffer = (nscapi::plugin_api::lpDeleteBuffer)module_.load_proc("NSDeleteBuffer");
		if (!fDeleteBuffer)
			throw plugin_exception(get_alias_or_name(), "Could not load NSDeleteBuffer");

		fHandleMessage = (nscapi::plugin_api::lpHandleMessage)module_.load_proc("NSHandleMessage");
		if (!fHandleMessage)
			throw plugin_exception(get_alias_or_name(), "Could not load NSHandleMessage");

		fUnLoadModule = (nscapi::plugin_api::lpUnLoadModule)module_.load_proc("NSUnloadModule");
		if (!fUnLoadModule)
			throw plugin_exception(get_alias_or_name(), "Could not load NSUnloadModule");

		fCommandLineExec = (nscapi::plugin_api::lpCommandLineExec)module_.load_proc("NSCommandLineExec");
		fHandleNotification = (nscapi::plugin_api::lpHandleNotification)module_.load_proc("NSHandleNotification");
		fHasNotificationHandler = (nscapi::plugin_api::lpHasNotificationHandler)module_.load_proc("NSHasNotificationHandler");

		fHasRoutingHandler = (nscapi::plugin_api::lpHasRoutingHandler)module_.load_proc("NSHasRoutingHandler");
		fRouteMessage = (nscapi::plugin_api::lpRouteMessage)module_.load_proc("NSRouteMessage");

		fHandleSchedule = (nscapi::plugin_api::lpHandleSchedule)module_.load_proc("NSHandleSchedule");
		fFetchMetrics = (nscapi::plugin_api::lpFetchMetrics)module_.load_proc("NSFetchMetrics");
		fSubmitMetrics = (nscapi::plugin_api::lpSubmitMetrics)module_.load_proc("NSSubmitMetrics");
		fOnEvent = (nscapi::plugin_api::lpOnEvent)module_.load_proc("NSOnEvent");
	} catch (plugin_exception &e) {
		throw e;
	} catch (dll::dll_exception &e) {
		throw plugin_exception(get_alias_or_name(), std::string("Unhanded exception when loading process: ") + e.what());
	} catch (...) {
		throw plugin_exception(get_alias_or_name(), "Unhandled exception when loading process: <UNKNOWN>");
	}
}

int nsclient::core::dll_plugin::commandLineExec(bool targeted, std::string &request, std::string &reply) {
	char *buffer = NULL;
	unsigned int len = 0;
	NSCAPI::nagiosReturn ret = commandLineExec(targeted, request.c_str(), request.size(), &buffer, &len);
	if (buffer != NULL) {
		reply = std::string(buffer, len);
		deleteBuffer(&buffer);
	}
	return ret;
}

bool nsclient::core::dll_plugin::has_command_line_exec() {
	return (isLoaded() && !loaded_) || (fCommandLineExec != NULL);
}

int nsclient::core::dll_plugin::commandLineExec(bool targeted, const char* request, const unsigned int request_len, char** reply, unsigned int *reply_len) {
	if (!has_command_line_exec())
		throw plugin_exception(get_alias_or_name(), "Library is not loaded or modules does not support command line");
	try {
		return fCommandLineExec(get_id(), targeted ? NSCAPI::target_module : NSCAPI::target_any, request, request_len, reply, reply_len);
	} catch (...) {
		throw plugin_exception(get_alias_or_name(), "Unhanded exception in handleCommand.");
	}
}
bool nsclient::core::dll_plugin::is_duplicate(boost::filesystem::path file, std::string alias) {
	if (alias.empty() && get_alias().empty())
		return module_.get_file() == dll::dll_impl::fix_module_name(file);
	if (alias.empty() || get_alias().empty())
		return false;
	return module_.get_file() == dll::dll_impl::fix_module_name(file) && alias == get_alias();
}

std::string nsclient::core::dll_plugin::get_version() {
	int major, minor, revision;
	getVersion(&major, &minor, &revision);
	return str::xtos(major) + "." + str::xtos(minor) + "." + str::xtos(revision);
}
