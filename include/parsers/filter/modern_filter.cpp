#include <parsers/filter/modern_filter.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

namespace modern_filter {

	bool error_handler_impl::has_errors() const {
		return !error.empty();
	}
	void error_handler_impl::log_error(const std::string error_) {
		NSC_DEBUG_MSG_STD(error_);
		error = error_;
	}
	void error_handler_impl::log_warning(const std::string error) {
		NSC_DEBUG_MSG_STD(error);
	}
	void error_handler_impl::log_debug(const std::string error) {
		NSC_DEBUG_MSG_STD(error);
	}
	std::string error_handler_impl::get_errors() const {
		return error;
	}

	bool error_handler_impl::is_debug() const {
		return debug;
	}
	void error_handler_impl::set_debug(bool debug_) {
		debug = debug_;
	}
}
