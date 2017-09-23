#ifndef _MONGOOSE_CONTROLLER_H
#define _MONGOOSE_CONTROLLER_H

#include "Request.h"
#include "Response.h"
#include "RequestHandler.h"
#include "StreamResponse.h"

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
    class Server;

    class NSCAPI_EXPORT Controller
    {
        public:
            Controller();
            virtual ~Controller();
            
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
             * Sets the controller prefix, for instance "/api"
             *
             * @param string the prefix of all urls for this controller
             */
            void setPrefix(std::string prefix);

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

// #define addRouteResponse(httpMethod, url, controllerType, method, responseType) \
//     registerRoute(httpMethod, url, new Mongoose::RequestHandler<controllerType, responseType>(this, &controllerType::method ));

            /**
             * Called when an exception occur during the rendering
             *
             * @param string the error message
             *
             * @return response a response to send, 404 will occur if NULL
             */
            virtual Response *serverInternalError(std::string message);

            virtual bool handles(std::string method, std::string url);

        protected:
			std::string prefix;
			typedef map<std::string, RequestHandlerBase*> handler_map;
			handler_map routes;
    };
}

#endif
