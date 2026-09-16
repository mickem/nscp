// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "GearmanClient.h"

#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>

GearmanClient::GearmanClient() = default;
GearmanClient::~GearmanClient() = default;

bool GearmanClient::loadModuleEx(std::string, NSCAPI::moduleLoadMode) {
  // Say so rather than sit there looking configured: an operator who enabled
  // the module and sees no checks arrive should not have to read the source to
  // find out why.
  NSC_LOG_WARNING("GearmanClient is loaded but does not run checks yet: the worker arrives in a later release.");
  return true;
}

bool GearmanClient::unloadModule() { return true; }
