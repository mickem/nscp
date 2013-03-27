#include "stdafx.h"

#include <boost/asio.hpp>
#include <protobuf/plugin.pb.h>
#include <nscapi/functions.hpp>

#include "handler_impl.hpp"



nscp::packet handler_impl::process(const nscp::packet &packet) {
	if (nscp::checks::is_query_request(packet)) {
		Plugin::QueryRequestMessage msg;
		msg.ParseFromString(packet.payload);
		std::wstring command = _T("todo: fixme");//utf8::cvt<std::wstring>(msg.command());

		std::string reply;
		try {
			NSCAPI::nagiosReturn returncode = handle_query_request(packet.payload, msg, reply);
			if (returncode == NSCAPI::returnIgnored)
				nscapi::protobuf::functions::create_simple_query_response_unknown(command, _T("Command was not found: ") + command, _T(""), reply);
		} catch (const nscp::nscp_exception &e) {
			nscapi::protobuf::functions::create_simple_query_response_unknown(command, _T("Processing error: ") + command + _T(": ") + utf8::cvt<std::wstring>(e.what()), _T(""), reply);
		} catch (const std::exception &e) {
			nscapi::protobuf::functions::create_simple_query_response_unknown(command, _T("Unknown error processing: ") + command + _T(": ") + utf8::cvt<std::wstring>(e.what()), _T(""), reply);
		}
		return nscp::factory::create_query_response(reply);
	} else if (nscp::checks::is_submit_request(packet)) {
		Plugin::SubmitRequestMessage msg;
		msg.ParseFromString(packet.payload);
		try {
			std::string reply;
			handle_submission_request(packet.payload, msg, reply);
			return nscp::factory::create_submission_response(reply);
		} catch (const nscp::nscp_exception &e) {
			return nscp::factory::create_error(_T("Exception processing message: ") + to_wstring(e.what()));
		} catch (const std::exception &e) {
			return nscp::factory::create_error(_T("Exception processing message: ") + to_wstring(e.what()));
		}
	} else if (nscp::checks::is_exec_request(packet)) {
		Plugin::ExecuteRequestMessage msg;
		msg.ParseFromString(packet.payload);
		try {
			std::string reply;
			handle_exec_request(packet.payload, msg, reply);
			return nscp::factory::create_submission_response(reply);
		} catch (const nscp::nscp_exception &e) {
			return nscp::factory::create_error(_T("Exception processing message: ") + to_wstring(e.what()));
		} catch (const std::exception &e) {
			return nscp::factory::create_error(_T("Exception processing message: ") + to_wstring(e.what()));
		}
	} else {
		this->log_error("nscp:handler", __FILE__, __LINE__, "Unknown packet: " + packet.to_string());
		return nscp::factory::create_error(_T("Unknown packet: ") + packet.to_wstring());
	}
	return nscp::factory::create_error(_T("Unknown error..."));
}
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
			nscapi::protobuf::functions::create_simple_query_response("_NSCP_CHECK", NSCAPI::returnOK, "I (" + nscapi::plugin_singleton->get_core()->getApplicationVersionString() + ") seem to be doing fine...", "", outBuffer);
		} else if (!allowArgs_ && payload.arguments_size() > 0) {
			nscapi::protobuf::functions::create_simple_query_response_unknown(command, _T("Arguments not allowed for command: ") + command, _T(""), outBuffer);
		} else {
			std::string tmpBuffer;
			Plugin::QueryRequestMessage tmp;
			tmp.mutable_header()->CopyFrom(hdr);
			tmp.add_payload()->CopyFrom(payload);
			tmp.SerializeToString(&tmpBuffer);
			NSCAPI::nagiosReturn returncode = nscapi::plugin_singleton->get_core()->query(payload.command(), tmpBuffer, outBuffer);
			if (returncode == NSCAPI::returnIgnored) {
				nscapi::protobuf::functions::create_simple_query_response_unknown(command, _T("Command was not found: ") + command, _T(""), outBuffer);
			}
			Plugin::QueryResponseMessage tmpResponse;
			tmpResponse.ParseFromString(outBuffer);
			for (int i=0;i<tmpResponse.payload_size();i++) {
				response.add_payload()->CopyFrom(tmpResponse.payload(i));
			}
		}
		response.SerializeToString(&reply);
	}
	// @todo: fixme this should probably be an aggregate right?
	return NSCAPI::isSuccess;
}




