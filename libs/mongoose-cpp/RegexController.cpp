#include "RegexController.h"

#include <boost/algorithm/string.hpp>
#include <utility>

#include "StreamResponse.h"

namespace Mongoose {
RegexpController::RegexpController(std::string prefix) : prefix(std::move(prefix)) {}

RegexpController::~RegexpController() {
  for (const routes_type::value_type &r : routes) {
    delete r.function;
  }
  routes.clear();
}

bool RegexpController::handles(std::string method, const std::string url) { return boost::algorithm::starts_with(url, prefix); }

Response *RegexpController::handleRequest(Request &request) {
  const std::string key = request.getUrl().substr(prefix.length());
  for (const route_info &i : routes) {
    if (i.verb == request.getMethod()) {
      boost::smatch what;
      if (boost::regex_match(key, what, i.regexp)) {
        return i.function->process(request, what);
      }
    }
  }
  return documentMissing("invalid handler for \"" + request.getMethod() + ":" + key + "\" in " + prefix);
}

void RegexpController::setPrefix(const std::string &prefix_) { prefix = prefix_; }
std::string RegexpController::get_prefix() const { return prefix; }

void RegexpController::registerRoute(std::string http_method, std::string route, RegexpRequestHandlerBase *handler) {
  std::string key = http_method + ":" + prefix + route;
  route_info i;
  i.function = handler;
  i.regexp = route;
  i.verb = http_method;
  routes.push_back(i);
}

bool RegexpController::validate_arguments(const std::size_t count, const boost::smatch &what, StreamResponse &response) {
  if (what.size() != (count + 1)) {
    response.setCodeBadRequest("Invalid request");
    return false;
  }
  return true;
}

std::string RegexpController::get_base(const Request &request) const { return request.get_host() + prefix; }

}  // namespace Mongoose
