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

namespace process {
	class process_exception : public std::exception {
		std::string error;
	public:
		//////////////////////////////////////////////////////////////////////////
		/// Constructor takes an error message.
		/// @param error the error message
		///
		/// @author mickem
		process_exception(std::string error) : error(error) {}
		~process_exception() throw() {}

		//////////////////////////////////////////////////////////////////////////
		/// Retrieve the error message from the exception.
		/// @return the error message
		///
		/// @author mickem
		const char* what() const throw() { return error.c_str(); }
	};

	class exec_arguments {
	public:
		exec_arguments(std::string root_path_, std::string command_, unsigned int timeout_, const std::string &encoding, std::string session, bool display, bool fork)
			: root_path(root_path_)
			, command(command_)
			, timeout(timeout_)
			, encoding(encoding)
			, session(session)
			, display(false)
			, ignore_perf(false)
			, fork(fork)
		{}

		std::string alias;
		std::string root_path;
		std::string command;
		unsigned int timeout;
		std::string user;
		std::string domain;
		std::string password;
		std::string encoding;
		std::string session;
		bool display;
		bool ignore_perf;
		bool fork;
	};
	void kill_all();
	int execute_process(process::exec_arguments args, std::string &output);
}