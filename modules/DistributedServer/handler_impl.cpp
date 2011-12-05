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
	NSCAPI::nagiosReturn ret;
	Plugin::Common::Header hdr;
	hdr.CopyFrom(msg.header());

	Plugin::QueryResponseMessage response;
	// @todo: swap data in the dhear (ie. sender /recipent)
	response.mutable_header()->CopyFrom(hdr);

	// @todo: Move split to core
	for (int i=0;i<msg.payload_size();i++) {
		const Plugin::QueryRequestMessage_Request &payload = msg.payload(i);
		std::wstring command = utf8::cvt<std::wstring>(payload.command());
		NSCAPI::nagiosReturn tmp = process_single_query_request_payload(command, hdr, payload, response);
		ret = nscapi::plugin_helper::maxState(ret, tmp);
	}
	response.SerializeToString(&reply);
	return ret;
}

NSCAPI::nagiosReturn handler_impl::handle_submission_request(const std::string &request, Plugin::SubmitRequestMessage &msg, std::string &reply) {
	NSCAPI::nagiosReturn ret;
	Plugin::Common::Header hdr;
	hdr.CopyFrom(msg.header());

	Plugin::SubmitResponseMessage response;
	// @todo: swap data in the dhear (ie. sender /recipent)
	response.mutable_header()->CopyFrom(hdr);
	std::wstring channel = utf8::cvt<std::wstring>(msg.channel());

	// @todo: Move split to core
	for (int i=0;i<msg.payload_size();i++) {
		const Plugin::QueryResponseMessage_Response &payload = msg.payload(i);
		std::string outBuffer;
		NSCAPI::nagiosReturn tmp = process_single_submit_request_payload(channel, hdr, payload, response);
		ret = nscapi::plugin_helper::maxState(ret, tmp);
	}
	response.SerializeToString(&reply);
	return ret;
}
NSCAPI::nagiosReturn handler_impl::handle_exec_request(const std::string &request, Plugin::ExecuteRequestMessage &msg, std::string &reply) {
	NSCAPI::nagiosReturn ret;
	Plugin::Common::Header hdr;
	hdr.CopyFrom(msg.header());

	Plugin::ExecuteResponseMessage response;
	// @todo: swap data in the dhear (ie. sender /recipent)
	response.mutable_header()->CopyFrom(hdr);

	// @todo: Move split to core
	for (int i=0;i<msg.payload_size();i++) {
		const Plugin::ExecuteRequestMessage_Request &payload = msg.payload(i);
		std::wstring command = utf8::cvt<std::wstring>(payload.command());
		NSCAPI::nagiosReturn tmp = process_single_exec_request_payload(command, hdr, payload, response);
		ret = nscapi::plugin_helper::maxState(ret, tmp);
	}
	response.SerializeToString(&reply);
	return ret;
}


NSCAPI::nagiosReturn handler_impl::process_single_query_request_payload(std::wstring &command, const Plugin::Common::Header &hdr, const Plugin::QueryRequestMessage_Request &payload, Plugin::QueryResponseMessage &response) {
	NSCAPI::nagiosReturn ret = NSCAPI::returnOK;
	std::string outBuffer;
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
			// @todo: escalte ret here
		} else {
			nscapi::functions::create_simple_query_response_unknown(command, _T("Nasty arguments not allowed for command: ") + command, _T(""), outBuffer);
		}
	}
	Plugin::QueryResponseMessage tmpResponse;
	tmpResponse.ParseFromString(outBuffer);
	for (int i=0;i<tmpResponse.payload_size();i++) {
		response.add_payload()->CopyFrom(tmpResponse.payload(i));
	}
	return ret;
}


NSCAPI::nagiosReturn handler_impl::process_single_submit_request_payload(std::wstring &channel, const Plugin::Common::Header &hdr, const Plugin::QueryResponseMessage_Response &payload, Plugin::SubmitResponseMessage &response) {
	std::string outBuffer;
	std::string tmpBuffer;
	Plugin::SubmitRequestMessage tmp;
	tmp.mutable_header()->CopyFrom(hdr);
	tmp.add_payload()->CopyFrom(payload);
	tmp.SerializeToString(&tmpBuffer);
	NSCAPI::nagiosReturn returncode = nscapi::plugin_singleton->get_core()->submit_message(channel, tmpBuffer, outBuffer);
	// @todo fix this!!!
	/*
	if (returncode == NSCAPI::returnIgnored) {
		nscapi::functions::create_simple_query_response_unknown(command, _T("Command was not found: ") + command, _T(""), outBuffer);
	}
	*/
	Plugin::SubmitResponseMessage tmpResponse;
	tmpResponse.ParseFromString(outBuffer);
	for (int i=0;i<tmpResponse.payload_size();i++) {
		response.add_payload()->CopyFrom(tmpResponse.payload(i));
	}
	return returncode;
}



NSCAPI::nagiosReturn handler_impl::process_single_exec_request_payload(std::wstring &command, const Plugin::Common::Header &hdr, const Plugin::ExecuteRequestMessage_Request &payload, Plugin::ExecuteResponseMessage &response) {
	NSCAPI::nagiosReturn ret = NSCAPI::returnOK;
	std::string outBuffer;
	if (command.empty() || command == _T("_NSCP_CHECK")) {
		nscapi::functions::create_simple_exec_response<std::string>("_NSCP_CHECK", NSCAPI::returnOK, "I (" + utf8::cvt<std::string>(nscapi::plugin_singleton->get_core()->getApplicationVersionString()) + ") seem to be doing fine...", outBuffer);
	} else if (!allowArgs_ && payload.arguments_size() > 0) {
		nscapi::functions::create_simple_exec_response(command, NSCAPI::returnUNKNOWN, _T("Arguments not allowed for command: ") + command, outBuffer);
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
			Plugin::ExecuteResponseMessage tmp;
			tmp.mutable_header()->CopyFrom(hdr);
			tmp.add_payload()->CopyFrom(payload);
			tmp.SerializeToString(&tmpBuffer);
			NSCAPI::nagiosReturn returncode = nscapi::plugin_singleton->get_core()->exec_command(_T("*"), command, tmpBuffer, outBuffer);
			if (returncode == NSCAPI::returnIgnored) {
				nscapi::functions::create_simple_exec_response(command, returncode, _T("Command was not found: ") + command, outBuffer);
			}
			ret = nscapi::plugin_helper::maxState(ret, returncode);
		} else {
			nscapi::functions::create_simple_exec_response(command, NSCAPI::returnUNKNOWN, _T("Nasty arguments not allowed for command: ") + command, outBuffer);
		}
	}
	Plugin::ExecuteResponseMessage tmpResponse;
	tmpResponse.ParseFromString(outBuffer);
	for (int i=0;i<tmpResponse.payload_size();i++) {
		response.add_payload()->CopyFrom(tmpResponse.payload(i));
	}
	return ret;
}


