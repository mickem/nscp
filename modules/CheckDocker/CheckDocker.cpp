// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckDocker.h"

#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/settings/helper.hpp>
#include <parsers/filter/modern_filter.hpp>

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
