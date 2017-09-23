#include "Controller.h"

#include "StreamResponse.h"

#include <boost/foreach.hpp>

namespace Mongoose
{
    Controller::Controller() 
        : prefix("")
    {
    }

    Controller::~Controller() {
		BOOST_FOREACH(handler_map::value_type &handler, routes) {
            delete handler.second;
        }
        routes.clear();
    }
            
    bool Controller::handles(std::string method, std::string url) {
		std::string key = method + ":" + url;

        return (routes.find(key) != routes.end());
    }

    Response *Controller::handleRequest(Request &request) {
		Response *response = NULL;
		std::string key = request.getMethod() + ":" + request.getUrl();
		if (routes.find(key) != routes.end()) {
			response = routes[key]->process(request);
		}
		return response;
    }

    void Controller::setPrefix(std::string prefix_) {
        prefix = prefix_;
    }
            
    void Controller::registerRoute(std::string httpMethod, std::string route, RequestHandlerBase *handler) {
		std::string key = httpMethod + ":" + prefix + route;
        routes[key] = handler;
    }

    Response *Controller::serverInternalError(std::string message) {
        StreamResponse *response = new StreamResponse;

        response->setCode(HTTP_SERVER_ERROR);
        response->append("[500] Server internal error: " + message);

        return response;
    }
}
