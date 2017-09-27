#include "MatchController.h"

#include "StreamResponse.h"

#include <boost/foreach.hpp>

namespace Mongoose
{
	MatchController::MatchController()
    {
    }

	MatchController::~MatchController() {
		BOOST_FOREACH(handler_map::value_type &handler, routes) {
            delete handler.second;
        }
        routes.clear();
    }
            
    bool MatchController::handles(std::string method, std::string url) {
		std::string key = method + ":" + url;

        return (routes.find(key) != routes.end());
    }

    Response *MatchController::handleRequest(Request &request) {
		Response *response = NULL;
		std::string key = request.getMethod() + ":" + request.getUrl();
		if (routes.find(key) != routes.end()) {
			response = routes[key]->process(request);
		}
		return response;
    }

    void MatchController::registerRoute(std::string httpMethod, std::string route, RequestHandlerBase *handler) {
		std::string key = httpMethod + ":" + prefix + route;
        routes[key] = handler;
    }

}
