#pragma once

#include <boost/noncopyable.hpp>
namespace nsclient {

	namespace logging {

		struct logger_helper {
			static std::string create(const std::wstring &module, NSCAPI::log_level::level level, const char* file, const int line, const std::wstring &message);
			static std::wstring render_log_level_short(NSCAPI::log_level::level code);
			static std::wstring render_log_level_long(NSCAPI::log_level::level code);
			static std::string get_formated_date(std::string format);
		};

		class logging_interface_impl : public nsclient::logging::logger_interface, boost::noncopyable {
			NSCAPI::log_level::level level_;
			bool console_log_;
			bool is_running_;
		public:
			logging_interface_impl() : level_(NSCAPI::log_level::info), console_log_(false), is_running_(false) {}
			virtual ~logging_interface_impl() {}



			bool should_log(NSCAPI::log_level::level level) const {
				return level <= level_;
			}
			NSCAPI::log_level::level get_log_level() const {
				return level_;
			}
			virtual void set_log_level(NSCAPI::log_level::level level) {
				level_ = level;
			}
			virtual void set_console_log(bool console_log) {
				console_log_ = console_log;
			}
			bool get_console_log() const {
				return console_log_;
			}
			void debug(const std::wstring &module, const char* file, const int line, const std::wstring &message) {
				if (should_log(NSCAPI::log_level::debug))
					do_log(logger_helper::create(module, NSCAPI::log_level::debug, file, line, message));
			}
			void info(const std::wstring &module, const char* file, const int line, const std::wstring &message) {
				if (should_log(NSCAPI::log_level::info))
					do_log(logger_helper::create(module, NSCAPI::log_level::info, file, line, message));
			}
			void warning(const std::wstring &module, const char* file, const int line, const std::wstring &message) {
				if (should_log(NSCAPI::log_level::warning))
					do_log(logger_helper::create(module, NSCAPI::log_level::warning, file, line, message));
			}
			void error(const std::wstring &module, const char* file, const int line, const std::wstring &message) {
				if (should_log(NSCAPI::log_level::error))
					do_log(logger_helper::create(module, NSCAPI::log_level::error, file, line, message));
			}
			void fatal(const std::wstring &module, const char* file, const int line, const std::wstring &message) {
				if (should_log(NSCAPI::log_level::critical))
					do_log(logger_helper::create(module, NSCAPI::log_level::critical, file, line, message));
			}
			void raw(const std::string &message) {
				do_log(message);
			}
			void log(const std::wstring &module, NSCAPI::log_level::level level, const char* file, const int line, const std::wstring &message) {
				if (should_log(level))
					do_log(logger_helper::create(module, level, file, line, message));
			}

			virtual void do_log(const std::string &data) = 0;
			virtual void configure() = 0;
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
			
		};
		typedef boost::shared_ptr<nsclient::logging::logging_interface_impl> log_impl_type;
	}
}


