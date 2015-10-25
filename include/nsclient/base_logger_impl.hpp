#pragma once

#include <boost/noncopyable.hpp>

#include <nsclient/logger.hpp>

namespace nsclient {

	namespace logging {

		struct logger_helper {
			static std::string create(const std::string &module, NSCAPI::log_level::level level, const char* file, const int line, const std::string &message);
			static std::string render_log_level_short(NSCAPI::log_level::level code);
			static std::string render_log_level_long(NSCAPI::log_level::level code);
			static std::string get_formated_date(std::string format);
		};

		class logging_interface_impl : public nsclient::logging::logger_interface, boost::noncopyable {
			NSCAPI::log_level::level level_;
			bool console_log_;
			bool oneline_;
			bool is_running_;
			bool no_std_err_;
		public:
			logging_interface_impl() : level_(NSCAPI::log_level::info), console_log_(false), oneline_(false), is_running_(false), no_std_err_(false) {}
			virtual ~logging_interface_impl() {}

			bool should_log(NSCAPI::log_level::level level) const {
				return level <= level_;
			}
			NSCAPI::log_level::level get_log_level() const {
				return level_;
			}
			virtual void set_log_level(const NSCAPI::log_level::level level) {
				level_ = level;
			}
			virtual void set_config(const std::string &key) {
				if (key == "console")
					console_log_ = true;
				else if (key == "oneline")
					oneline_ = true;
				else if (key == "no-std-err")
					no_std_err_ = true;
				else
					do_log(logger_helper::create("logger", NSCAPI::log_level::error, __FILE__, __LINE__, "Invalid key: " + key));
			}
			void debug(const std::string &module, const char* file, const int line, const std::string &message) {
				if (should_log(NSCAPI::log_level::debug))
					do_log(logger_helper::create(module, NSCAPI::log_level::debug, file, line, message));
			}
			void trace(const std::string &module, const char* file, const int line, const std::string &message) {
				if (should_log(NSCAPI::log_level::trace))
					do_log(logger_helper::create(module, NSCAPI::log_level::trace, file, line, message));
			}
			void info(const std::string &module, const char* file, const int line, const std::string &message) {
				if (should_log(NSCAPI::log_level::info))
					do_log(logger_helper::create(module, NSCAPI::log_level::info, file, line, message));
			}
			void warning(const std::string &module, const char* file, const int line, const std::string &message) {
				if (should_log(NSCAPI::log_level::warning))
					do_log(logger_helper::create(module, NSCAPI::log_level::warning, file, line, message));
			}
			void error(const std::string &module, const char* file, const int line, const std::string &message) {
				if (should_log(NSCAPI::log_level::error))
					do_log(logger_helper::create(module, NSCAPI::log_level::error, file, line, message));
			}
			void fatal(const std::string &module, const char* file, const int line, const std::string &message) {
				if (should_log(NSCAPI::log_level::critical))
					do_log(logger_helper::create(module, NSCAPI::log_level::critical, file, line, message));
			}			void raw(const std::string &message) {
				do_log(message);
			}
			void log(const std::string &module, NSCAPI::log_level::level level, const char* file, const int line, const std::string &message) {
				if (should_log(level))
					do_log(logger_helper::create(module, level, file, line, message));
			}

			virtual void do_log(const std::string data) = 0;
			virtual void synch_configure() = 0;
			virtual void asynch_configure() = 0;
			virtual bool shutdown() {
				is_running_ = false;
				return true;
			}
			virtual bool startup() {
				is_running_ = true;
				return true;
			}
			bool is_started() const {
				return is_running_;
			}

		public:
			bool is_console() const {
				return console_log_;
			}
			bool is_oneline() const {
				return oneline_;
			}
			bool is_no_std_err() const {
				return no_std_err_;
			}
			
		};
		typedef boost::shared_ptr<nsclient::logging::logging_interface_impl> log_impl_type;
	}
}


