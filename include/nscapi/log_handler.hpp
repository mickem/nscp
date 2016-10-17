#pragma once

#include <string>

namespace nscapi {

	struct log_handler {
		virtual void log_debug(std::string module, std::string file, int line, std::string msg) const = 0;
		virtual void log_error(std::string module, std::string file, int line, std::string msg) const = 0;
	};
}