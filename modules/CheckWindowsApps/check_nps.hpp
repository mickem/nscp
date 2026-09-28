// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only
#pragma once
#include <nscapi/protobuf/command.hpp>
namespace check_nps {
void check_nps_auth(const PB::Commands::QueryRequestMessage::Request &, PB::Commands::QueryResponseMessage::Response *);
void check_nps_accounting(const PB::Commands::QueryRequestMessage::Request &, PB::Commands::QueryResponseMessage::Response *);
void check_nps_counters(const PB::Commands::QueryRequestMessage::Request &, PB::Commands::QueryResponseMessage::Response *);
}  // namespace check_nps
