#pragma once

#include <nsca/nsca_packet.hpp>

namespace nsca {
	namespace server {
		class handler : boost::noncopyable {
		public:
			virtual void handle(nsca::packet packet) = 0;
			virtual void log_debug(std::string module, std::string file, int line, std::string msg) const = 0;
			virtual void log_error(std::string module, std::string file, int line, std::string msg) const = 0;
			virtual unsigned int get_payload_length() = 0;
			virtual void set_payload_length(unsigned int payload) = 0;
			virtual void set_perf_data(bool) = 0;
			virtual void set_channel(std::wstring channel) = 0;
			virtual std::wstring get_channel() = 0;
			virtual void set_encryption(std::string encryption) = 0;
			virtual int get_encryption() = 0;
			virtual std::string get_password() = 0;
			virtual void set_password(std::string pwd) = 0;
		};
	}// namespace server
} // namespace nsca
