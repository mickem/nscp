// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once
#include <facts/service_facts.hpp>

#include "systemd_units_linux.hpp"

namespace service_facts {
// Direct argv execution; the runner throws on timeout or a nonzero exit.
using command_runner = systemd_units::command_runner;
std::vector<service> gather_systemd(const command_runner &run);
}  // namespace service_facts
