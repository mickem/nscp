// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once
#include <facts/service_facts.hpp>
#include <functional>

namespace service_facts {
// Direct argv execution; the runner throws on timeout or a nonzero exit.
using command_runner = std::function<std::string(const std::vector<std::string> &)>;
std::vector<service> gather_systemd(const command_runner &run);
}  // namespace service_facts
