// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckIIS.h"

#include "check_iis_checks.hpp"

bool CheckIIS::loadModuleEx(const std::string &, NSCAPI::moduleLoadMode) { return true; }
bool CheckIIS::unloadModule() { return true; }

void CheckIIS::check_iis_app_pools(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_iis::check_iis_app_pools(request, response);
}
void CheckIIS::check_iis_sites(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_iis::check_iis_sites(request, response);
}
void CheckIIS::check_iis_worker_processes(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_iis::check_iis_worker_processes(request, response);
}
void CheckIIS::check_iis_request_queues(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_iis::check_iis_request_queues(request, response);
}
