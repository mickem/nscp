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

#include "CheckSystem.h"

#include <boost/program_options.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_settings_helper.hpp>

#include "check_cpu.h"
#include "check_memory.h"
#include "check_os_version.h"
#include "check_pagefile.h"
#include "check_process.h"
#include "check_service.h"
#include "check_uptime.h"

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

/**
 * New version of the load call.
 * Start the background collector thread and let it run until unloadModule() is called.
 * @return true
 */
bool CheckSystem::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
  sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
  settings.set_alias("system", alias, "unix");
  std::string counter_path = settings.alias().get_settings_path("counters");

  // Start the CPU collector thread
  collector_ = boost::shared_ptr<pdh_thread>(new pdh_thread());
  collector_->start();

  return true;
}

/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool CheckSystem::unloadModule() {
  if (collector_) {
    collector_->stop();
    collector_.reset();
  }
  return true;
}

void CheckSystem::check_service(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  checks::check_service(request, response);
}
void CheckSystem::check_memory(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_memory::check_memory(collector_, request, response);
}
void CheckSystem::check_process(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_proc::check_process(request, response);
}
void CheckSystem::check_cpu(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  checks::check_cpu(collector_, request, response);
}
void CheckSystem::check_uptime(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  checks::check_uptime(request, response);
}
void CheckSystem::check_pagefile(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_page::check_pagefile(collector_, request, response);
}
void CheckSystem::check_os_version(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  os_version::check_os_version(request, response);
}
