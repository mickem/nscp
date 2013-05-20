#pragma once

#include <nscp/packet.hpp>
#include <boost/tuple/tuple.hpp>

namespace nscp {
	namespace server {
		class handler : public boost::noncopyable {
		public:
			virtual nscp::packet process(const nscp::packet &packet) = 0;

			virtual void log_debug(std::string module, std::string file, int line, std::string msg) const = 0;
			virtual void log_error(std::string module, std::string file, int line, std::string msg) const = 0;
			virtual nscp::packet create_error(std::string msg) = 0;


		};
	}// namespace server
} // namespace nscp
