/*
 * Copyright (C) 2004-2016 Michael Medin
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

#ifdef WIN32

// clang-format off
#include <win/windows.hpp>
// clang-format on
#include <iphlpapi.h>

namespace win {

typedef DWORD(WINAPI *get_tcp_table_fn)(PMIB_TCPTABLE pTcpTable, PULONG pdwSize, BOOL bOrder);
typedef DWORD(WINAPI *get_udp_table_fn)(PMIB_UDPTABLE pUdpTable, PULONG pdwSize, BOOL bOrder);
typedef DWORD(WINAPI *get_extended_tcp_table_fn)(PVOID pTcpTable, PULONG pdwSize, BOOL bOrder, ULONG ulAf,
                                                 TCP_TABLE_CLASS tableClass, ULONG reserved);
typedef DWORD(WINAPI *get_extended_udp_table_fn)(PVOID pUdpTable, PULONG pdwSize, BOOL bOrder, ULONG ulAf,
                                                 UDP_TABLE_CLASS tableClass, ULONG reserved);

struct iphlpapi_api {
  HMODULE dll;
  get_tcp_table_fn get_tcp_table;
  get_udp_table_fn get_udp_table;
  get_extended_tcp_table_fn get_extended_tcp_table;
  get_extended_udp_table_fn get_extended_udp_table;

  iphlpapi_api()
      : dll(NULL), get_tcp_table(NULL), get_udp_table(NULL), get_extended_tcp_table(NULL), get_extended_udp_table(NULL) {
    dll = ::LoadLibraryA("iphlpapi.dll");
    if (!dll) return;
    get_tcp_table = reinterpret_cast<get_tcp_table_fn>(::GetProcAddress(dll, "GetTcpTable"));
    get_udp_table = reinterpret_cast<get_udp_table_fn>(::GetProcAddress(dll, "GetUdpTable"));
    get_extended_tcp_table = reinterpret_cast<get_extended_tcp_table_fn>(::GetProcAddress(dll, "GetExtendedTcpTable"));
    get_extended_udp_table = reinterpret_cast<get_extended_udp_table_fn>(::GetProcAddress(dll, "GetExtendedUdpTable"));
  }

  ~iphlpapi_api() {
    if (dll) ::FreeLibrary(dll);
  }
};

inline iphlpapi_api &load_iphlpapi() {
  static iphlpapi_api api;
  return api;
}

}  // namespace win

#endif

