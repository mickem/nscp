#pragma once

#include <NSCAPI.h>
#include <boost/shared_ptr.hpp>

namespace nsclient {
	namespace logging {
		class raw_subscriber {
		public:
			virtual void on_raw_log_message(std::string &payload) = 0;
		};
		class logger_interface {
		public:
			virtual void raw(const std::string &message) = 0;

			virtual void debug(const std::string &module, const char* file, const int line, const std::string &message) = 0;
			virtual void info(const std::string &module, const char* file, const int line, const std::string &message) = 0;
			virtual void warning(const std::string &module, const char* file, const int line, const std::string &message) = 0;
			virtual void error(const std::string &module, const char* file, const int line, const std::string &message) = 0;
			virtual void fatal(const std::string &module, const char* file, const int line, const std::string &message) = 0;

			virtual void log(const std::string &module, NSCAPI::log_level::level level, const char* file, const int line, const std::string &message) = 0;

			virtual bool should_log(NSCAPI::log_level::level level) const = 0;
			virtual NSCAPI::log_level::level get_log_level() const = 0;
			virtual void set_log_level(NSCAPI::log_level::level level) = 0;

			virtual void set_console_log(bool value) = 0;
			virtual bool get_console_log() const = 0;
		};
		class logger {
		public:
			typedef boost::shared_ptr<raw_subscriber> raw_subscriber_type;
			static logger_interface* get_logger();
			static void subscribe_raw(raw_subscriber_type subscriber);
			static void clear_subscribers();
			static bool startup();
			static bool shutdown();
			static void configure();

			static void set_log_level(NSCAPI::log_level::level level);
			static void set_log_level(std::string level);

			static void set_backend(std::string backend);
			static void destroy();

		};
	}
}