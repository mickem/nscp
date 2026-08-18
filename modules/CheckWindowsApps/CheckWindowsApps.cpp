// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckWindowsApps.h"

#include "check_iis_checks.hpp"
#include "check_rds_licenses.hpp"
#include "check_rds_sessions.hpp"

bool CheckWindowsApps::loadModuleEx(const std::string &, NSCAPI::moduleLoadMode) { return true; }
bool CheckWindowsApps::unloadModule() { return true; }

void CheckWindowsApps::check_iis_app_pools(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_iis::check_iis_app_pools(request, response);
}
void CheckWindowsApps::check_iis_sites(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_iis::check_iis_sites(request, response);
}
void CheckWindowsApps::check_iis_worker_processes(const PB::Commands::QueryRequestMessage::Request &request,
                                                  PB::Commands::QueryResponseMessage::Response *response) {
  check_iis::check_iis_worker_processes(request, response);
}
void CheckWindowsApps::check_iis_request_queues(const PB::Commands::QueryRequestMessage::Request &request,
                                                PB::Commands::QueryResponseMessage::Response *response) {
  check_iis::check_iis_request_queues(request, response);
}

void CheckWindowsApps::check_rds_licenses(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_rds::check_rds_licenses(request, response);
}
void CheckWindowsApps::check_rds_sessions(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_rds::check_rds_sessions(request, response);
}
void CheckWindowsApps::check_rds_session_load(const PB::Commands::QueryRequestMessage::Request &request,
                                              PB::Commands::QueryResponseMessage::Response *response) {
  check_rds::check_rds_session_load(request, response);
}
void CheckWindowsApps::check_rds_broker(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_rds::check_rds_broker(request, response);
}
