#pragma once

#include <check_nt/packet.hpp>
#include <boost/tuple/tuple.hpp>

namespace check_nt {
	namespace server {
		class handler : public boost::noncopyable {
		public:
			virtual check_nt::packet handle(check_nt::packet packet) = 0;
			virtual void log_debug(std::string module, std::string file, int line, std::string msg) const = 0;
			virtual void log_error(std::string module, std::string file, int line, std::string msg) const = 0;
			virtual check_nt::packet create_error(std::wstring msg) = 0;

			virtual void set_allow_arguments(bool) = 0;
			virtual void set_allow_nasty_arguments(bool) = 0;
			virtual void set_perf_data(bool) = 0;

			virtual void set_password(std::wstring password) = 0;
			virtual std::wstring get_password() const = 0;

		};
	}// namespace server
} // namespace check_nt
