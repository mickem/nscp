// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>

namespace check_rds {

// Check Remote Desktop licensing via WMI Win32_TSLicenseKeyPack (issued vs
// available CALs per key pack).
void check_rds_licenses(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace check_rds
