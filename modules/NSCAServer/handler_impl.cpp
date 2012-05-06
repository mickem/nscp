#include "stdafx.h"

#include <boost/asio.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include "handler_impl.hpp"
#include <config.h>

void nsca_handler_impl::handle(nsca::packet p) {
	std::wstring response;
	std::string::size_type pos = p.result.find('|');
	if (pos != std::string::npos) {
		std::wstring msg = utf8::cvt<std::wstring>(p.result.substr(0, pos));
		std::wstring perf = utf8::cvt<std::wstring>(p.result.substr(++pos));
		nscapi::core_helper::submit_simple_message(channel_, utf8::cvt<std::wstring>(p.service), nscapi::plugin_helper::int2nagios(p.code), msg, perf, response);
	} else {
		std::wstring empty, msg = utf8::cvt<std::wstring>(p.result);
		nscapi::core_helper::submit_simple_message(channel_, utf8::cvt<std::wstring>(p.service), nscapi::plugin_helper::int2nagios(p.code), msg, empty, response);

	}
	NSC_DEBUG_MSG(_T("Got response: ") + response);
}




