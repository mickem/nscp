// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckActiveDirectory.h"

#include <chrono>

#include "bounded_worker.hpp"
#include "check_ad_replication.hpp"
#include "check_kdc.hpp"
#include "check_secure_channel.hpp"

// Last, and on its own: a Windows.h that precedes an asio include drags in
// the old winsock.h and breaks it. Only GetModuleHandleExW is needed here.
#include <Windows.h>

namespace {

// Keep this DLL mapped for the rest of the process. A worker parked in
// bounded_workers is blocked inside a Windows call that cannot be cancelled,
// and when that call returns it returns into this module: unloading the DLL
// under it would turn a slow directory server into an access violation. Only
// taken when unload finds a worker still blocked; the cost is that the module
// can no longer be unloaded and reloaded without restarting the service.
void pin_this_module() {
  static const char anchor = 0;
  HMODULE self = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&anchor), &self);
}

}  // namespace

CheckActiveDirectory::CheckActiveDirectory() {}

bool CheckActiveDirectory::loadModuleEx(std::string, NSCAPI::moduleLoadMode) { return true; }

bool CheckActiveDirectory::unloadModule() {
  check_ad::module_workers().shutdown(std::chrono::seconds(2), pin_this_module);
  return true;
}

void CheckActiveDirectory::check_ad_replication(const PB::Commands::QueryRequestMessage::Request &request,
                                                PB::Commands::QueryResponseMessage::Response *response) {
  check_ad_replication_command::check(request, response);
}

void CheckActiveDirectory::check_secure_channel(const PB::Commands::QueryRequestMessage::Request &request,
                                                PB::Commands::QueryResponseMessage::Response *response) {
  check_secure_channel_command::check(request, response);
}

void CheckActiveDirectory::check_kdc(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_kdc_command::check(request, response);
}
