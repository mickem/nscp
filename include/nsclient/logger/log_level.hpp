/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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