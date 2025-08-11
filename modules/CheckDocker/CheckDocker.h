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

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/thread.hpp>
#include <nscapi/nscapi_protobuf_command.hpp>
#include <nscapi/nscapi_protobuf_log.hpp>
#include <nscapi/nscapi_protobuf_metrics.hpp>
#include <nscapi/plugin.hpp>
#include <string>

class CheckDocker : public nscapi::impl::simple_plugin {
 public:
  CheckDocker() {}

  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  void check_docker(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void handleLogMessage(const PB::Log::LogEntry::Entry &message);
  void fetchMetrics(PB::Metrics::MetricsMessage::Response *response);

  std::size_t get_errors(std::string &last_error);
};