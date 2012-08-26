#pragma once

#include <check_mk/data.hpp>
#include <boost/tuple/tuple.hpp>

namespace check_mk {
	namespace server {
		class handler : public boost::noncopyable {
		public:
			virtual check_mk::packet process() = 0;

			virtual void log_debug(std::string module, std::string file, int line, std::string msg) const = 0;
			virtual void log_error(std::string module, std::string file, int line, std::string msg) const = 0;
		};
	}// namespace server
} // namespace check_mk
