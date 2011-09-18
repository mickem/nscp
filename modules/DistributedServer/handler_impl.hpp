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
		GET_CORE()->Message(NSCAPI::debug, file, line, msg);
	}
	void log_error(std::string file, int line, std::wstring msg) {
		GET_CORE()->Message(NSCAPI::error, file, line, msg);
	}
};
