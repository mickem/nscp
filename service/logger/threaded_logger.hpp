#pragma once

#include <nsclient/logger/base_logger_impl.hpp>

#include <concurrent_queue.hpp>

#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>

#include <string>


namespace nsclient {
	namespace logging {
		namespace impl {
			class threaded_logger : public nsclient::logging::log_driver_interface_impl {
				concurrent_queue<std::string> log_queue_;
				boost::thread thread_;

				log_driver_instance background_logger_;
				logging_subscriber *subscriber_manager_;

			public:

				threaded_logger(logging_subscriber *subscriber_manager, log_driver_instance background_logger);
				virtual ~threaded_logger();

				virtual void do_log(const std::string data);
				void push(const std::string &data);

				void thread_proc();

				virtual void asynch_configure();
				virtual void synch_configure();
				virtual bool startup();
				virtual bool shutdown();

				//virtual void set_log_level(NSCAPI::log_level::level level);
				virtual void set_config(const std::string &key);
			};
		}
	}
}