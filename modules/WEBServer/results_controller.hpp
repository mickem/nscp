// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <RegexController.h>
#include <StreamResponse.h>

#include <memory>

#include "result_store.hpp"
#include "session_manager_interface.hpp"

// REST view over the passive-result cache (see result_store.hpp):
//
//   GET    /api/v2/results          poll, optionally filtered   results.list
//   GET    /api/v2/results/{key}    one cached result           results.get
//   DELETE /api/v2/results          drop everything cached      results.delete
//   DELETE /api/v2/results/{key}    drop one cached result      results.delete
//
// The list route is a *poll*: with `clear on poll` (the default) the entries
// it returns are removed, so the next poll reports what has happened since
// this one rather than repeating it. That is what makes the `worst` cache
// mode meaningful - it holds the worst result since the previous poll.
// Fetching a single key is a lookup, not a poll, and never drains.
//
// Every route answers 503 while the cache is disabled, rather than 404 or a
// misleading empty list, so an operator can tell "switched off" from "nothing
// has reported yet".
class results_controller : public Mongoose::RegexpController {
  std::shared_ptr<session_manager_interface> session;
  std::shared_ptr<result_store> results;
  bool clear_on_poll;

 public:
  results_controller(int version, const std::shared_ptr<session_manager_interface> &session, const std::shared_ptr<result_store> &results, bool clear_on_poll);

  void list_results(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
  void get_result(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
  void clear_results(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
  void delete_result(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);

 private:
  // Answers 503 and returns false when the cache is switched off.
  bool require_enabled(Mongoose::StreamResponse &response) const;
};
