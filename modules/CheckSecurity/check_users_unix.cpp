// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unix logged-on users via the utmp database (the same source as `who`).

#include "check_users.hpp"

#include <utmpx.h>

#include <cstring>

namespace users_source {

void gather(std::vector<users_filter::filter_obj_ptr> &out, std::string & /*error*/) {
  setutxent();
  for (struct utmpx *ut = getutxent(); ut != nullptr; ut = getutxent()) {
    if (ut->ut_type != USER_PROCESS) continue;  // only real interactive logins
    if (ut->ut_user[0] == '\0') continue;

    auto o = std::make_shared<users_filter::filter_obj>();
    o->user.assign(ut->ut_user, strnlen(ut->ut_user, sizeof(ut->ut_user)));
    o->client.assign(ut->ut_host, strnlen(ut->ut_host, sizeof(ut->ut_host)));
    // utmp only records live logins, so everything here is "active". A non-empty
    // host means the login came in over the network (ssh/rlogin/X display).
    o->session_state = "active";
    o->session_type = o->client.empty() ? "console" : "remote";
    out.push_back(o);
  }
  endutxent();
}

}  // namespace users_source
