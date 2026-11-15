// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <map>
#include <nscapi/protobuf/command.hpp>
#include <string>
#include <trend/trend_buffer.hpp>

#include "used_history.hpp"

struct check_drive {
  // Per-drive used-space history from the CheckDisk collector (keyed like the
  // collector's disk_free rows), feeding the full_in/rate trend keywords. Pass
  // an empty map when no collector is running: the keywords stay unset.
  typedef std::map<std::string, trend::trend_buffer> trend_map;
  // The collector's hour and day change in used space per drive, keyed the
  // same way, feeding used_change_1h/used_change_24h. Empty without a
  // collector: the keywords are then unknown.
  typedef used_history::change_map change_map;

  static void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                    const trend_map &trends, const change_map &changes);
};
