#pragma once

#include <string>

namespace nsclient {
	namespace logging {


		class log_level {

			static const int off		=  0;	// Used to disable logging
			static const int critical	=  1;	// Critical error
			static const int error		=  2;	// Error
			static const int warning	=  3;	// Warning
			static const int info		= 10;	// information
			static const int debug		= 50;	// Debug messages
			static const int trace		= 99;	// Trace messages

			int current_level_;

		public:

			log_level() : current_level_(info) {}
			
			bool should_trace() const {
				return current_level_ >= trace;
			}
			bool should_debug() const {
				return current_level_ >= debug;
			}
			bool should_info() const {
				return current_level_ >= info;
			}
			bool should_warning() const {
				return current_level_ >= warning;
			}
			bool should_error() const {
				return current_level_ >= error;
			}
			bool should_critical() const {
				return current_level_ >= critical;
			}

			bool set(const std::string level);

			std::string get() const;

		};

	}
}