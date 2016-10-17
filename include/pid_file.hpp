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