#pragma once

#include <nscapi/nscapi_protobuf.hpp>

#include <string>

namespace nsclient {
	namespace logging {

		struct logger_helper {
			static std::string get_formated_date(std::string format);
			static void log_fatal(std::string message);
			static std::pair<bool, std::string> render_console_message(const bool oneline, const std::string &data);
 			static std::string render_log_level_short(::Plugin::LogEntry::Entry::Level l);
 			static std::string render_log_level_long(::Plugin::LogEntry::Entry::Level l);
		};

	}
}