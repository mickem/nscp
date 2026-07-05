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
