// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// check_smb_client outside Windows: the counter set it reads is a Windows
// one, so the check reports itself unavailable rather than an empty (and
// therefore OK) set of shares. A CIFS reader (/proc/fs/cifs/Stats) would go in
// an smb_client_source_linux.cpp of its own.

#include "check_smb_client.hpp"

namespace smb_client_check {

counter_snapshot read_counters() {
  counter_snapshot snapshot;
  snapshot.error = "check_smb_client reads the Windows SMB Client Shares performance counters and is only available on Windows";
  return snapshot;
}

namespace check {
void check_smb_client(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_smb_client_with(request, response, &read_counters);
}
}  // namespace check

}  // namespace smb_client_check
