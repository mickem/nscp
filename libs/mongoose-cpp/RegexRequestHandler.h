#pragma once

#include "Request.h"
#include "Response.h"

#include <boost/regex.hpp>

#include <string>

namespace Mongoose
{

	class RegexpRequestHandlerBase {
	public:
		virtual Response *process(Request &request, boost::smatch &what) = 0;
	};

    template<typename T, typename R>
    class RegexpRequestHandler : public RegexpRequestHandlerBase
    {
        public:
            typedef void (T::*fPtr)(Request &request, boost::smatch &what, R &response);

			RegexpRequestHandler(T *controller_, fPtr function_)
                : controller(controller_), function(function_)
            {
            }

            Response *process(Request &request, boost::smatch &what)
            {
                R *response = new R;

                try {
                    (controller->*function)(request, what, *response);
				} catch (string exception) {
					return controller->serverInternalError(exception);
				} catch (const std::exception &exception) {
					return controller->serverInternalError(exception.what());
				} catch (...) {
                    return controller->serverInternalError("Unknown error");
                }

                return response;
            }

        protected:
            T *controller;
            fPtr function;
    };
}
