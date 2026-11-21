// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Stopping the Icinga 2 agent NSClient++ takes over from
// (`nscp icinga-agent import --stop-icinga`).

#include <string>

namespace icinga_agent {

// Stop the `icinga2` service and keep it from starting again: disabled in the
// Windows service manager, `systemctl disable --now` elsewhere. Disabled
// first, so a service that is slow to stop still does not come back. Returns
// a line saying what was done. Throws std::runtime_error when there is no such
// service, or it could not be disabled or stopped.
std::string stop_icinga_service();

}  // namespace icinga_agent
