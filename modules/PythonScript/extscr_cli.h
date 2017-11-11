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
#include "script_interface.hpp"


class extscr_cli {
private:
	boost::shared_ptr<script_provider_interface> provider_;
	std::string alias_;
public:

	extscr_cli(boost::shared_ptr<script_provider_interface> provider_, std::string alias_);

	bool run(std::string cmd, const Plugin::ExecuteRequestMessage_Request &request, Plugin::ExecuteResponseMessage_Response *response);
	void add_script(const Plugin::ExecuteRequestMessage_Request &request, Plugin::ExecuteResponseMessage_Response *response);
	void configure(const Plugin::ExecuteRequestMessage_Request &request, Plugin::ExecuteResponseMessage_Response *response);
	void list(const Plugin::ExecuteRequestMessage_Request &request, Plugin::ExecuteResponseMessage_Response *response);
	void show(const Plugin::ExecuteRequestMessage_Request &request, Plugin::ExecuteResponseMessage_Response *response);
	void delete_script(const Plugin::ExecuteRequestMessage_Request &request, Plugin::ExecuteResponseMessage_Response *response);
    
private:
	bool validate_sandbox(boost::filesystem::path pscript, Plugin::ExecuteResponseMessage::Response *response);

};
