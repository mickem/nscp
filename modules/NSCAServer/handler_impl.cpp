#include "stdafx.h"

#include <boost/asio.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include "handler_impl.hpp"

void nsca_handler_impl::handle(nsca::packet p) {
	std::string response;
	std::string::size_type pos = p.result.find('|');
	if (pos != std::string::npos) {
		std::string msg = p.result.substr(0, pos);
		std::string perf = p.result.substr(++pos);
		nscapi::core_helper::submit_simple_message(channel_, p.service, nscapi::plugin_helper::int2nagios(p.code), msg, perf, response);
	} else {
		std::string empty, msg = p.result;
		nscapi::core_helper::submit_simple_message(channel_, p.service, nscapi::plugin_helper::int2nagios(p.code), msg, empty, response);

	}
}




