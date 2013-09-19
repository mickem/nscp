#pragma once

#include <boost/tuple/tuple.hpp>

#include <nscapi/nscapi_plugin_interface.hpp>

#include <nscp/packet.hpp>
#include <nscp/server/handler.hpp>

class handler_impl : public nscp::server::handler {
	bool allowArgs_;
public:
	handler_impl() : allowArgs_(false) {}

	NSCAPI::nagiosReturn handle_submission_request(const std::string &,Plugin::SubmitRequestMessage &,std::string &) {return 0;}
	NSCAPI::nagiosReturn handle_exec_request(const std::string &,Plugin::ExecuteRequestMessage &,std::string &) {return 0;}

	nscp::packet create_error(std::string msg) {
		return nscp::factory::create_error(msg);
	}

	virtual void set_allow_arguments(bool v)  {
		allowArgs_ = v;
	}

	virtual nscp::packet process(const nscp::packet &packet);
	void process_payload(nscp::packet &response, const nscp::data::frame &frame);
	void process_header(nscp::packet &response, const nscp::data::frame &frame);
	void process_error(nscp::packet &response, const nscp::data::frame &frame);

	virtual void log_debug(std::string module, std::string file, int line, std::string msg) const {
		if (GET_CORE()->should_log(NSCAPI::log_level::debug)) {
			GET_CORE()->log(NSCAPI::log_level::debug, file, line, msg);
		}
	}
	virtual void log_error(std::string module, std::string file, int line, std::string msg) const {
		if (GET_CORE()->should_log(NSCAPI::log_level::error)) {
			GET_CORE()->log(NSCAPI::log_level::error, file, line, msg);
		}
	}
};
