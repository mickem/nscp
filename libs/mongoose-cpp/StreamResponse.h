#ifndef _MONGOOSE_STREAM_RESPONSE_H
#define _MONGOOSE_STREAM_RESPONSE_H

#include "Response.h"

#include "dll_defines.hpp"

#include <map>
#include <sstream>
#include <iostream>

/**
 * A stream response to a request
 */
namespace Mongoose
{
    class NSCAPI_EXPORT StreamResponse : public Response {
	private:
		std::stringstream ss;
    public:
        /**
         * Gets the response body
         *
         * @return string the response body
         */
		virtual std::string getBody();
		void append(std::string data);
		void write(const char* buffer, std::size_t len);
    };
}

#endif
