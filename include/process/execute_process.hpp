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
		bool fork;
		bool display;
		bool ignore_perf;
	};
	void kill_all();
	int execute_process(process::exec_arguments args, std::string &output);
}