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

#include <dll/dll.hpp>
#include <list>
#include <nsclient/logger/logger.hpp>
#include <set>
#include <string>

#include "plugin_interface.hpp"
#include "plugin_manager.hpp"

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

struct script_def {
  std::string provider;
  std::string script;
  std::string alias;
  std::string command;
};

class zip_plugin : public boost::noncopyable, public plugin_interface {
  boost::filesystem::path file_;
  path_instance paths_;
  plugin_mgr_instance plugins_;
  logging::logger_instance logger_;
  std::string name_;
  std::string description_;

  std::list<script_def> scripts_;
  std::set<std::string> modules_;
  std::list<std::string> on_start_;

 public:
  zip_plugin(unsigned int id, const boost::filesystem::path &file, const std::string &alias, const path_instance &paths, const plugin_mgr_instance &plugins,
             const logging::logger_instance &logger);
  ~zip_plugin() override = default;

  bool load_plugin(NSCAPI::moduleLoadMode mode) override;
  bool has_start() override { return false; }
  bool start_plugin() override { return true; }
  bool has_prepare_shutdown() override { return false; }
  void prepare_shutdown_plugin() override {}
  void unload_plugin() override;

  std::string getName() override;
  std::string getDescription() override;
  bool hasCommandHandler() override { return false; }
  bool hasNotificationHandler() override { return false; }
  bool hasMessageHandler() override { return false; }
  NSCAPI::nagiosReturn handleCommand(std::string request, std::string &reply) override;
  NSCAPI::nagiosReturn handle_schedule(const std::string &request) override;
  NSCAPI::nagiosReturn handleNotification(const char *channel, std::string &request, std::string &reply) override;
  bool has_on_event() override { return false; }
  NSCAPI::nagiosReturn on_event(const std::string &request) override;
  NSCAPI::nagiosReturn fetchMetrics(std::string &request) override;
  NSCAPI::nagiosReturn submitMetrics(const std::string &request) override;
  void handleMessage(const char *data, unsigned int len) override;
  int commandLineExec(bool targeted, std::string &request, std::string &reply) override;
  bool has_command_line_exec() override { return false; }
  bool is_duplicate(boost::filesystem::path file, std::string alias) override;

  bool has_routing_handler() override { return false; }

  bool route_message(const char *channel, const char *buffer, unsigned int buffer_len, char **new_channel_buffer, char **new_buffer,
                     unsigned int *new_buffer_len) override;

  bool hasMetricsFetcher() override { return false; }
  bool hasMetricsSubmitter() override { return false; }

  std::string getModule() override;

  static void on_log_message(std::string &) {}
  std::string get_version() override;

 private:
  logging::logger_instance get_logger() { return logger_; }
  void on_log_message(const std::string &) override {}
  void read_metadata();
  void read_metadata(const std::string &string);
};
}  // namespace core
}  // namespace nsclient
