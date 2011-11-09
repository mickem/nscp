#pragma once

#include <nscp/packet.hpp>
#include <nscp/handler.hpp>

class handler_impl : public nscp::handler, private boost::noncopyable {
	bool allowArgs_;
	bool allowNasty_;
	bool noPerfData_;
public:
	handler_impl() : noPerfData_(false), allowNasty_(false), allowArgs_(false) {}

	NSCAPI::nagiosReturn handle_query_request(const std::string &request, Plugin::QueryRequestMessage &msg, std::string &reply);
	NSCAPI::nagiosReturn handle_submission_request(const std::string &request, Plugin::SubmitRequestMessage &msg, std::string &reply);
	NSCAPI::nagiosReturn handle_exec_request(const std::string &request, Plugin::ExecuteRequestMessage &msg, std::string &reply);

	NSCAPI::nagiosReturn process_single_query_request_payload(std::wstring &command, const Plugin::Common::Header &hdr, const Plugin::QueryRequestMessage_Request &payload, Plugin::QueryResponseMessage &response);
	NSCAPI::nagiosReturn process_single_submit_request_payload(std::wstring &channel, const Plugin::Common::Header &hdr, const Plugin::QueryResponseMessage_Response &payload, Plugin::SubmitResponseMessage &response);
	NSCAPI::nagiosReturn process_single_exec_request_payload(std::wstring &command, const Plugin::Common::Header &hdr, const Plugin::ExecuteRequestMessage_Request &payload, Plugin::ExecuteResponseMessage &response);

	nscp::packet create_error(std::wstring msg) {
		return nscp::factory::create_error(msg);
	}

	virtual void set_allow_arguments(bool v)  {
		allowArgs_ = v;
	}
	virtual void set_allow_nasty_arguments(bool v) {
		allowNasty_ = v;
	}
	virtual void set_perf_data(bool v) {
		noPerfData_ = !v;
		if (noPerfData_)
			log_debug(__FILE__, __LINE__, _T("Performance data disabled!"));
	}

	void log_debug(std::string file, int line, std::wstring msg) {
		GET_CORE()->log(NSCAPI::debug, file, line, msg);
	}
	void log_error(std::string file, int line, std::wstring msg) {
		GET_CORE()->log(NSCAPI::error, file, line, msg);
	}
};
