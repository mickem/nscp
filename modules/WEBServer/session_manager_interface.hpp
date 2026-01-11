#pragma once

#include <Request.h>
#include <StreamResponse.h>

#include <list>
#include <socket/allowed_hosts.hpp>
#include <string>

#include "error_handler_interface.hpp"
#include "metrics_handler.hpp"
#include "token_store.hpp"
#include "user_manager.h"
struct session_manager_interface {
 private:
  error_handler_interface *log_data;

  metrics_handler metrics_store;
  token_store tokens;
  socket_helpers::allowed_hosts_manager allowed_hosts;
  user_manager users;

 public:
  session_manager_interface();

  bool process_auth_header(const std::string &grant, Mongoose::Request &request, Mongoose::StreamResponse &response);
  bool is_logged_in(const std::string &grant, Mongoose::Request &request, Mongoose::StreamResponse &response);

  bool is_allowed(const std::string &ip);

  bool validate_token(const std::string &token);
  void revoke_token(const std::string &token);
  std::string generate_token(const std::string &user);

  std::string get_metrics();
  std::string get_metrics_v2();
  std::string get_open_metrics();
  void set_metrics(const std::string &metrics, const std::string &metrics_list, std::list<std::string> open_metrics);

  void add_log_message(bool is_error, const error_handler_interface::log_entry &entry) const;
  error_handler_interface *get_log_data() const;
  void reset_log() const;

  void set_allowed_hosts(const std::string &host);
  void set_allowed_hosts_cache(bool value);

  std::list<std::string> boot();
  bool validate_user(const std::string &user, const std::string &password);
  void store_user_in_response(const std::string &user, Mongoose::StreamResponse &response);
  void store_token_in_response(const std::string &token, Mongoose::StreamResponse &response) const;
  bool can(const std::string &grant, Mongoose::StreamResponse &response);
  static void get_user_from_response(const Mongoose::StreamResponse &response, std::string &user, std::string &key);
  void add_user(const std::string &user, const std::string &role, const std::string &password);
  bool has_user(const std::string &user) const;
  void add_grant(const std::string &role, const std::string &grant);
};
