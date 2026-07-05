// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CauseCrashes.h"

#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_convert.hpp>
#include <nscapi/protobuf/functions_response.hpp>

namespace po = boost::program_options;

void CauseCrashes::crash_client(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  po::options_description desc = nscapi::program_options::create_desc(request);
  po::variables_map vm;
  if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response)) return;
  int *foo = 0;
  *foo = 0;
  return nscapi::protobuf::functions::set_response_bad(*response, "We should have crashed now...");
}