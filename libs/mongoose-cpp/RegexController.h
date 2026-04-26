#pragma once

#include <boost/regex.hpp>
#include <string>

#include "Controller.h"
#include "RegexRequestHandler.h"
#include "Request.h"
#include "Response.h"
#include "StreamResponse.h"
#include "dll_defines.hpp"

/**
 * A controller is a module that respond to requests
 *
 * You can override the preProcess, process and postProcess to answer to
 * the requests
 */
namespace Mongoose {
struct route_info {
  std::string verb;
  boost::regex regexp;
  RegexpRequestHandlerBase *function;
};

class NSCAPI_EXPORT RegexpController : public Controller {
 public:
  explicit RegexpController(std::string prefix);
  ~RegexpController() override;

  /**
   * Handle a request, this will try to match the request, if this
   * controller handles it, it will preProcess, process then postProcess it
   *
   * @param request the request
   *
   * @return Response the created response, or NULL if the controller
   *         does not handle this request
   */
  Response *handleRequest(Request &request) override;

  /**
   * Registers a route to the controller
   *
   * @param http_method the HTTP method for this route (e.g. "GET", "POST")
   * @param route the route path (e.g. "/items/(\\d+)")
   * @param handler the request handler for this route, which will receive the regex match results as a boost::smatch
     object. The handler is responsible for validating the number of capture groups in smatch and returning an appropriate error response if the count is wrong.
     See RegexpController::validate_arguments for a helper function to do this validation.
     Note that the regex should be anchored (e.g. "^/items/(\\d+)$") to ensure that it matches the entire path.
   */
  virtual void registerRoute(std::string http_method, std::string route, RegexpRequestHandlerBase *handler);

  /**
   * Sets the controller prefix, for instance "/api"
   *
   * @param prefix the prefix of all urls for this controller
   */
  void setPrefix(const std::string &prefix);
  std::string get_prefix() const;

  template <class T>
  void addRoute(std::string httpMethod, std::string url, T *instance, typename RegexpRequestHandler<T, StreamResponse>::fPtr handler) {
    registerRoute(httpMethod, url, new RegexpRequestHandler<T, StreamResponse>(instance, handler));
  }

  bool handles(std::string method, std::string url) override;

  static bool validate_arguments(std::size_t count, const boost::smatch &what, StreamResponse &response);

  std::string get_base(const Request &request) const;

 protected:
  typedef std::list<route_info> routes_type;
  routes_type routes;
  std::string prefix;
};
}  // namespace Mongoose
