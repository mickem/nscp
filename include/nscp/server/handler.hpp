#pragma once

#include <nscp/packet.hpp>
#include <boost/tuple/tuple.hpp>

namespace nscp {
	namespace server {
		class server_handler {
		private:
			server_handler(const server_handler &other) {}
			server_handler& operator= (const server_handler &other) {
				return *this;
			}
		public:
			server_handler() {}
			virtual nscp::packet process(const nscp::packet &packet) = 0;
			virtual std::list<nscp::packet> process_all(const std::list<nscp::packet> &packet) = 0;

			virtual void log_debug(std::string file, int line, std::wstring msg) = 0;
			virtual void log_error(std::string file, int line, std::wstring msg) = 0;
			virtual nscp::packet create_error(std::wstring msg) = 0;

		};
	}// namespace server
} // namespace nrpe
