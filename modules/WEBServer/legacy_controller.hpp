// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <MatchController.h>
#include <StreamResponse.h>

#include <boost/thread/shared_mutex.hpp>
#include <client/simple_client.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <string>

#include "session_manager_interface.hpp"

// The pre-v1 HTTP endpoints, kept for clients that have not moved to
// /api/v2. What is left here is JSON and plain text; the raw-protobuf routes
// this class used to carry are gone:
//
//   POST /query.pb          - took a serialized QueryRequestMessage and passed
//                             it into the core verbatim, header included, so
//                             the caller chose the identity the permission
//                             layer attributed the call to. Its only consumer
//                             was NSClient++'s own NSCPClient, which now uses
//                             /api/v2/queries like every other client.
//   POST /settings/query.pb - unreachable since the static controller began
//                             claiming every /settings URL, and it read
//                             settings without the redaction the v2 endpoints
//                             apply.
//   run_exec_pb             - never registered on any route.
//
// Anything that still needs to run a check over HTTP uses
// GET /api/v2/queries/<command>/commands/execute, which stamps the caller
// identity from the authenticated session instead of taking it on trust.
class legacy_controller : public Mongoose::MatchController {
  std::shared_ptr<session_manager_interface> session;
  const nscapi::core_wrapper *core;
  const unsigned int plugin_id;
  std::shared_ptr<client::cli_client> client;

  std::string status;
  boost::shared_mutex mutex_;

 public:
  legacy_controller(const std::shared_ptr<session_manager_interface> &session, const nscapi::core_wrapper *core, unsigned int plugin_id,
                    const std::shared_ptr<client::cli_client> &client);
  std::string get_status();
  bool set_status(std::string status_);
  void console_exec(Mongoose::Request &request, Mongoose::StreamResponse &response);

  void auth_token(Mongoose::Request &request, Mongoose::StreamResponse &response);
  void auth_logout(Mongoose::Request &request, Mongoose::StreamResponse &response);

  void log_status(Mongoose::Request &request, Mongoose::StreamResponse &response);
  void log_messages(Mongoose::Request &request, Mongoose::StreamResponse &response);
  void get_metrics(Mongoose::Request &request, Mongoose::StreamResponse &response);
  void log_reset(Mongoose::Request &request, Mongoose::StreamResponse &response);
  void reload(Mongoose::Request &request, Mongoose::StreamResponse &response);
  void alive(Mongoose::Request &request, Mongoose::StreamResponse &response);
};
