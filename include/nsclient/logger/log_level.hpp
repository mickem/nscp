/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
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