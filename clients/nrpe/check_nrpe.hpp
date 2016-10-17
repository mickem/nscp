#pragma once

#include <boost/tuple/tuple.hpp>

#include <nscapi/nscapi_protobuf.hpp>
#include <client/command_line_parser.hpp>
#include <nscapi/nscapi_targets.hpp>
#include <nscapi/nscapi_protobuf_types.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>
#include <socket/client.hpp>

#include <nrpe/packet.hpp>
#include <nrpe/client/nrpe_client_protocol.hpp>

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;

#include "../modules/NRPEClient/nrpe_client.hpp"
#include "../modules/NRPEClient/nrpe_handler.hpp"

class check_nrpe {
private:
	client::configuration client_;

public:
	check_nrpe();
	void query(const Plugin::QueryRequestMessage &request, Plugin::QueryResponseMessage &response);
};