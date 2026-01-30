#pragma once

#include <boost/filesystem/path.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <map>
#include <nsclient/logger/logger.hpp>
#include <string>

namespace nsclient {
namespace core {

class path_manager {
  typedef std::map<std::string, std::string> paths_type;

  logging::log_client_accessor log_instance_;
  boost::timed_mutex mutex_;
  boost::filesystem::path basePath;
  boost::filesystem::path tempPath;
  paths_type paths_cache_;

 public:
  path_manager(const logging::log_client_accessor& log_instance_);
  std::string getFolder(const std::string& key);
  std::string expand_path(std::string file);

 private:
  std::string get_app_data_path();
  boost::filesystem::path getBasePath();
  boost::filesystem::path getTempPath();
  logging::log_client_accessor get_logger() { return log_instance_; }
};
typedef boost::shared_ptr<path_manager> path_instance;
}  // namespace core

}  // namespace nsclient
