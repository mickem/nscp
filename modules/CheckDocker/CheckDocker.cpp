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

#include "CheckDocker.h"

#include <boost/date_time.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/helpers.hpp>

#include "check_docker.hpp"

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

bool CheckDocker::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
  // sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
  return true;
}

bool CheckDocker::unloadModule() { return true; }
std::string render(int, const std::string, int, std::string message) { return message; }
void CheckDocker::handleLogMessage(const PB::Log::LogEntry::Entry &_message) {
  /*
  if (message.level() != PB::Log::LogEntry_Entry_Level_LOG_CRITICAL && message.level() != PB::Log::LogEntry_Entry_Level_LOG_ERROR)
          return;
  {
          boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
          if (!lock.owns_lock())
                  return;
          error_count_++;
          last_error_ = message.message();
  }
  */
}

void CheckDocker::fetchMetrics(PB::Metrics::MetricsMessage::Response *_response) {}

void CheckDocker::check_docker(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  docker_checks::check(request, response);
}
