#pragma once

#include <boost/tuple/tuple.hpp>

#include <protobuf/plugin.pb.h>

#include <client/command_line_parser.hpp>
#include <nscapi/nscapi_targets.hpp>
#include <nscapi/nscapi_protobuf_types.hpp>
#include <nscapi/nscapi_plugin_interface.hpp>
#include <socket/client.hpp>

#include <nrpe/packet.hpp>
#include <nrpe/client/nrpe_client_protocol.hpp>

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;


#include "../modules/NRPEClient/nrpe_client.hpp"

class check_nrpe  {
private:
	client::command_manager commands;
	nscapi::targets::handler<nrpe_client::custom_reader> targets;

public:
	check_nrpe();
	void query(const std::vector<std::string> &args, Plugin::QueryResponseMessage::Response &response);


};

