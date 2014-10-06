#pragma once

#include <parsers/where/dll_defines.hpp>


namespace parsers {
	namespace where {

		struct constants {
			static NSCAPI_EXPORT long long now;
			static NSCAPI_EXPORT long long get_now();
			static NSCAPI_EXPORT void reset();
		};
	}
}


