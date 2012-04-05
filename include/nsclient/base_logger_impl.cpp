#include <nsclient/logger.hpp>
#include <nsclient/base_logger_impl.hpp>
#include <boost/date_time.hpp>

std::wstring nsclient::logging::logger_helper::render_log_level_short(NSCAPI::log_level::level code) {
	switch (code) {
		case NSCAPI::log_level::critical:
			return _T("c");
		case NSCAPI::log_level::warning:
			return _T("w");
		case NSCAPI::log_level::error:
			return _T("e");
		case NSCAPI::log_level::info:
			return _T("l");
		case NSCAPI::log_level::debug:
			return _T("d");
		default:
			return _T("?");
	}
}

std::wstring nsclient::logging::logger_helper::render_log_level_long(NSCAPI::log_level::level code) {
	switch (code) {
		case NSCAPI::log_level::error:
			return _T("error");
		case NSCAPI::log_level::critical:
			return _T("critical");
		case NSCAPI::log_level::warning:
			return _T("warning");
		case NSCAPI::log_level::info:
			return _T("message");
		case NSCAPI::log_level::debug:
			return _T("debug");
	}
	return _T("unknown");
}

std::string nsclient::logging::logger_helper::get_formated_date(std::string format) {
	std::stringstream ss;
	boost::posix_time::time_facet *facet = new boost::posix_time::time_facet(format.c_str());
	ss.imbue(std::locale(std::cout.getloc(), facet));
	ss << boost::posix_time::second_clock::local_time();
	return ss.str();
}
