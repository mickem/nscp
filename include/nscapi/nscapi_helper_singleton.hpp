#pragma once

#include <NSCAPI.h>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/dll_defines.hpp>

namespace nscapi {
	class NSCAPI_EXPORT helper_singleton {
		core_wrapper* core_;
	public:
		helper_singleton();
		core_wrapper* get_core() const {
			return core_;
		}
	};

	extern helper_singleton* plugin_singleton;
}