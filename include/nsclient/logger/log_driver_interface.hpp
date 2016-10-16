#pragma once

#include <nsclient/logger/logger.hpp>

#include <boost/shared_ptr.hpp>

#include <string>

namespace nsclient {
	namespace logging {


		struct log_driver_interface {

			log_driver_interface() {}
			virtual ~log_driver_interface() {}
			virtual void do_log(const std::string data) = 0;
			virtual void synch_configure() = 0;
			virtual void asynch_configure() = 0;

			virtual void set_config(const std::string &key) = 0;
			virtual void set_config(const boost::shared_ptr<log_driver_interface> other) = 0;
			virtual bool shutdown() = 0;
			virtual bool startup() = 0;
			virtual bool is_console() const = 0;
			virtual bool is_oneline() const = 0;
			virtual bool is_no_std_err() const = 0;
			virtual bool is_started() const = 0;

		};

		typedef boost::shared_ptr<log_driver_interface> log_driver_instance;

	}
}