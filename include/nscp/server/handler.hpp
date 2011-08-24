#pragma once

#include <nscp/packet.hpp>
#include <boost/tuple/tuple.hpp>

namespace nscp {
	namespace server {
		class handler {
		public:
			handler() {}
			handler(const handler &other) {}
			handler& operator= (const handler &other) {
				return *this;
			}
			virtual std::list<nscp::packet> process(nscp::packet &packet) = 0;

			virtual void log_debug(std::string file, int line, std::wstring msg) = 0;
			virtual void log_error(std::string file, int line, std::wstring msg) = 0;
			virtual nscp::packet create_error(std::wstring msg) = 0;

		};
	}// namespace server
} // namespace nrpe
