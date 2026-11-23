// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_service.hpp"

#include <stdexcept>

#ifdef WIN32
#include <win/service_control.hpp>
#else
#include <sys/wait.h>

#include <cstdio>
#endif

namespace icinga_agent {

#ifdef WIN32

// The service manager calls are the ones the service itself and the installer
// use (win/service_control.hpp), not a second copy of them.
std::string stop_icinga_service() {
  const std::wstring name = L"icinga2";
  try {
    if (!win_service_control::isInstalled(name)) throw std::runtime_error("There is no icinga2 service on this host");
    win_service_control::SetStartType(name, SERVICE_DISABLED);
    // Returns once the service has stopped; a service that was not running
    // is left as it is.
    win_service_control::Stop(name);
  } catch (const win_service_control::service_control_exception &e) {
    throw std::runtime_error(std::string(e.what()) + " (the service manager needs an elevated prompt)");
  }
  return "The icinga2 service is stopped and disabled.";
}

#else

std::string stop_icinga_service() {
  // A fixed command: nothing from the command line or the configuration.
  FILE *pipe = popen("systemctl disable --now icinga2 2>&1", "r");
  if (pipe == nullptr) throw std::runtime_error("Cannot run systemctl to stop the icinga2 service");
  std::string output;
  char buffer[256];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) output += buffer;
  const int status = pclose(pipe);
  while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) output.pop_back();
  if (status == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    throw std::runtime_error("`systemctl disable --now icinga2` failed" + (output.empty() ? std::string() : ": " + output));
  }
  return "The icinga2 service is stopped and disabled.";
}

#endif

}  // namespace icinga_agent
