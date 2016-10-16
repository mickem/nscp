#pragma once

#include <nsclient/logger/base_logger_impl.hpp>

#include <string>

namespace nsclient {
	namespace logging {
		namespace impl {
			class simple_file_logger : public nsclient::logging::log_driver_interface_impl {
				std::string file_;
				std::size_t max_size_;
				std::string format_;

			public:
				simple_file_logger(std::string file);
				std::string base_path();

				void do_log(const std::string data);
				struct config_data {
					std::string file;
					std::string format;
					std::size_t max_size;
				};
				config_data do_config(const bool log_fault);
				void synch_configure();
				void asynch_configure();
				bool shutdown() { return true;  }
			};

		}
	}
}