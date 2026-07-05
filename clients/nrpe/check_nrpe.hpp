// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <client/command_line_parser.hpp>
#include <nscapi/protobuf/command.hpp>

class check_nrpe {
 private:
  client::configuration client_;

 public:
  check_nrpe();
  void query(const PB::Commands::QueryRequestMessage &request, PB::Commands::QueryResponseMessage &response);
};
