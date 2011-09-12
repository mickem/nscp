#include "stdafx.h"

#include <boost/asio.hpp>
#include <protobuf/plugin.pb.h>
#include <nscapi/functions.hpp>

#include "handler_impl.hpp"

std::list<nscp::packet> handler_impl::process(nscp::packet &packet) {
	std::list<nscp::packet> result;

	Plugin::Common::Header hdr;

	if (nscp::checks::is_query_request(packet)) {
		Plugin::QueryRequestMessage msg;
		msg.ParseFromString(packet.payload);
		hdr.CopyFrom(msg.header());

		// @todo: Make split optional
		// @todo: Make this return ONE response not multiple responses

		for (int i=0;i<msg.payload_size();i++) {
			const Plugin::QueryRequestMessage_Request &payload = msg.payload(i);
			std::string outBuffer;
			std::wstring command = utf8::cvt<std::wstring>(payload.command());

			if (command.empty() || command == _T("_NSCP_CHECK")) {
				nscapi::functions::create_simple_query_response(_T("_NSCP_CHECK"), NSCAPI::returnOK, _T("I (") + nscapi::plugin_singleton->get_core()->getApplicationVersionString() + _T(") seem to be doing fine..."), _T(""), outBuffer);
			} else if (!allowArgs_ && payload.arguments_size() > 0) {
				nscapi::functions::create_simple_query_response_unknown(command, _T("Arguments not allowed for command: ") + command, _T(""), outBuffer);
			} else {
				bool ok = true;
				if (!allowNasty_) {
					for (int j=0;j<payload.arguments_size();j++) {
						if (payload.arguments(j).find_first_of(NASTY_METACHARS) != std::wstring::npos) {
							ok = false;
							break;
						}
					}
				}
				if (ok) {
					std::string tmpBuffer;
					Plugin::QueryRequestMessage tmp;
					tmp.mutable_header()->CopyFrom(hdr);
					tmp.add_payload()->CopyFrom(payload);
					tmp.SerializeToString(&tmpBuffer);
					NSCAPI::nagiosReturn returncode = nscapi::plugin_singleton->get_core()->query(command, tmpBuffer, outBuffer);
					if (returncode == NSCAPI::returnIgnored) {
						nscapi::functions::create_simple_query_response_unknown(command, _T("Command was not found: ") + command, _T(""), outBuffer);
					}
				} else {
					nscapi::functions::create_simple_query_response_unknown(command, _T("Nasty arguments not allowed for command: ") + command, _T(""), outBuffer);
				}
			}
			result.push_back(nscp::factory::create_query_response(outBuffer));
		}
	} else if (nscp::checks::is_query_response(packet)) {

		// @todo handle submission here

	} else {
		NSC_LOG_ERROR(_T("Unknown packet: ") + packet.to_wstring());
		result.push_back(create_error(_T("Unknown packet: ") + packet.to_wstring()));
	}
	return result;
}




