#include "MatchController.h"

#include <boost/algorithm/string.hpp>
#include <utility>

namespace Mongoose {

MatchController::MatchController(std::string prefix) : prefix(std::move(prefix)) {}

MatchController::~MatchController() {
  for (const handler_map::value_type &handler : routes) {
    delete handler.second;
  }
  routes.clear();
}

bool MatchController::handles(const std::string method, const std::string url) {
  std::string key = method + ":" + url;
  if (!prefix.empty()) {
    if (!boost::algorithm::starts_with(url, prefix)) {
      return false;
    }
    key = method + ":" + url.substr(prefix.size());
  }

  return (routes.find(key) != routes.end());
}

Response *MatchController::handleRequest(Request &request) {
  Response *response = nullptr;
  const std::string key = request.getMethod() + ":" + request.getUrl();
  if (routes.find(key) != routes.end()) {
    response = routes[key]->process(request);
  }
  return response;
}

void MatchController::registerRoute(std::string http_method, std::string route, RequestHandlerBase *handler) {
  const std::string key = http_method + ":" + prefix + route;
  routes[key] = handler;
}

}  // namespace Mongoose
