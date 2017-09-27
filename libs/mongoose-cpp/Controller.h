#pragma once

#include "Request.h"
#include "Response.h"

#include "dll_defines.hpp"

#include <string>

/**
 * A controller is a module that respond to requests
 * 
 * You can override the preProcess, process and postProcess to answer to
 * the requests
 */
namespace Mongoose
{
    class NSCAPI_EXPORT Controller
    {
        public:
            /**
             * Handle a request, this will try to match the request, if this
             * controller handles it, it will preProcess, process then postProcess it
             *
             * @param Request the request
             *
             * @return Response the created response, or NULL if the controller
             *         does not handle this request
             */
            virtual Response *handleRequest(Request &request) = 0;
            virtual bool handles(std::string method, std::string url) = 0;

            /**
             * Called when an exception occur during the rendering
             *
             * @param string the error message
             *
             * @return response a response to send, 404 will occur if NULL
             */
            Response* serverInternalError(std::string message);
            Response* documentMissing(std::string message);


    };
}
