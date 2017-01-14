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

#include <string>
#include <boost/filesystem/path.hpp>

#ifdef WIN32
typedef int pid_t;
#else
#include <unistd.h>
#include <sys/types.h>
#endif

class pidfile {
public:
	pidfile(boost::filesystem::path const & rundir, std::string const & process_name);
	pidfile(boost::filesystem::path const & filepath);
	~pidfile();

	bool create(pid_t const pid) const;
	bool create() const;
	bool remove() const;

	pid_t get_pid() const;

	static boost::filesystem::path get_default_rundir() {
		return "/var/run";
	}

	static std::string get_default_pidfile(const std::string &name) {
		return (get_default_rundir() / (name + ".pid")).string();
	}

private:
	boost::filesystem::path filepath_;
};