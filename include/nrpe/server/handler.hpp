#pragma once

#include <nrpe/packet.hpp>
#include <boost/tuple/tuple.hpp>

namespace nrpe {
	namespace server {
		class handler : boost::noncopyable {
		public:
			virtual std::list<nrpe::packet> handle(nrpe::packet packet) = 0;
			virtual void log_debug(std::string module, std::string file, int line, std::string msg) const = 0;
			virtual void log_error(std::string module, std::string file, int line, std::string msg) const = 0;
			virtual nrpe::packet create_error(std::string msg) = 0;
			virtual unsigned int get_payload_length() = 0;
			virtual void set_payload_length(unsigned int payload) = 0;
			virtual void set_allow_arguments(bool) = 0;
			virtual void set_allow_nasty_arguments(bool) = 0;
			virtual void set_perf_data(bool) = 0;
			virtual void set_multiple_packets(bool v) = 0;

		};
	}// namespace server
} // namespace nrpe
