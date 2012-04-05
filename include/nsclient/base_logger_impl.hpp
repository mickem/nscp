#pragma once

#include <boost/noncopyable.hpp>
namespace nsclient {

	namespace logging {

		struct logger_helper {
			static std::string create(NSCAPI::log_level::level level, const char* file, const int line, std::wstring message);
			static std::wstring render_log_level_short(NSCAPI::log_level::level code);
			static std::wstring render_log_level_long(NSCAPI::log_level::level code);
			static std::string get_formated_date(std::string format);
		};

		class logging_interface_impl : public nsclient::logging::logger_interface, boost::noncopyable {
			NSCAPI::log_level::level level_;
			bool console_log_;
		public:
			logging_interface_impl() : level_(NSCAPI::log_level::info), console_log_(false) {}



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
			void debug(const char* file, const int line, std::wstring message) {
				if (should_log(NSCAPI::log_level::debug))
					do_log(logger_helper::create(NSCAPI::log_level::debug, file, line, message));
			}
			void info(const char* file, const int line, std::wstring message) {
				if (should_log(NSCAPI::log_level::info))
					do_log(logger_helper::create(NSCAPI::log_level::info, file, line, message));
			}
			void warning(const char* file, const int line, std::wstring message) {
				if (should_log(NSCAPI::log_level::warning))
					do_log(logger_helper::create(NSCAPI::log_level::warning, file, line, message));
			}
			void error(const char* file, const int line, std::wstring message) {
				if (should_log(NSCAPI::log_level::error))
					do_log(logger_helper::create(NSCAPI::log_level::error, file, line, message));
			}
			void fatal(const char* file, const int line, std::wstring message) {
				if (should_log(NSCAPI::log_level::critical))
					do_log(logger_helper::create(NSCAPI::log_level::critical, file, line, message));
			}
			void raw(const std::string &message) {
				do_log(message);
			}
			void log(NSCAPI::log_level::level level, const char* file, const int line, std::wstring message) {
				if (should_log(level))
					do_log(logger_helper::create(level, file, line, message));
			}

			virtual void do_log(const std::string &data) = 0;
			virtual void configure() = 0;
			virtual bool shutdown() = 0;
			virtual bool startup() = 0;
		};
		typedef boost::shared_ptr<nsclient::logging::logging_interface_impl> log_impl_type;
	}
}


