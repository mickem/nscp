#pragma once

#include <NSCAPI.h>
#include <nscp/packet.hpp>
#include <nscp/server/handler.hpp>

namespace nscp {

	struct handler : public nscp::server::server_handler {
		virtual nscp::packet process(const nscp::packet &packet);
		virtual std::list<nscp::packet> process_all(const std::list<nscp::packet> &packet);

		virtual NSCAPI::nagiosReturn handle_query_request(const std::string &request, Plugin::QueryRequestMessage &msg, std::string &reply) = 0;
		virtual NSCAPI::nagiosReturn handle_submission_request(const std::string &request, Plugin::SubmitRequestMessage &msg, std::string &reply) = 0;
		virtual NSCAPI::nagiosReturn handle_exec_request(const std::string &request, Plugin::ExecuteRequestMessage &msg, std::string &reply) = 0;
	};


}

