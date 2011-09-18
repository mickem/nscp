#include <nscp/handler.hpp>
#include <nscapi/functions.hpp>

std::list<nscp::packet> nscp::handler::process_all(const std::list<nscp::packet> &packets) {
	std::list<nscp::packet> result;
	BOOST_FOREACH(const nscp::packet &packet, packets) {
		result.push_back(process(packet));
	}
	return result;
}


nscp::packet nscp::handler::process(const nscp::packet &packet) {
	Plugin::Common::Header hdr;
	if (nscp::checks::is_query_request(packet)) {
		Plugin::QueryRequestMessage msg;
		msg.ParseFromString(packet.payload);
		std::wstring command = _T("todo: fixme");//utf8::cvt<std::wstring>(msg.command());
		hdr.CopyFrom(msg.header());
		std::string reply;
		try {
			NSCAPI::nagiosReturn returncode = handle_query_request(packet.payload, msg, reply);
			if (returncode == NSCAPI::returnIgnored)
				nscapi::functions::create_simple_query_response_unknown(command, _T("Command was not found: ") + command, _T(""), reply);
		} catch (const nscp::nscp_exception &e) {
			nscapi::functions::create_simple_query_response_unknown(command, _T("Processing error: ") + command + _T(": ") + utf8::cvt<std::wstring>(e.what()), _T(""), reply);
		} catch (const std::exception &e) {
			nscapi::functions::create_simple_query_response_unknown(command, _T("Unknown error processing: ") + command + _T(": ") + utf8::cvt<std::wstring>(e.what()), _T(""), reply);
		}
		return nscp::factory::create_query_response(reply);
	} else if (nscp::checks::is_query_response(packet)) {
		// @todo handle submission here
		return nscp::factory::create_error(_T("Submissions currently not supported: ") + packet.to_wstring());
	} else {
		this->log_error(__FILE__, __LINE__, _T("Unknown packet: ") + packet.to_wstring());
		return nscp::factory::create_error(_T("Unknown packet: ") + packet.to_wstring());
	}
}