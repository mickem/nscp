#pragma once

#include <boost/optional.hpp>
#include <boost/thread/mutex.hpp>
#include <map>
#include <string>

class bookmarks {
 public:
  typedef boost::optional<std::string> op_string;
  typedef std::map<std::string, std::string> map_type;

 private:
  boost::timed_mutex mutex_;
  map_type bookmarks_;

 public:
  void add(const std::string key, const std::string bookmark);
  op_string get(const std::string key);
  map_type get_copy();
};
