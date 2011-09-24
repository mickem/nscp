#pragma once

#include <nsca/nsca_packet.hpp>

namespace nsca {
	namespace server {
		class handler {
		public:
			handler() {}
			handler(const handler &other) {}
			handler& operator= (const handler &other) {
				return *this;
			}
			virtual void handle(nsca::packet packet) = 0;
			virtual void log_debug(std::string file, int line, std::wstring msg) = 0;
			virtual void log_error(std::string file, int line, std::wstring msg) = 0;
			virtual unsigned int get_payload_length() = 0;
			virtual void set_payload_length(unsigned int payload) = 0;
			virtual void set_perf_data(bool) = 0;
			virtual void set_channel(std::wstring channel) = 0;
			virtual std::wstring get_channel() = 0;

		};
	}// namespace server
} // namespace nsca
