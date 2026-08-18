// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckRDS.h"

#include "check_rds_licenses.hpp"
#include "check_rds_sessions.hpp"

bool CheckRDS::loadModuleEx(const std::string &, NSCAPI::moduleLoadMode) { return true; }
bool CheckRDS::unloadModule() { return true; }

void CheckRDS::check_rds_licenses(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_rds::check_rds_licenses(request, response);
}
void CheckRDS::check_rds_sessions(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_rds::check_rds_sessions(request, response);
}
void CheckRDS::check_rds_session_load(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_rds::check_rds_session_load(request, response);
}
void CheckRDS::check_rds_broker(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_rds::check_rds_broker(request, response);
}
