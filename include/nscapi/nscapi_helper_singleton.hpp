#pragma once

#include <NSCAPI.h>
#include <nscapi/nscapi_core_wrapper.hpp>

namespace nscapi {

	class helper_singleton {
		core_wrapper* core_;
	public:
		helper_singleton();
		core_wrapper* get_core() const {
			return core_;
		}
	};

	extern helper_singleton* plugin_singleton;
}