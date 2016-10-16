#pragma once

#include <nsclient/logger/base_logger_impl.hpp>

#include <vector>
#include <string>

namespace nsclient {
	namespace logging {
		namespace impl {
			class simple_console_logger : public nsclient::logging::log_driver_interface_impl {
				std::string format_;
				std::vector<char> buf_;
			public:
				simple_console_logger();
				void do_log(const std::string data);
				struct config_data {
					std::string format;
				};
				config_data do_config();
				void synch_configure();
				void asynch_configure();
			};
		}
	}
}