#include "stdafx.h"

#include <boost/asio.hpp>
#include "handler_impl.hpp"
#include <config.h>

void handler_impl::handle(nsca::packet p) {
	std::wstring response;
	std::wstring msg = utf8::cvt<std::wstring>(p.result);
	std::wstring perf = _T(""); // @todo fix this!

	GET_CORE()->submit_simple_message(channel_, utf8::cvt<std::wstring>(p.service), p.code, msg, perf, response);
	NSC_DEBUG_MSG(_T("Got response: ") + response);
}




