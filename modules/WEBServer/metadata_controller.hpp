#pragma once

#include <RegexController.h>
#include <StreamResponse.h>

#include <boost/thread/shared_mutex.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <string>

#include "session_manager_interface.hpp"

class metadata_controller : public Mongoose::RegexpController {
  boost::shared_ptr<session_manager_interface> session;
  const nscapi::core_wrapper *core;
  const unsigned int plugin_id;

public:
  metadata_controller(const int version, boost::shared_ptr<session_manager_interface> session, const nscapi::core_wrapper *core, unsigned int plugin_id);

  void get_index(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
  void get_counters(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
  void get_channels(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
};
