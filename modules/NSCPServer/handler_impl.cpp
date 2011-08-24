#include "stdafx.h"

#include <boost/asio.hpp>
#include <protobuf/plugin.pb.h>
#include <nscapi/functions.hpp>

#include "handler_impl.hpp"

std::list<nscp::packet> handler_impl::process(nscp::packet &packet) {
	std::list<nscp::packet> result;

	Plugin::Common::Header hdr;

	if (packet.is_command_request()) {
		Plugin::QueryRequestMessage msg;
		msg.ParseFromString(packet.payload);
		hdr.CopyFrom(msg.header());

		// @todo: Make split optional

		for (int i=0;i<msg.payload_size();i++) {

				// @todo re-add support for nasty arguments / arguments passing
	/*
	if (!allowNasty_) {
		if (cmd.first.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
			NSC_LOG_ERROR(_T("Request command contained illegal metachars!"));
			throw nscp::nscp_exception(_T("Request command contained illegal metachars!"));
		}
		if (cmd.second.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
			NSC_LOG_ERROR(_T("Request arguments contained illegal metachars!"));
			throw nscp::nscp_exception(_T("Request command contained illegal metachars!"));
		}
	}
	*/

			std::string outBuffer;
			std::wstring command = utf8::cvt<std::wstring>(msg.payload(i).command());
			if (command.empty() || command == _T("_NSCP_CHECK")) {
				nscapi::functions::create_simple_query_response(NSCAPI::returnOK, _T("I (") + nscapi::plugin_singleton->get_core()->getApplicationVersionString() + _T(") seem to be doing fine..."), _T(""), outBuffer);
			} else {
				std::string tmpBuffer;
				Plugin::QueryRequestMessage tmp;
				tmp.mutable_header()->CopyFrom(hdr);
				tmp.add_payload()->CopyFrom(msg.payload(i));
				tmp.SerializeToString(&tmpBuffer);
				NSCAPI::nagiosReturn returncode = nscapi::plugin_singleton->get_core()->query(command, tmpBuffer, outBuffer);
			}
			result.push_back(nscp::packet::create_query_response(outBuffer));
		}
	} else {
		NSC_LOG_ERROR(_T("Unknown packet: ") + packet.to_wstring());
		result.push_back(create_error(_T("Unknown packet: ") + packet.to_wstring()));
	}
	return result;
}




