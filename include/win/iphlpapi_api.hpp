// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#ifdef WIN32

// clang-format off
#include <win/windows.hpp>
// clang-format on
#include <iphlpapi.h>

namespace win {

typedef DWORD(WINAPI *get_tcp_table_fn)(PMIB_TCPTABLE pTcpTable, PULONG pdwSize, BOOL bOrder);
typedef DWORD(WINAPI *get_udp_table_fn)(PMIB_UDPTABLE pUdpTable, PULONG pdwSize, BOOL bOrder);
typedef DWORD(WINAPI *get_extended_tcp_table_fn)(PVOID pTcpTable, PULONG pdwSize, BOOL bOrder, ULONG ulAf, TCP_TABLE_CLASS tableClass, ULONG reserved);
typedef DWORD(WINAPI *get_extended_udp_table_fn)(PVOID pUdpTable, PULONG pdwSize, BOOL bOrder, ULONG ulAf, UDP_TABLE_CLASS tableClass, ULONG reserved);

struct iphlpapi_api {
  HMODULE dll;
  get_tcp_table_fn get_tcp_table;
  get_udp_table_fn get_udp_table;
  get_extended_tcp_table_fn get_extended_tcp_table;
  get_extended_udp_table_fn get_extended_udp_table;

  iphlpapi_api() : dll(NULL), get_tcp_table(NULL), get_udp_table(NULL), get_extended_tcp_table(NULL), get_extended_udp_table(NULL) {
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
