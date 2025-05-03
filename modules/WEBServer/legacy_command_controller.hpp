#pragma once

#include "session_manager_interface.hpp"

#include <client/simple_client.hpp>

#include <nscapi/nscapi_core_wrapper.hpp>

#include <MatchController.h>
#include <RegexController.h>
#include <StreamResponse.h>

#include <boost/thread/shared_mutex.hpp>

#include <string>

class legacy_command_controller : public Mongoose::RegexpController {
  boost::shared_ptr<session_manager_interface> session;
  const nscapi::core_wrapper *core;
  const unsigned int plugin_id;
  boost::shared_ptr<client::cli_client> client;

  boost::shared_mutex mutex_;

 public:
  legacy_command_controller(boost::shared_ptr<session_manager_interface> session, nscapi::core_wrapper *core, unsigned int plugin_id,
                    boost::shared_ptr<client::cli_client> client);
  void handle_query(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
};
