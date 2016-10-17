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
