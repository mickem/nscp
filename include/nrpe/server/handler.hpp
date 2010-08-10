#pragma once

#include <nrpe/packet.hpp>
#include <boost/tuple/tuple.hpp>

namespace nrpe {
	namespace server {
		class handler {
		public:
			handler() {}
			handler(const handler &other) {}
			handler& operator= (const handler &other) {
				return *this;
			}
			virtual nrpe::packet handle(nrpe::packet packet) = 0;
			virtual void log_debug(std::wstring file, int line, std::wstring msg) = 0;
			virtual void log_error(std::wstring file, int line, std::wstring msg) = 0;
			virtual nrpe::packet create_error(std::wstring msg) = 0;
			virtual unsigned int get_payload_length() = 0;
			virtual void set_payload_length(unsigned int payload) = 0;

		};
	}// namespace server
} // namespace nrpe
