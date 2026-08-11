// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <map>
#include <nscapi/protobuf/command.hpp>
#include <string>
#include <trend/trend_buffer.hpp>

struct check_drive {
  // Per-drive used-space history from the CheckDisk collector (keyed like the
  // collector's disk_free rows), feeding the full_in/rate trend keywords. Pass
  // an empty map when no collector is running: the keywords stay unset.
  typedef std::map<std::string, trend::trend_buffer> trend_map;

  static void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                    const trend_map &trends);
};
