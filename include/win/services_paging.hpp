/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <bytes/buffer.hpp>
#include <error/error.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <win/windows.hpp>

namespace win_list_services {
namespace detail {

// Outcome of one EnumServicesStatusEx call. Mirrors the BOOL/GetLastError
// pair plus the two output counters that drive paging decisions.
struct enum_page_status {
  bool success;        // BOOL return cast to bool
  DWORD last_error;    // GetLastError() when !success; ignored otherwise
  DWORD bytes_needed;  // *pcbBytesNeeded
  DWORD count;         // *lpServicesReturned
};

// Drives the EnumServicesStatusEx paging state machine.
//
// fetch is called twice per page:
//   - probe with buf=nullptr, buf_size=0 to discover page size,
//   - bulk with a sized buffer to fetch the page.
// process is called once per filled buffer.
//
// Loops until the SCM signals no more data: success on probe, success on
// bulk, or zero-byte probe with ERROR_MORE_DATA. Throws on any other
// error so callers don't have to.
//
// Fetch:   enum_page_status(LPBYTE buf, DWORD buf_size, DWORD &resume_handle)
// Process: void(const BYTE *buf, DWORD count)
//
// Pulled out of enum_services so the paging logic that regressed in
// #703 / #229 (treating bulk-call ERROR_MORE_DATA as fatal instead of
// paging through it) is unit-testable without a live SCM.
template <class Fetch, class Process>
void enumerate_paged_services(Fetch fetch, Process process) {
  for (;;) {
    DWORD handle = 0;
    const enum_page_status probe = fetch(nullptr, 0, handle);
    if (probe.success) return;
    if (probe.last_error != ERROR_MORE_DATA) {
      throw nsclient::nsclient_exception("Failed to enumerate service status: " + error::format::from_system(probe.last_error));
    }
    if (probe.bytes_needed == 0) return;

    const hlp::buffer<BYTE> buf(probe.bytes_needed + 10);
    const enum_page_status bulk = fetch(buf.get(), probe.bytes_needed, handle);
    if (!bulk.success && bulk.last_error != ERROR_MORE_DATA) {
      throw nsclient::nsclient_exception("Failed to enumerate service: " + error::format::from_system(bulk.last_error));
    }

    process(buf.get(), bulk.count);

    if (bulk.success) return;
  }
}

}  // namespace detail
}  // namespace win_list_services
