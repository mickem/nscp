/**************************************************************************
*   Copyright (C) 2004-2008 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
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
		exec_arguments(std::string root_path_, std::string command_, unsigned int timeout_, const std::string &encoding)
			: root_path(root_path_)
			, command(command_)
			, timeout(timeout_) 
			, encoding(encoding)
		{}

		std::string root_path;
		std::string command;
		unsigned int timeout;
		std::string user;
		std::string domain;
		std::string password;
		std::string encoding;
	};
	int execute_process(process::exec_arguments args, std::string &output);
}
