/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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