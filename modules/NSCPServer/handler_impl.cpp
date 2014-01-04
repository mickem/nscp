#include "stdafx.h"

#include <boost/asio.hpp>
#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/functions.hpp>

#include "handler_impl.hpp"



nscp::packet handler_impl::process(const nscp::packet &packet) {
	nscp::packet response;
	BOOST_FOREACH(const nscp::data::frame &frame, packet.frames_) {
		if (frame.header.type == nscp::data::frame_payload) {
			process_payload(response, frame);
		} else if (frame.header.type == nscp::data::frame_envelope) {
			process_header(response, frame);
		} else if (frame.header.type == nscp::data::frame_error) {
			process_error(response, frame);
		} else {
			this->log_error("nscp:handler", __FILE__, __LINE__, "Unknown packet: " + packet.to_string());
			return nscp::factory::create_error("Unknown packet: " + packet.to_string());
		}
	}
	return response;
}

void handler_impl::process_header(nscp::packet &response, const nscp::data::frame &frame) {

}
void handler_impl::process_error(nscp::packet &response, const nscp::data::frame &frame) {

}

void handler_impl::process_payload(nscp::packet &response, const nscp::data::frame &frame) {
	std::string reply;
	NSCPIPC::PayloadMessage reply_message;
	try {
		NSCPIPC::PayloadMessage request_message;
		request_message.ParseFromString(frame.payload);
		if (request_message.type() == NSCPIPC::Common::QUERY_REQUEST) {
			NSCAPI::nagiosReturn returncode = nscapi::plugin_singleton->get_core()->query(request_message.message(), reply);
			if (returncode == NSCAPI::returnIgnored)
				nscapi::protobuf::functions::create_simple_query_response_unknown("UNKNOWN", "Command was not found", reply);
			reply_message.set_type(NSCPIPC::Common::QUERY_RESPONSE);
			reply_message.set_message(reply);
		} else if (request_message.type() == NSCPIPC::Common::SETTINGS_REQUEST) {
			nscapi::plugin_singleton->get_core()->settings_query(request_message.message(), reply);
			reply_message.set_type(NSCPIPC::Common::SETTINGS_RESPONSE);
			reply_message.set_message(reply);
		} else if (request_message.type() == NSCPIPC::Common::REGISTRY_REQUEST) {
			nscapi::plugin_singleton->get_core()->registry_query(request_message.message(), reply);
			reply_message.set_type(NSCPIPC::Common::REGISTRY_RESPONSE);
			reply_message.set_message(reply);
	// 	} else if (nscp::checks::is_submit_request(packet)) {
	// 		Plugin::SubmitRequestMessage msg;
	// 		msg.ParseFromString(packet.payload);
	// 		try {
	// 			std::string reply;
	// 			handle_submission_request(packet.payload, msg, reply);
	// 			return nscp::factory::create_submission_response(reply);
	// 		} catch (const nscp::nscp_exception &e) {
	// 			return nscp::factory::create_error(_T("Exception processing message: ") + to_wstring(e.what()));
	// 		} catch (const std::exception &e) {
	// 			return nscp::factory::create_error(_T("Exception processing message: ") + to_wstring(e.what()));
	// 		}
	// 	} else if (nscp::checks::is_exec_request(packet)) {
	// 		Plugin::ExecuteRequestMessage msg;
	// 		msg.ParseFromString(packet.payload);
	// 		try {
	// 			std::string reply;
	// 			handle_exec_request(packet.payload, msg, reply);
	// 			return nscp::factory::create_submission_response(reply);
	// 		} catch (const nscp::nscp_exception &e) {
	// 			return nscp::factory::create_error(_T("Exception processing message: ") + to_wstring(e.what()));
	// 		} catch (const std::exception &e) {
	// 			return nscp::factory::create_error(_T("Exception processing message: ") + to_wstring(e.what()));
	// 		}
		} else {
			this->log_error("nscp:handler", __FILE__, __LINE__, "Unknown packet: " + frame.to_string());
			reply_message.set_type(NSCPIPC::Common::ERROR_MESSAGE);
			reply_message.set_message("TODO");
		}
	} catch (const std::exception &e) {
		this->log_error("nscp:handler", __FILE__, __LINE__, utf8::utf8_from_native(e.what()));
		reply_message.set_type(NSCPIPC::Common::ERROR_MESSAGE);
		reply_message.set_message("TODO");
	} catch (...) {
		this->log_error("nscp:handler", __FILE__, __LINE__, "Unknown exception");
		reply_message.set_type(NSCPIPC::Common::ERROR_MESSAGE);
		reply_message.set_message("TODO");
	}
		response.add_payload(reply_message.SerializeAsString());
}




