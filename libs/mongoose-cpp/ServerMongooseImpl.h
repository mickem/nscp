// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <boost/atomic/atomic.hpp>
#include <boost/thread/mutex.hpp>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <threads/queue.hpp>
#include <vector>

#include "Controller.h"
#include "Request.h"
#include "Response.h"
#include "Server.h"
#include "dll_defines.hpp"

// clang-format off
// Has to be after boost or we get namespace clashes
#include "mongoose_wrapper.h"
// clang-format on

/**
 * Wrapper for the Mongoose server
 */
namespace Mongoose {

class NSCP_MONGOOSE_EXPORT ServerMongooseImpl final : public Server {
 public:
  /**
   * Constructs the server
   *
   * @param logger the logger to use for logging
   */
  explicit ServerMongooseImpl(WebLoggerPtr logger);
  ~ServerMongooseImpl() override;

  /**
   * Runs the Mongoose server
   */
  bool start(const std::string &bind) override;

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

  void onHttpRequest(mg_connection *connection, mg_http_message *message);

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
  bool setSsl(std::string &certificate, std::string &key) override;
  void setWorkerThreads(std::size_t threads) override;
  void setAcceptFilter(accept_filter filter) override;
  bool isServerThread() const override;
  void setThreadReporting(const std::string &thread_name, thread_reporter reporter) override;
  void setTlsOptions(const std::string &tls_version, const std::string &ciphers) override;

  /**
   * Does the server handles url?
   */
  bool handles(std::string method, std::string url);

  void thread_proc();

  // A finished answer, rendered to what mg_http_reply() takes.
  struct reply {
    int code = 0;
    std::string headers;
    std::string body;
  };
  // Render a controller's response (security headers, cookies, content type).
  static reply render(Response &response, bool is_ssl);

  // Where this server's mongoose log lines go, and whether the self-signed
  // certificate hint has been given. Per server: mongoose's log hook is
  // process-wide, so each poll thread routes its own lines (see log_wrapper).
  struct log_target {
    WebLogger *logger = nullptr;
    std::atomic<bool> cert_issue_logged{false};
  };

 private:
  // Worker pool (setWorkerThreads > 1): a request is handed to a worker with
  // the connection still marked as answering (mongoose holds back pipelined
  // requests meanwhile), and the worker's reply comes back to the poll thread,
  // the only thread that may write to a connection.
  struct job {
    unsigned long connection_id = 0;
    Controller *controller = nullptr;
    std::unique_ptr<Request> request;
    bool is_ssl = false;
    bool close = false;
  };
  struct pending_reply {
    reply answer;
    bool close = false;
  };
  // Everything the workers share with the server, held by shared_ptr so the
  // workers' own bookkeeping never goes through the server object: a stop()
  // run on a worker detaches them, and the pool lives on until the last of
  // them has gone. (The controller a worker is running still belongs to the
  // server; see stop_and_release() for freeing a server from its own thread.)
  struct worker_pool {
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<job> jobs;
    // No new jobs are queued and the workers leave once their current one
    // returns.
    bool stopping = false;
    // The manager is gone (the poll thread has stopped): an answer finished
    // now is dropped instead of being handed to mg_wakeup().
    bool orphaned = false;
    mg_mgr *mgr = nullptr;
    // Connections waiting for a worker's answer, and the answers not yet
    // written. A connection that closes in between is dropped from both, so a
    // late answer is discarded rather than kept.
    std::set<unsigned long> waiting;
    std::map<unsigned long, pending_reply> replies;
    // replies.size(), readable without the lock: mg_wakeup() can drop its
    // datagram without saying so, so the poll loop sweeps whenever this is
    // non-zero.
    std::atomic<std::size_t> ready{0};
  };
  static void worker_proc(const std::shared_ptr<worker_pool> &pool);
  // Store a finished answer for the poll thread to write.
  static void hand_back(worker_pool &pool, const job &finished, reply answer);
  bool admits(mg_connection *connection) const;
  // Answers handed back but not yet written, or written but not yet sent.
  // Poll thread only.
  bool has_unsent_answers() const;
  void deliver(mg_connection *connection);
  // Write every answer that is ready (poll thread).
  void deliver_ready();
  void forget(unsigned long connection_id);
  // 503 "Server is stopping" and close (poll thread).
  void reply_stopping(mg_connection *connection);

 protected:
  WebLoggerPtr logger_;
  std::string certificate;
  std::string key;
  // setSsl() was called and the certificate did not load: start() refuses.
  bool ssl_failed_ = false;
  mg_mgr mgr{};

  std::vector<Controller *> controllers;

  boost::atomic<bool> stop_thread_;
  boost::timed_mutex mutex_;
  std::shared_ptr<boost::thread> thread_;
  // Whether a poll thread took ownership of mgr (it frees it when it stops).
  // A server that never got that far frees it in its destructor.
  bool poll_thread_owns_mgr_ = false;

  log_target log_target_;

  std::size_t worker_threads_ = 1;
  accept_filter accept_filter_;
  std::string thread_name_ = "web server";
  thread_reporter thread_reporter_;
  // False until start() succeeds and again from the moment stop() begins: no
  // new connection is kept and no new request is taken while the server
  // drains, so nothing is accepted that no worker will run.
  std::atomic<bool> accepting_{false};
  // Set while a worker pool runs; null when every request is answered on the
  // poll thread.
  std::shared_ptr<worker_pool> pool_;
  std::vector<std::shared_ptr<boost::thread>> workers_;
};
}  // namespace Mongoose
