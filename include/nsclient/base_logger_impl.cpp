#include <nsclient/logger.hpp>
#include <nsclient/base_logger_impl.hpp>
#include <boost/date_time.hpp>

std::string nsclient::logging::logger_helper::render_log_level_short(NSCAPI::log_level::level code) {
	switch (code) {
		case NSCAPI::log_level::critical:
			return "C";
		case NSCAPI::log_level::warning:
			return "W";
		case NSCAPI::log_level::error:
			return "E";
		case NSCAPI::log_level::info:
			return "L";
		case NSCAPI::log_level::debug:
			return "D";
		case NSCAPI::log_level::trace:
			return "T";
		default:
			return "?";
	}
}

std::string nsclient::logging::logger_helper::render_log_level_long(NSCAPI::log_level::level code) {
	switch (code) {
		case NSCAPI::log_level::error:
			return "error";
		case NSCAPI::log_level::critical:
			return "critical";
		case NSCAPI::log_level::warning:
			return "warning";
		case NSCAPI::log_level::info:
			return "message";
		case NSCAPI::log_level::debug:
			return "debug";
		case NSCAPI::log_level::trace:
			return "trace";
	}
	return "unknown";
}

std::string nsclient::logging::logger_helper::get_formated_date(std::string format) {
	std::stringstream ss;
	boost::posix_time::time_facet *facet = new boost::posix_time::time_facet(format.c_str());
	ss.imbue(std::locale(std::cout.getloc(), facet));
	ss << boost::posix_time::second_clock::local_time();
	return ss.str();
}
