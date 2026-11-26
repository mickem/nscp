// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The Windows source of check_smb_client: the "SMB Client Shares" performance
// counter set, through PDH.

#include <boost/algorithm/string/trim.hpp>
#include <string>
#include <win/pdh/pdh_interface.hpp>
#include <win/pdh/pdh_object_gather.hpp>

#include "check_smb_client.hpp"

namespace smb_client_check {

counter_snapshot read_counters() {
  counter_snapshot snapshot;
  try {
    // The latencies are averages and the rest rates: both need two samples a
    // second apart, or every one of them reads 0.
    snapshot.instances = PDH::gather_object_instances(counter_set, counter_names(), true);
  } catch (const PDH::pdh_exception &e) {
    // PDH messages come from FormatMessage and end in \r\n; trim so the check
    // output stays a single line.
    std::string reason = e.reason();
    boost::algorithm::trim(reason);
    snapshot.error = "The SMB Client Shares performance counters are not available - they need Windows 8 / Windows Server 2012 or later (" + reason + ")";
  }
  return snapshot;
}

namespace check {
void check_smb_client(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_smb_client_with(request, response, &read_counters);
}
}  // namespace check

}  // namespace smb_client_check
