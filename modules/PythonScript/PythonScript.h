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

#include <nscapi/nscapi_protobuf_command.hpp>
#include <nscapi/nscapi_protobuf_metrics.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>


#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <boost/python.hpp>
#include <boost/shared_ptr.hpp>

class PythonScript : public nscapi::impl::simple_plugin {
private:
	boost::filesystem::path root_;
	std::string alias_;

	boost::shared_ptr<script_provider_interface> provider_;

public:
	PythonScript() {}
	virtual ~PythonScript() {}
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	void query_fallback(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, const PB::Commands::QueryRequestMessage &request_message);
	void handleNotification(const std::string &channel, const PB::Commands::QueryResponseMessage::Response &request, PB::Commands::SubmitResponseMessage::Response *response, const PB::Commands::SubmitRequestMessage &request_message);
	bool commandLineExec(const int target_mode, const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response, const PB::Commands::ExecuteRequestMessage &request_message);
	void submitMetrics(const PB::Metrics::MetricsMessage &response);
	void fetchMetrics(PB::Metrics::MetricsMessage::Response *response);
	void onEvent(const PB::Commands::EventMessage &request, const std::string &buffer);

	void execute_script(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response);

private:
	void loadScript(std::string alias, std::string script);
	//boost::optional<boost::filesystem::path> find_file(std::string file);
};
