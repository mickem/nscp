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

// Windows logged-on sessions via the Remote Desktop Services (WTS) API — no WMI
// dependency, and it distinguishes console from RDP and active from disconnected.

#include "check_users.hpp"

#include <Windows.h>
#include <WtsApi32.h>

#include <str/utf8.hpp>

namespace users_source {

namespace {

std::string query_string(DWORD session, WTS_INFO_CLASS info) {
  LPWSTR buf = nullptr;
  DWORD bytes = 0;
  std::string out;
  if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, session, info, &buf, &bytes) && buf != nullptr) {
    out = utf8::cvt<std::string>(std::wstring(buf));
  }
  if (buf != nullptr) WTSFreeMemory(buf);
  return out;
}

std::string state_name(WTS_CONNECTSTATE_CLASS s) {
  switch (s) {
    case WTSActive:
      return "active";
    case WTSDisconnected:
      return "disconnected";
    case WTSConnected:
      return "connected";
    case WTSIdle:
      return "idle";
    case WTSListen:
      return "listen";
    default:
      return "other";
  }
}

}  // namespace

void gather(std::vector<users_filter::filter_obj_ptr> &out, std::string &error) {
  PWTS_SESSION_INFOW sessions = nullptr;
  DWORD count = 0;
  if (!WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessions, &count)) {
    error = "Failed to enumerate terminal sessions (WTSEnumerateSessions)";
    return;
  }

  for (DWORD i = 0; i < count; ++i) {
    const DWORD id = sessions[i].SessionId;
    const std::string user = query_string(id, WTSUserName);
    if (user.empty()) continue;  // skip services / the listener session

    auto o = std::make_shared<users_filter::filter_obj>();
    o->user = user;
    o->session_state = state_name(sessions[i].State);
    o->client = query_string(id, WTSClientName);

    // Protocol: 0 = console, 2 = RDP (1 = legacy Citrix ICA).
    USHORT *proto = nullptr;
    DWORD bytes = 0;
    o->session_type = "console";
    if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, id, WTSClientProtocolType, reinterpret_cast<LPWSTR *>(&proto), &bytes) && proto != nullptr) {
      o->session_type = (*proto == 2) ? "rdp" : (*proto == 1 ? "ica" : "console");
    }
    if (proto != nullptr) WTSFreeMemory(proto);

    out.push_back(o);
  }

  WTSFreeMemory(sessions);
}

}  // namespace users_source
