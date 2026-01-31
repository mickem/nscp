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

#pragma once

#include <NSCAPI.h>

#include <boost/algorithm/string.hpp>
#include <dll/dll.hpp>

#include "plugin_interface.hpp"

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
namespace nsclient {
namespace core {
class dll_plugin : public boost::noncopyable, public plugin_interface {
  ::dll::dll_impl module_;
  bool loaded_;
  bool loading_;
  bool broken_;
  bool started_;

  nscapi::plugin_api::lpModuleHelperInit fModuleHelperInit;
  nscapi::plugin_api::lpLoadModule fLoadModule;
  nscapi::plugin_api::lpStartModule fStartModule;
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

  void on_log_message(std::string &payload) override { handleMessage(payload.c_str(), static_cast<unsigned int>(payload.size())); }
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
