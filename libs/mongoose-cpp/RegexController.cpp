#include "RegexController.h"

#include "StreamResponse.h"

#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

namespace Mongoose
{
	RegexpController::RegexpController(std::string prefix)
        : prefix(prefix)
    {
    }

	RegexpController::~RegexpController() {
		BOOST_FOREACH(routes_type::value_type &r, routes) {
            delete r.function;
        }
        routes.clear();
    }
            
    bool RegexpController::handles(std::string method, std::string url) {
		return boost::algorithm::starts_with(url, prefix);
    }

    Response* RegexpController::handleRequest(Request &request) {
		std::string key = request.getUrl().substr(prefix.length());
		BOOST_FOREACH(const route_info &i, routes) {
			if (i.verb == request.getMethod()) {
				boost::smatch what;
				if (boost::regex_match(key, what, i.regexp)) {
					return i.function->process(request, what);
				}
			}
		}
		return documentMissing("invalid handler for \"" + request.getMethod() + ":" + key + "\" in " + prefix);
    }

    void RegexpController::setPrefix(std::string prefix_) {
        prefix = prefix_;
    }
	std::string RegexpController::get_prefix() const {
		return prefix;
	}
            
    void RegexpController::registerRoute(std::string httpMethod, std::string route, RegexpRequestHandlerBase *handler) {
		std::string key = httpMethod + ":" + prefix + route;
		route_info i;
		i.function = handler;
		i.regexp = route;
		i.verb = httpMethod;
		routes.push_back(i);
    }

	bool RegexpController::validate_arguments(std::size_t count, boost::smatch &what, Mongoose::StreamResponse &response) {
		if (what.size() != (count+1)) {
			response.setCode(HTTP_BAD_REQUEST);
			response.append("Invalid request");
			return false;
		}
		return true;
	}

	std::string RegexpController::get_base(Request &request) {
		return request.get_host() + prefix;
	}

}
