/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
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

#include <boost/tuple/tuple.hpp>

#include <nscapi/nscapi_protobuf.hpp>
#include <client/command_line_parser.hpp>
#include <nscapi/nscapi_targets.hpp>
#include <nscapi/nscapi_protobuf_types.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <socket/client.hpp>

#include <nrpe/packet.hpp>
#include <nrpe/client/nrpe_client_protocol.hpp>
#include "nrpe_client.hpp"

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;

class NRPEClient : public nscapi::impl::simple_plugin {
private:

	std::string channel_;
	std::string target_path;

	client::command_manager commands;


public:
	NRPEClient();
	virtual ~NRPEClient();
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	void nrpe_forward(const std::string &command, Plugin::QueryRequestMessage &request, Plugin::QueryResponseMessage *response);
	void query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message);
	bool commandLineExec(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message);
	void handleNotification(const std::string &channel, const Plugin::SubmitRequestMessage &request, Plugin::SubmitResponseMessage *response);

private:
	nscapi::targets::handler<nrpe_client::custom_reader> targets;

	void add_options(po::options_description &desc, nrpe_client::connection_data &command_data);
	static nrpe_client::connection_data parse_header(const ::Plugin::Common_Header &header, client::configuration::data_type data);

private:
	void add_local_options(po::options_description &desc, client::configuration::data_type data);
	void add_command(std::string key, std::string args);
	void add_target(std::string key, std::string args);

	bool install_server(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response);
	bool make_cert(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response);
};

