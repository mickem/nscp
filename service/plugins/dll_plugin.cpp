// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "dll_plugin.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <str/xtos.hpp>

#include "../core_api.h"
#include "NSCAPI.h"

/**
 * Default c-tor
 * Initializes the plug in name but does not load the actual plug in.<br>
 * To load the plug in use function load() that loads an initializes the plug in.
 *
 * @param file The file (DLL) to load as a NSC plug in.
 */
nsclient::core::dll_plugin::dll_plugin(const unsigned int id, const boost::filesystem::path file, std::string alias)
    : plugin_interface(id, alias),
      module_(file),
      loaded_(false),
      loading_(false),
      broken_(false),
      started_(false),
      fModuleHelperInit(nullptr),
      fLoadModule(nullptr),
      fStartModule(nullptr),
      fPrepareShutdown(nullptr),
      fGetName(nullptr),
      fGetVersion(nullptr),
      fGetDescription(nullptr),
      fHasCommandHandler(nullptr),
      fHasMessageHandler(nullptr),
      fHandleCommand(nullptr),
      fHandleMessage(nullptr),
      fDeleteBuffer(nullptr),
      fUnLoadModule(nullptr),
      fCommandLineExec(nullptr),
      fHasNotificationHandler(nullptr),
      fHandleNotification(nullptr),
      fHasRoutingHandler(nullptr),
      fRouteMessage(nullptr),
      fFetchMetrics(nullptr),
      fSubmitMetrics(nullptr),
      fOnEvent(nullptr) {
  load_dll();
}
nsclient::core::dll_plugin::dispatch_lock::dispatch_lock(dll_plugin &owner) : owner_(owner), entered_(false) {
  boost::lock_guard<boost::mutex> guard(owner_.dispatch_mutex_);
  // Only the bookkeeping is serialised, never the dispatch itself: two callers
  // arriving at the same module from different transports both go straight in.
  const boost::thread::id self = boost::this_thread::get_id();
  // A thread already inside may re-enter even once an unload has started. It
  // is one of the calls that unload is waiting for, so the module cannot go
  // away underneath it - and refusing would fail the outer call (a check_multi
  // running its sub-checks) for no gain.
  if (owner_.unloading_ && owner_.dispatchers_.find(self) == owner_.dispatchers_.end()) return;
  owner_.dispatchers_.insert(self);
  entered_ = true;
}
nsclient::core::dll_plugin::dispatch_lock::~dispatch_lock() {
  if (!entered_) return;
  boost::lock_guard<boost::mutex> guard(owner_.dispatch_mutex_);
  // Erase one entry, not every entry for this thread: a module that dispatches
  // into itself nests, and the outer call is still running.
  const std::multiset<boost::thread::id>::iterator it = owner_.dispatchers_.find(boost::this_thread::get_id());
  if (it != owner_.dispatchers_.end()) owner_.dispatchers_.erase(it);
  if (owner_.dispatchers_.empty()) owner_.dispatch_idle_.notify_all();
}

/**
 * Default d-tor
 */
nsclient::core::dll_plugin::~dll_plugin() {
  if (isLoaded()) {
    try {
      dll_plugin::unload_plugin();
    } catch (const plugin_exception &) {
      // ...
    }
  }
  try {
    unload_dll();
  } catch (const plugin_exception &) {
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
  if (module_.is_loaded()) throw plugin_exception(get_alias_or_name(), "Module already loaded");
  try {
    module_.load_library();
  } catch (dll::dll_exception &e) {
    throw plugin_exception(get_alias_or_name() + " (" + module_.get_file().string(), e.what());
  }
  loadRemoteProcs_();
}

bool nsclient::core::dll_plugin::load_plugin(NSCAPI::moduleLoadMode mode) {
  if ((loaded_ || loading_) && mode != NSCAPI::reloadStart) return true;
  if (!fLoadModule) throw plugin_exception(get_alias_or_name(), "Critical error (fLoadModule)");
  loading_ = true;
  if (fLoadModule(get_id(), get_alias().c_str(), mode)) {
    loaded_ = true;
    loading_ = false;
    return true;
  }
  return false;
}

bool nsclient::core::dll_plugin::has_start() { return fStartModule != nullptr; }

bool nsclient::core::dll_plugin::start_plugin() {
  if (started_) {
    return true;
  }
  if (!fStartModule) return true;
  if (fStartModule(get_id())) {
    started_ = true;
    return true;
  }
  return false;
}

bool nsclient::core::dll_plugin::has_prepare_shutdown() { return fPrepareShutdown != nullptr; }

void nsclient::core::dll_plugin::prepare_shutdown_plugin() {
  if (!isLoaded()) return;
  if (!fPrepareShutdown) return;
  try {
    fPrepareShutdown(get_id());
  } catch (...) {
    throw plugin_exception(get_alias_or_name(), "Unhandled exception in fPrepareShutdown.");
  }
}

void nsclient::core::dll_plugin::setBroken(bool broken) { broken_ = broken; }
bool nsclient::core::dll_plugin::isBroken() const { return broken_; }

/**
 * Get the plug in version.
 *
 * @bug Not implemented as of yet
 *
 * @param major Major version
 * @param minor Minor version
 * @param revision Revision
 * @return False
 */
bool nsclient::core::dll_plugin::getVersion(int *major, int *minor, int *revision) {
  if (!isLoaded()) throw plugin_exception(get_alias_or_name(), "Library is not loaded");
  if (!fGetVersion) throw plugin_exception(get_alias_or_name(), "Critical error (fGetVersion)");
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
  if (!isLoaded()) throw plugin_exception(get_alias_or_name(), "Module not loaded");
  try {
    if (fHasCommandHandler(get_id())) return true;
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
  if (!isLoaded()) throw plugin_exception(get_alias_or_name(), "Module not loaded");
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
  if (!isLoaded()) throw plugin_exception(get_alias_or_name(), "Module not loaded");
  if (!fHasNotificationHandler) return false;
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
  if (!isLoaded()) throw plugin_exception(get_alias_or_name(), "Module not loaded");
  if (!fHasRoutingHandler) return false;
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
 * @param request The request buffer
 * @param request_length Request buffer length
 * @param response The response buffer
 * @param response_length Response buffer length
 * @return Status of execution. Could be error codes, buffer length messages etc.
 * @throws NSPluginException if the module is not loaded.
 */
NSCAPI::nagiosReturn nsclient::core::dll_plugin::handleCommand(const char *request, unsigned int request_length, char **response,
                                                               unsigned int *response_length) {
  if (!isLoaded() || !loaded_ || fHandleCommand == nullptr) throw plugin_exception(get_alias_or_name(), "Library is not loaded");
  dispatch_lock dispatch(*this);
  if (!dispatch.entered()) throw plugin_exception(get_alias_or_name(), "Library is unloading");
  try {
    return fHandleCommand(get_id(), request, request_length, response, response_length);
  } catch (...) {
    throw plugin_exception(get_alias_or_name(), "Unhandled exception in handleCommand.");
  }
}
NSCAPI::nagiosReturn nsclient::core::dll_plugin::handleCommand(const std::string request, std::string &reply) {
  char *buffer = nullptr;
  unsigned int len = 0;
  NSCAPI::nagiosReturn ret = handleCommand(request.c_str(), static_cast<unsigned int>(request.size()), &buffer, &len);
  if (buffer != nullptr) {
    reply = std::string(buffer, len);
    deleteBuffer(&buffer);
  }
  return ret;
}

NSCAPI::nagiosReturn nsclient::core::dll_plugin::handle_schedule(const char *dataBuffer, const unsigned int dataBuffer_len) {
  if (!isLoaded() || fHandleSchedule == nullptr) throw plugin_exception(get_alias_or_name(), "Library is not loaded");
  dispatch_lock dispatch(*this);
  if (!dispatch.entered()) throw plugin_exception(get_alias_or_name(), "Library is unloading");
  try {
    return fHandleSchedule(get_id(), dataBuffer, dataBuffer_len);
  } catch (...) {
    throw plugin_exception(get_alias_or_name(), "Unhandled exception in handle_schedule.");
  }
}

NSCAPI::nagiosReturn nsclient::core::dll_plugin::handle_schedule(const std::string &request) {
  return handle_schedule(request.c_str(), static_cast<unsigned int>(request.size()));
}

NSCAPI::nagiosReturn nsclient::core::dll_plugin::handleNotification(const char *channel, std::string &request, std::string &reply) {
  char *buffer = nullptr;
  unsigned int len = 0;
  const NSCAPI::nagiosReturn ret = handleNotification(channel, request.c_str(), static_cast<unsigned int>(request.size()), &buffer, &len);
  if (buffer != nullptr) {
    reply = std::string(buffer, len);
    deleteBuffer(&buffer);
  }
  return ret;
}

NSCAPI::nagiosReturn nsclient::core::dll_plugin::handleNotification(const char *channel, const char *dataBuffer, const unsigned int dataBuffer_len,
                                                                    char **returnBuffer, unsigned int *returnBuffer_len) {
  if (!isLoaded() || !loaded_ || fHandleNotification == nullptr) throw plugin_exception(get_alias_or_name(), "Library is not loaded");
  dispatch_lock dispatch(*this);
  if (!dispatch.entered()) throw plugin_exception(get_alias_or_name(), "Library is unloading");
  try {
    return fHandleNotification(get_id(), channel, dataBuffer, dataBuffer_len, returnBuffer, returnBuffer_len);
  } catch (...) {
    throw plugin_exception(get_alias_or_name(), "Unhandled exception in handleNotification.");
  }
}

NSCAPI::nagiosReturn nsclient::core::dll_plugin::on_event(const std::string &request) {
  return on_event(request.c_str(), static_cast<unsigned int>(request.size()));
}
NSCAPI::nagiosReturn nsclient::core::dll_plugin::on_event(const char *request_buffer, const unsigned int request_buffer_len) {
  if (!isLoaded() || !loaded_ || fOnEvent == nullptr) throw plugin_exception(get_alias_or_name(), "Library is not loaded");
  dispatch_lock dispatch(*this);
  if (!dispatch.entered()) throw plugin_exception(get_alias_or_name(), "Library is unloading");
  try {
    return fOnEvent(get_id(), request_buffer, request_buffer_len);
  } catch (...) {
    throw plugin_exception(get_alias_or_name(), "Unhandled exception in OnEvent.");
  }
}
bool nsclient::core::dll_plugin::has_on_event() {
  if (!isLoaded()) throw plugin_exception(get_alias_or_name(), "Module not loaded");
  return fOnEvent != nullptr;
}

NSCAPI::nagiosReturn nsclient::core::dll_plugin::fetchMetrics(std::string &request) {
  char *buffer = nullptr;
  unsigned int len = 0;
  NSCAPI::nagiosReturn ret = fetchMetrics(&buffer, &len);
  if (buffer != nullptr) {
    request = std::string(buffer, len);
    deleteBuffer(&buffer);
  }
  return ret;
}

NSCAPI::nagiosReturn nsclient::core::dll_plugin::fetchMetrics(char **returnBuffer, unsigned int *returnBuffer_len) {
  if (!isLoaded() || !loaded_ || fFetchMetrics == nullptr) throw plugin_exception(get_alias_or_name(), "Library is not loaded");
  dispatch_lock dispatch(*this);
  if (!dispatch.entered()) throw plugin_exception(get_alias_or_name(), "Library is unloading");
  try {
    return fFetchMetrics(get_id(), returnBuffer, returnBuffer_len);
  } catch (...) {
    throw plugin_exception(get_alias_or_name(), "Unhanded exception in fFetchMetrics.");
  }
}

NSCAPI::nagiosReturn nsclient::core::dll_plugin::submitMetrics(const std::string &request) {
  return submitMetrics(request.c_str(), static_cast<unsigned int>(request.size()));
}

NSCAPI::nagiosReturn nsclient::core::dll_plugin::submitMetrics(const char *buffer, const unsigned int buffer_len) {
  if (!isLoaded() || !loaded_ || fSubmitMetrics == nullptr) throw plugin_exception(get_alias_or_name(), "Library is not loaded");
  dispatch_lock dispatch(*this);
  if (!dispatch.entered()) throw plugin_exception(get_alias_or_name(), "Library is unloading");
  try {
    return fSubmitMetrics(get_id(), buffer, buffer_len);
  } catch (...) {
    throw plugin_exception(get_alias_or_name(), "Unhanded exception in SubmitMetrics.");
  }
}

bool nsclient::core::dll_plugin::route_message(const char *channel, const char *buffer, unsigned int buffer_len, char **new_channel_buffer, char **new_buffer,
                                               unsigned int *new_buffer_len) {
  if (!isLoaded() || !loaded_ || fRouteMessage == nullptr) throw plugin_exception(get_alias_or_name(), "Library is not loaded");
  dispatch_lock dispatch(*this);
  if (!dispatch.entered()) throw plugin_exception(get_alias_or_name(), "Library is unloading");
  try {
    return fRouteMessage(get_id(), channel, buffer, buffer_len, new_channel_buffer, new_buffer, new_buffer_len);
  } catch (...) {
    throw plugin_exception(get_alias_or_name(), "Unhandled exception in route_message.");
  }
}

void nsclient::core::dll_plugin::deleteBuffer(char **buffer) {
  if (!isLoaded()) throw plugin_exception(get_alias_or_name(), "Library is not loaded");
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
void nsclient::core::dll_plugin::handleMessage(const char *data, unsigned int len) {
  if (!fHandleMessage) throw plugin_exception(get_alias_or_name(), "Library is not loaded");
  // A log line racing the unload must not call into an instance that
  // unload_plugin has torn down. Unlike every other entry point this one does
  // NOT take the shared dispatch lock: simple_console_logger dispatches
  // subscribers synchronously on the caller's thread, so a module that logs
  // from inside its own fUnLoadModule would arrive here on the thread already
  // holding dispatch_mutex_ exclusively and deadlock. The flag is atomic
  // instead, which orders this read against unload_plugin's write without
  // making the unload wait on a log callback. That leaves a narrow window --
  // an unload starting between this check and the call below still tears the
  // instance down under it -- which closing properly needs a lock this path
  // can reach without deadlocking.
  if (unloaded_) return;
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
  if (!isLoaded()) return;
  {
    boost::unique_lock<boost::mutex> lock(dispatch_mutex_);
    // Close the door first, then wait for whoever is already inside.
    unloading_ = true;
    const boost::thread::id self = boost::this_thread::get_id();
    const boost::system_time deadline = boost::get_system_time() + boost::posix_time::seconds(5);
    // Wait until the only calls left inside are this thread's own. A handler
    // unloading the module it is running in is itself one of them, further up
    // this stack, and can never leave before we return - so waiting for it
    // would be waiting for ourselves.
    while (dispatchers_.size() != dispatchers_.count(self)) {
      if (!dispatch_idle_.timed_wait(lock, deadline)) {
        // Other threads are still executing inside the module and did not come
        // back within the wait. Tearing it down now is precisely the race this
        // count exists to prevent, so leave it loaded and serving: leaking a
        // module is much cheaper than calling into one whose instance has been
        // destroyed.
        unloading_ = false;
        throw plugin_exception(get_alias_or_name(), "Refused to unload: calls into the module were still in flight after 5s");
      }
    }
  }
  // Only call into the DSO while a module instance can exist there (fLoadModule
  // was invoked — even unsuccessfully — and unload has not run yet). A second
  // call, e.g. from the destructor of a shared_ptr copy that outlives main(),
  // would re-enter the module after its static state is destructed.
  if (!loaded_ && !loading_) return;
  loaded_ = false;
  loading_ = false;
  unloaded_ = true;
  if (!fUnLoadModule) throw plugin_exception(get_alias_or_name(), "Critical error (fUnLoadModule)");
  try {
    fUnLoadModule(get_id());
  } catch (...) {
    throw plugin_exception(get_alias_or_name(), "Unhandled exception in fUnLoadModule.");
  }
}
void nsclient::core::dll_plugin::unload_dll() {
  fModuleHelperInit = nullptr;
  fLoadModule = nullptr;
  fPrepareShutdown = nullptr;
  fGetName = nullptr;
  fGetVersion = nullptr;
  fGetDescription = nullptr;
  fHasCommandHandler = nullptr;
  fHasMessageHandler = nullptr;
  fHandleCommand = nullptr;
  fDeleteBuffer = nullptr;
  fHandleMessage = nullptr;
  fUnLoadModule = nullptr;
  fCommandLineExec = nullptr;
  fHasNotificationHandler = nullptr;
  fHandleNotification = nullptr;
  fHasRoutingHandler = nullptr;
  fRouteMessage = nullptr;
  fHandleSchedule = nullptr;
  fFetchMetrics = nullptr;
  fSubmitMetrics = nullptr;
  fOnEvent = nullptr;
  module_.unload_library();
}
bool nsclient::core::dll_plugin::getName_(char *buf, unsigned int buflen) {
  if (fGetName == nullptr) return false;
  try {
    return fGetName(buf, buflen) ? true : false;
  } catch (...) {
    return false;
  }
}
bool nsclient::core::dll_plugin::getDescription_(char *buf, unsigned int buflen) {
  if (fGetDescription == nullptr) throw plugin_exception(get_alias_or_name(), "Critical error (fGetDescription)");
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
    if (!fLoadModule) throw plugin_exception(get_alias_or_name(), "Could not load NSLoadModuleEx");

    fStartModule = (nscapi::plugin_api::lpStartModule)module_.load_proc("NSStartModule");
    fPrepareShutdown = (nscapi::plugin_api::lpPrepareShutdown)module_.load_proc("NSPrepareShutdown");

    fModuleHelperInit = (nscapi::plugin_api::lpModuleHelperInit)module_.load_proc("NSModuleHelperInit");
    if (!fModuleHelperInit) throw plugin_exception(get_alias_or_name(), "Could not load NSModuleHelperInit");

    try {
      fModuleHelperInit(get_id(), &NSAPILoader);
    } catch (...) {
      throw plugin_exception(get_alias_or_name(), "Unhandled exception in getDescription.");
    }

    fGetName = (nscapi::plugin_api::lpGetName)module_.load_proc("NSGetModuleName");
    if (!fGetName) throw plugin_exception(get_alias_or_name(), "Could not load NSGetModuleName");

    fGetVersion = (nscapi::plugin_api::lpGetVersion)module_.load_proc("NSGetModuleVersion");
    if (!fGetVersion) throw plugin_exception(get_alias_or_name(), "Could not load NSGetModuleVersion");

    fGetDescription = (nscapi::plugin_api::lpGetDescription)module_.load_proc("NSGetModuleDescription");
    if (!fGetDescription) throw plugin_exception(get_alias_or_name(), "Could not load NSGetModuleDescription");

    fHasCommandHandler = (nscapi::plugin_api::lpHasCommandHandler)module_.load_proc("NSHasCommandHandler");
    if (!fHasCommandHandler) throw plugin_exception(get_alias_or_name(), "Could not load NSHasCommandHandler");

    fHasMessageHandler = (nscapi::plugin_api::lpHasMessageHandler)module_.load_proc("NSHasMessageHandler");
    if (!fHasMessageHandler) throw plugin_exception(get_alias_or_name(), "Could not load NSHasMessageHandler");

    fHandleCommand = (nscapi::plugin_api::lpHandleCommand)module_.load_proc("NSHandleCommand");

    fDeleteBuffer = (nscapi::plugin_api::lpDeleteBuffer)module_.load_proc("NSDeleteBuffer");
    if (!fDeleteBuffer) throw plugin_exception(get_alias_or_name(), "Could not load NSDeleteBuffer");

    fHandleMessage = (nscapi::plugin_api::lpHandleMessage)module_.load_proc("NSHandleMessage");
    if (!fHandleMessage) throw plugin_exception(get_alias_or_name(), "Could not load NSHandleMessage");

    fUnLoadModule = (nscapi::plugin_api::lpUnLoadModule)module_.load_proc("NSUnloadModule");
    if (!fUnLoadModule) throw plugin_exception(get_alias_or_name(), "Could not load NSUnloadModule");

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
  char *buffer = nullptr;
  unsigned int len = 0;
  const NSCAPI::nagiosReturn ret = commandLineExec(targeted, request.c_str(), static_cast<unsigned int>(request.size()), &buffer, &len);
  if (buffer != nullptr) {
    reply = std::string(buffer, len);
    deleteBuffer(&buffer);
  }
  return ret;
}

// A mapped-but-not-started module (the client path) may still be exec'd, but
// only when it actually exports NSCommandLineExec: the old first disjunct made
// every module whose load had failed claim the export and then call nullptr.
bool nsclient::core::dll_plugin::has_command_line_exec() { return isLoaded() && fCommandLineExec != nullptr; }

int nsclient::core::dll_plugin::commandLineExec(bool targeted, const char *request, const unsigned int request_len, char **reply, unsigned int *reply_len) {
  if (!has_command_line_exec()) throw plugin_exception(get_alias_or_name(), "Library is not loaded or modules does not support command line");
  dispatch_lock dispatch(*this);
  if (!dispatch.entered()) throw plugin_exception(get_alias_or_name(), "Library is unloading");
  try {
    return fCommandLineExec(get_id(), targeted ? NSCAPI::target_module : NSCAPI::target_any, request, request_len, reply, reply_len);
  } catch (...) {
    throw plugin_exception(get_alias_or_name(), "Unhanded exception in handleCommand.");
  }
}
bool nsclient::core::dll_plugin::is_duplicate(boost::filesystem::path file, std::string alias) {
  if (alias.empty() && get_alias().empty()) return module_.get_file() == dll::dll_impl::fix_module_name(file);
  if (alias.empty() || get_alias().empty()) return false;
  return module_.get_file() == dll::dll_impl::fix_module_name(file) && alias == get_alias();
}

std::string nsclient::core::dll_plugin::get_version() {
  int major, minor, revision;
  getVersion(&major, &minor, &revision);
  return str::xtos(major) + "." + str::xtos(minor) + "." + str::xtos(revision);
}
