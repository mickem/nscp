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

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>

class CheckDisk : public nscapi::impl::simple_plugin {
private:
	bool show_errors_;

public:
	CheckDisk();

	std::wstring get_filter(unsigned int drvType);

	// Check commands
	NSCAPI::nagiosReturn check_filesize(const std::string &target, const std::string &command, std::list<std::string> &arguments, std::string &msg, std::string &perf);
	NSCAPI::nagiosReturn check_files(const std::string &target, const std::string &command, std::list<std::string> &arguments, std::string &msg, std::string &perf);
	void check_files(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_drivesize(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);

	void checkDriveSize(Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void checkFiles(Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
};
