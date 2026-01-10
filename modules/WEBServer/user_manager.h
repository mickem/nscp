#ifndef NSCP_USER_MANAGER_H
#define NSCP_USER_MANAGER_H
#include <boost/unordered/unordered_map.hpp>
#include <string>

class user_manager {
  boost::unordered_map<std::string, std::string> users;

 public:
  bool validate_user(const std::string &user, const std::string &password);
  void add_user(const std::string &user, const std::string &password);
  bool has_user(const std::string &user) const;
};

#endif  // NSCP_USER_MANAGER_H
