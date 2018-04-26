#pragma once

#include "Request.h"
#include "Response.h"
#include "RegexRequestHandler.h"
#include "StreamResponse.h"
#include "Controller.h"

#include "dll_defines.hpp"

#include <boost/regex.hpp>

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
    struct route_info {
      std::string verb;
      boost::regex regexp;
	  RegexpRequestHandlerBase* function;
	};

    class NSCAPI_EXPORT RegexpController : public Controller
    {
        public:
			RegexpController(std::string prefix);
            virtual ~RegexpController();
            
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
            virtual void registerRoute(std::string httpMethod, std::string route, RegexpRequestHandlerBase *handler);


			/**
			* Sets the controller prefix, for instance "/api"
			*
			* @param string the prefix of all urls for this controller
			*/
			void setPrefix(std::string prefix);
			std::string get_prefix() const;

			template<class T>
			void addRoute(std::string httpMethod, std::string url, T *instance, typename RegexpRequestHandler<T, Mongoose::StreamResponse>::fPtr handler) {
				registerRoute(httpMethod, url, new Mongoose::RegexpRequestHandler<T, Mongoose::StreamResponse>(instance, handler));
			}

            virtual bool handles(std::string method, std::string url);

			bool validate_arguments(std::size_t count, boost::smatch &what, Mongoose::StreamResponse &response);

			std::string get_base(Request &request);
        protected:
          typedef std::list<route_info> routes_type;
          routes_type routes;
          std::string prefix;
    };
}
