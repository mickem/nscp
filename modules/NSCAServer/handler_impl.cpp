#include "stdafx.h"

#include <boost/asio.hpp>
#include "handler_impl.hpp"
#include <config.h>

void handler_impl::handle(nsca::packet p) {
	std::wstring response;
	std::string::size_type pos = p.result.find('|');
	if (pos != std::string::npos) {
		std::wstring msg = utf8::cvt<std::wstring>(p.result.substr(0, pos));
		std::wstring perf = utf8::cvt<std::wstring>(p.result.substr(++pos));
		GET_CORE()->submit_simple_message(channel_, utf8::cvt<std::wstring>(p.service), p.code, msg, perf, response);
	} else {
		std::wstring empty;
		GET_CORE()->submit_simple_message(channel_, utf8::cvt<std::wstring>(p.service), p.code, utf8::cvt<std::wstring>(p.result), empty, response);

	}
	std::wstring perf = _T(""); // @todo fix this!

	NSC_DEBUG_MSG(_T("Got response: ") + response);
}




