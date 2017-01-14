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

#include <pid_file.hpp>

#include <fstream>
#include <boost/filesystem/operations.hpp>

#ifdef WIN32
#include <process.h>
#endif
pidfile::pidfile(boost::filesystem::path const & rundir, std::string const & process_name)
	: filepath_((rundir.empty() ? get_default_rundir() : rundir) / (process_name + ".pid")) {}

pidfile::pidfile(boost::filesystem::path const & filepath) : filepath_(filepath) {}

pidfile::~pidfile() {
	try {
		remove();
	} catch (...) {}
}

bool pidfile::create(pid_t const pid) const {
	remove();

	std::ofstream out(filepath_.string().c_str());
	out << pid;
	return out.good();
}

bool pidfile::create() const {
	return create(get_pid());
}

pid_t pidfile::get_pid() const {
#ifdef WIN32
	return ::_getpid();
#else
	return ::getpid();
#endif
}

bool pidfile::remove() const {
	return boost::filesystem::remove(filepath_);
}