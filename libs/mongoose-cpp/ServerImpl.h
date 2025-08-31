#pragma once

#include <boost/atomic/atomic.hpp>
#include <boost/thread/mutex.hpp>
#include <threads/queue.hpp>
#include <vector>

#include "Controller.h"
#include "Request.h"
#include "Response.h"
#include "Server.h"
#include "dll_defines.hpp"

// clang-format off
// Has to be after boost or we get namespace clashes
#include <mongoose.h>
// clang-format on

/**
 * Wrapper for the Mongoose server
 */
namespace Mongoose {

class NSCAPI_EXPORT ServerImpl final : public Server {
 public:
  /**
   * Constructs the server
   *
   * @param logger the logger to use for logging
   */
  explicit ServerImpl(WebLoggerPtr logger);
  ~ServerImpl() override;

  /**
   * Runs the Mongoose server
   */
  void start(std::string &bind) override;

  /**
   * Stops the Mongoose server
   */
  void stop() override;

  /**
   * Register a new controller on the server
   *
   * @param controller a pointer to a controller
   */
  void registerController(Controller *controller) override;

  /**
   * Main event handler (called by mongoose when something happens)
   *
   * @param connection the mongoose connection
   * @param ev event type
   * @param ev_data event data
   */
  static void event_handler(mg_connection *connection, int ev, void *ev_data);

  void onHttpRequest(mg_connection *connection, mg_http_message *message) const;

  /**
   * Process the request by controllers
   *
   * @param request the request
   *
   * @return Response the response if one of the controllers can handle it,
   *         NULL else
   */
  Response *handleRequest(Request &request);

  /**
   * Setup the mongoose ssl options section
   *
   * @param certificate the name of the certificate to use
   */
#if MG_ENABLE_OPENSSL
  void initTls(mg_connection *connection) const;
#endif
  void setSsl(std::string &certificate, std::string &key) override;

  /**
   * Does the server handles url?
   */
  bool handles(std::string method, std::string url);

  void thread_proc();

 protected:
  WebLoggerPtr logger_;
  std::string certificate;
  std::string key;
  std::string ciphers;
  mg_mgr mgr{};

  std::vector<Controller *> controllers;

  boost::atomic<bool> stop_thread_;
  boost::timed_mutex mutex_;
  boost::shared_ptr<boost::thread> thread_;
};
}  // namespace Mongoose
