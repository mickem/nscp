#pragma once

#include <RegexController.h>
#include <StreamResponse.h>

#include <boost/thread/shared_mutex.hpp>
#include <client/simple_client.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <string>

#include "session_manager_interface.hpp"

class log_controller : public Mongoose::RegexpController {
  boost::shared_ptr<session_manager_interface> session;
  const nscapi::core_wrapper *core;
  const unsigned int plugin_id;

  typedef std::vector<std::pair<std::string, std::string> > arg_vector;

 public:
  log_controller(int version, const boost::shared_ptr<session_manager_interface> &session, const nscapi::core_wrapper *core, unsigned int plugin_id);

  void get_log(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
  void get_log_since(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
  void add_log(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
  void get_status(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
  void reset_status(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
  void delete_logs(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
};
