#pragma once

#include "dll_defines.hpp"

#include <string>

/**
 * A stream response to a request
 */
namespace Mongoose
{
	struct NSCAPI_EXPORT Helpers {
		static std::string encode_b64(std::string &str);
		static std::string decode_b64(std::string &str);
	};
}
