// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// check_firewall models the Windows firewall (three profiles, each an on/off
// boolean). Linux firewalls (firewalld/ufw/nftables) use a fundamentally
// different zone/policy model, so this check is Windows-only for now.

#include "check_firewall.hpp"

namespace firewall_source {

void gather(std::vector<firewall_filter::filter_obj_ptr> & /*out*/, std::string &error) {
  error = "check_firewall is not supported on this platform (Windows-only; the Linux firewall model differs)";
}

}  // namespace firewall_source
