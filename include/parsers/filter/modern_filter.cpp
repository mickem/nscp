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

#include <parsers/filter/modern_filter.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

namespace modern_filter {
	bool error_handler_impl::has_errors() const {
		return !error.empty();
	}
	void error_handler_impl::log_error(const std::string message) {
		NSC_DEBUG_MSG_STD(message);
		error = message;
	}
	void error_handler_impl::log_warning(const std::string message) {
		NSC_DEBUG_MSG_STD(message);
	}
	void error_handler_impl::log_debug(const std::string message) {
		NSC_DEBUG_MSG_STD(message);
	}
	std::string error_handler_impl::get_errors() const {
		return error;
	}

	bool error_handler_impl::is_debug() const {
		return debug_;
	}
	void error_handler_impl::set_debug(bool debug) {
		debug_ = debug;
	}
}