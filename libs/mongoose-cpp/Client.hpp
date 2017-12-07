#pragma once

#include "dll_defines.hpp"

#include "Response.h"

#include <boost/shared_ptr.hpp>

#include <string>
#include <map>

/**
 * Wrapper for the Mongoose server
 */
namespace Mongoose
{
    class NSCAPI_EXPORT Client
    {
        public:
			typedef std::map<std::string, std::string> header_type;
			/**
             * Constructs the server
             *
             * @param int the number of the port to listen to
             * @param string documentRoot the root that should be used for static files
             */
            Client(std::string url);
            virtual ~Client();
			boost::shared_ptr<Response> fetch(std::string verb, header_type hdr, std::string payload);

		private:
			std::string url_;
	};
}
