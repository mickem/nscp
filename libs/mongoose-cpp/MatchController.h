#pragma once

#include <map>
#include <string>
#include <vector>

#include "Controller.h"
#include "Request.h"
#include "RequestHandler.h"
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
class NSCAPI_EXPORT MatchController : public Controller {
 public:
  MatchController() = default;
  explicit MatchController(std::string prefix);
  ~MatchController() override;

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
   * @param route the route path (e.g. "/items")
   * @param handler the request handler for this route
   */
  virtual void registerRoute(std::string http_method, std::string route, RequestHandlerBase *handler);

  template <class T>
  void addRoute(std::string httpMethod, std::string url, T *instance, typename RequestHandler<T, StreamResponse>::fPtr handler) {
    registerRoute(httpMethod, url, new RequestHandler<T, StreamResponse>(instance, handler));
  }

  bool handles(std::string method, std::string url) override;

 protected:
  std::string prefix;
  typedef std::map<std::string, RequestHandlerBase *> handler_map;
  handler_map routes;
};
}  // namespace Mongoose
