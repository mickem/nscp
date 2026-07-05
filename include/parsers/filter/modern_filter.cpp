// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <parsers/filter/modern_filter.hpp>

namespace modern_filter {
bool error_handler_impl::has_errors() const { return !error.empty(); }
void error_handler_impl::log_error(const std::string message) {
  NSC_DEBUG_MSG_STD(message);
  error = message;
}
void error_handler_impl::log_warning(const std::string message) { NSC_DEBUG_MSG_STD(message); }
void error_handler_impl::log_debug(const std::string message) { NSC_DEBUG_MSG_STD(message); }
std::string error_handler_impl::get_errors() const { return error; }

bool error_handler_impl::is_debug() const { return debug_; }
void error_handler_impl::set_debug(bool debug) { debug_ = debug; }
}  // namespace modern_filter