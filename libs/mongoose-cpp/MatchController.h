#pragma once

#include "Request.h"
#include "Response.h"
#include "RequestHandler.h"
#include "StreamResponse.h"
#include "Controller.h"

#include "dll_defines.hpp"

#include <map>
#include <vector>
#include <string>

/**
 * A controller is a module that respond to requests
 * 
 * You can override the preProcess, process and postProcess to answer to
 * the requests
 */
namespace Mongoose
{
    class NSCAPI_EXPORT MatchController : public Controller
    {
        public:
			MatchController();
			MatchController(std::string prefix);
			virtual ~MatchController();
            
            /**
             * Handle a request, this will try to match the request, if this
             * controller handles it, it will preProcess, process then postProcess it
             *
             * @param Request the request
             *
             * @return Response the created response, or NULL if the controller
             *         does not handle this request
             */
            virtual Response *handleRequest(Request &request);

            /**
             * Registers a route to the controller
             *
             * @param string the route path
             * @param RequestHandlerBase the request handler for this route
             */
            virtual void registerRoute(std::string httpMethod, std::string route, RequestHandlerBase *handler);

			template<class T>
			void addRoute(std::string httpMethod, std::string url, T *instance, typename RequestHandler<T, Mongoose::StreamResponse>::fPtr handler) {
				registerRoute(httpMethod, url, new Mongoose::RequestHandler<T, Mongoose::StreamResponse>(instance, handler));
			}


            virtual bool handles(std::string method, std::string url);

        protected:
			std::string prefix;
			typedef map<std::string, RequestHandlerBase*> handler_map;
			handler_map routes;
    };
}
