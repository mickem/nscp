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
//   GET    /api/v2/results          list, optionally filtered   results.list
//   GET    /api/v2/results/{key}    one cached result           results.get
//   DELETE /api/v2/results          drop everything cached      results.delete
//   DELETE /api/v2/results/{key}    drop one cached result      results.delete
class results_controller : public Mongoose::RegexpController {
  std::shared_ptr<session_manager_interface> session;
  std::shared_ptr<result_store> results;

 public:
  results_controller(int version, const std::shared_ptr<session_manager_interface> &session, const std::shared_ptr<result_store> &results);

  void list_results(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
  void get_result(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
  void clear_results(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
  void delete_result(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
};
