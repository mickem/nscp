#pragma once

#include <boost/tuple/tuple.hpp>

#include <nscapi/nscapi_protobuf.hpp>
#include <client/command_line_parser.hpp>
#include <nscapi/nscapi_targets.hpp>
#include <nscapi/nscapi_protobuf_types.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>
#include <socket/client.hpp>

#include <nscp/packet.hpp>
#include <nscp/client/nscp_client_protocol.hpp>

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;

#include "../modules/NSCPClient/nscp_client.hpp"
#include "../modules/NSCPClient/nscp_handler.hpp"

class check_nscp {
private:
	client::configuration client_;

public:
	check_nscp();
	void query(const Plugin::QueryRequestMessage &request, Plugin::QueryResponseMessage &response);
};