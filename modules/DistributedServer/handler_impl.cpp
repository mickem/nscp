#include "stdafx.h"

#include <boost/asio.hpp>
#include <protobuf/plugin.pb.h>
#include <nscapi/functions.hpp>

#include "handler_impl.hpp"

#include "stdafx.h"

#include <boost/asio.hpp>
#include <protobuf/plugin.pb.h>
#include <nscapi/functions.hpp>

#include "handler_impl.hpp"

NSCAPI::nagiosReturn handler_impl::handle_query_request(const std::string &request, Plugin::QueryRequestMessage &msg, std::string &reply) {
	Plugin::Common::Header hdr;
	hdr.CopyFrom(msg.header());

	Plugin::QueryResponseMessage response;
	// @todo: swap data in the dhear (ie. sender /recipent)
	response.mutable_header()->CopyFrom(hdr);

	// @todo: Make split optional
	for (int i=0;i<msg.payload_size();i++) {
		const Plugin::QueryRequestMessage_Request &payload = msg.payload(i);
		std::string outBuffer;
		std::wstring command = utf8::cvt<std::wstring>(payload.command());

		if (command.empty() || command == _T("_NSCP_CHECK")) {
			nscapi::functions::create_simple_query_response(_T("_NSCP_CHECK"), NSCAPI::returnOK, _T("I (") + nscapi::plugin_singleton->get_core()->getApplicationVersionString() + _T(") seem to be doing fine..."), _T(""), outBuffer);
		} else if (!allowArgs_ && payload.arguments_size() > 0) {
			nscapi::functions::create_simple_query_response_unknown(command, _T("Arguments not allowed for command: ") + command, _T(""), reply);
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
		Plugin::QueryResponseMessage tmpResponse;
		tmpResponse.ParseFromString(outBuffer);
		for (int i=0;i<tmpResponse.payload_size();i++) {
			response.add_payload()->CopyFrom(tmpResponse.payload(i));
		}
	}
	response.SerializeToString(&reply);
	// @todo: fixme this should probably be an aggregate right?
	return NSCAPI::isSuccess;
}







