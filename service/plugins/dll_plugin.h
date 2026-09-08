// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/thread.hpp>
#include <set>
#include <NSCAPI.h>

#include <boost/algorithm/string.hpp>
#include <dll/dll.hpp>

#include "plugin_interface.hpp"

/**
 * NSCPlugin is a wrapper class to wrap all DLL calls and make things simple and clean inside the actual application.<br>
 * Things tend to be one-to-one by which I mean that a call to a function here should call the corresponding function in the plug in (if loaded).
 * If things are "broken" NSPluginException is called to indicate this. Error states are returned for normal "conditions".
 *
 * @version 1.0
 * first version
 *
 * @date 02-12-2005
 *
 * @todo
 * getVersion() is not implemented as of yet.
 */
namespace nsclient {
namespace core {
class dll_plugin : public boost::noncopyable, public plugin_interface {
  ::dll::dll_impl module_;
  bool loaded_;
  bool loading_;
  // A reference count of the calls currently inside the module, not a
  // reader/writer lock. Dispatches never block each other, and never block
  // behind a pending unload: boost::shared_mutex is writer-preferring, so a
  // queued unload stalled every new dispatch - and a module re-entering itself
  // through the core (check_multi calling check_always_ok, a script querying a
  // command its own module serves) deadlocked against its own outer read lock.
  // The count is kept as the multiset of dispatching threads because
  // unload_plugin needs one thing a plain counter cannot give it: whether the
  // calls in flight are only its own, which is the case when a handler unloads
  // the module it is itself running in.
  mutable boost::mutex dispatch_mutex_;
  boost::condition_variable dispatch_idle_;
  std::multiset<boost::thread::id> dispatchers_;
  // Set by unload_plugin before it waits: no dispatch may enter after it.
  bool unloading_ = false;

  // Registers this thread as being inside the module for as long as it lives.
  // Never blocks. Entering is refused once an unload has started, which the
  // entry points check through entered().
  class dispatch_lock {
    dll_plugin &owner_;
    bool entered_;

   public:
    explicit dispatch_lock(dll_plugin &owner);
    ~dispatch_lock();
    bool entered() const { return entered_; }
    dispatch_lock(const dispatch_lock &) = delete;
    dispatch_lock &operator=(const dispatch_lock &) = delete;
  };
  // Set once unload_plugin has run; nothing is delivered to the module after.
  // Atomic because handleMessage reads it off the logger path without the
  // dispatch lock (see dll_plugin.cpp for why it cannot take one).
  std::atomic<bool> unloaded_{false};
  bool broken_;
  bool started_;

  nscapi::plugin_api::lpModuleHelperInit fModuleHelperInit;
  nscapi::plugin_api::lpLoadModule fLoadModule;
  nscapi::plugin_api::lpStartModule fStartModule;
  nscapi::plugin_api::lpPrepareShutdown fPrepareShutdown;
  nscapi::plugin_api::lpGetName fGetName;
  nscapi::plugin_api::lpGetVersion fGetVersion;
  nscapi::plugin_api::lpGetDescription fGetDescription;
  nscapi::plugin_api::lpHasCommandHandler fHasCommandHandler;
  nscapi::plugin_api::lpHasMessageHandler fHasMessageHandler;
  nscapi::plugin_api::lpHandleCommand fHandleCommand;
  nscapi::plugin_api::lpHandleSchedule fHandleSchedule;
  nscapi::plugin_api::lpHandleMessage fHandleMessage;
  nscapi::plugin_api::lpDeleteBuffer fDeleteBuffer;
  nscapi::plugin_api::lpUnLoadModule fUnLoadModule;
  nscapi::plugin_api::lpCommandLineExec fCommandLineExec;
  nscapi::plugin_api::lpHasNotificationHandler fHasNotificationHandler;
  nscapi::plugin_api::lpHandleNotification fHandleNotification;
  nscapi::plugin_api::lpHasRoutingHandler fHasRoutingHandler;
  nscapi::plugin_api::lpRouteMessage fRouteMessage;
  nscapi::plugin_api::lpFetchMetrics fFetchMetrics;
  nscapi::plugin_api::lpSubmitMetrics fSubmitMetrics;
  nscapi::plugin_api::lpOnEvent fOnEvent;

 public:
  dll_plugin(const unsigned int id, const boost::filesystem::path file, std::string alias);
  ~dll_plugin() override;

  bool load_plugin(NSCAPI::moduleLoadMode mode) override;
  bool has_start() override;
  bool start_plugin() override;
  bool has_prepare_shutdown() override;
  void prepare_shutdown_plugin() override;
  void unload_plugin() override;

  std::string getName() override;
  std::string getDescription() override;
  bool hasCommandHandler() override;
  bool hasNotificationHandler() override;
  bool hasMessageHandler() override;
  NSCAPI::nagiosReturn handleCommand(const std::string request, std::string &reply) override;
  NSCAPI::nagiosReturn handle_schedule(const std::string &request) override;
  NSCAPI::nagiosReturn handleNotification(const char *channel, std::string &request, std::string &reply) override;
  bool has_on_event() override;
  NSCAPI::nagiosReturn on_event(const std::string &request) override;
  NSCAPI::nagiosReturn fetchMetrics(std::string &request) override;
  NSCAPI::nagiosReturn submitMetrics(const std::string &request) override;
  void handleMessage(const char *data, unsigned int len) override;
  int commandLineExec(bool targeted, std::string &request, std::string &reply) override;
  bool has_command_line_exec() override;
  bool is_duplicate(boost::filesystem::path file, std::string alias) override;

  bool has_routing_handler() override;

  bool route_message(const char *channel, const char *buffer, unsigned int buffer_len, char **new_channel_buffer, char **new_buffer,
                     unsigned int *new_buffer_len) override;

  bool hasMetricsFetcher() override { return fFetchMetrics != nullptr; }
  bool hasMetricsSubmitter() override { return fSubmitMetrics != nullptr; }

  std::string getModule() override {
#ifndef WIN32
    std::string file = module_.get_module_name();
    if (file.substr(0, 3) == "lib") file = file.substr(3);
    return file;
#else
    return module_.get_module_name();
#endif
  }

  void on_log_message(const std::string &payload) override { handleMessage(payload.c_str(), static_cast<unsigned int>(payload.size())); }
  std::string get_version() override;

 private:
  void load_dll();
  void unload_dll();

  void setBroken(bool broken);
  bool isBroken() const;

  NSCAPI::nagiosReturn handleCommand(const char *request, const unsigned int request_length, char **response, unsigned int *response_length);
  NSCAPI::nagiosReturn handle_schedule(const char *dataBuffer, const unsigned int dataBuffer_len);
  NSCAPI::nagiosReturn handleNotification(const char *channel, const char *request_buffer, const unsigned int request_buffer_len, char **response_buffer,
                                          unsigned int *response_buffer_len);
  NSCAPI::nagiosReturn on_event(const char *request_buffer, const unsigned int request_buffer_len);
  NSCAPI::nagiosReturn fetchMetrics(char **response_buffer, unsigned int *response_buffer_len);
  NSCAPI::nagiosReturn submitMetrics(const char *buffer, const unsigned int buffer_len);
  int commandLineExec(bool targeted, const char *request, const unsigned int request_len, char **reply, unsigned int *reply_len);
  bool getVersion(int *major, int *minor, int *revision);

  bool isLoaded() const { return module_.is_loaded(); }

  bool getName_(char *buf, unsigned int buflen);
  bool getDescription_(char *buf, unsigned int buflen);
  void loadRemoteProcs_(void);
  void deleteBuffer(char **buffer);
};
}  // namespace core
}  // namespace nsclient
