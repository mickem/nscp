// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <functional>

#include "check_service.h"

namespace systemd_units {
// Arguments exclude the executable, so both checks and facts use these queries
// with their own command failure policy.
using command_runner = std::function<std::string(const std::vector<std::string> &)>;
std::vector<std::string> list_services(const command_runner &run);
std::vector<checks::check_svc_filter::filter_obj> show_services(const std::vector<std::string> &names, const command_runner &run,
                                                                const std::string &properties = "", bool require_complete = false);
}  // namespace systemd_units
