#pragma once

#include <boost/filesystem/path.hpp>
#include <boost/thread/mutex.hpp>
#include <map>
#include <memory>
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
  // Path overrides loaded from boot.ini's [paths] section. Populated once
  // during NSCSettingsImpl::boot() before any reader exists, then treated as
  // immutable - no mutex needed on reads. If we ever need to support runtime
  // reload, add synchronisation at that point.
  paths_type overrides_;

 public:
  explicit path_manager(const logging::log_client_accessor& log_instance_);
  std::string getFolder(const std::string& key);
  std::string expand_path(std::string file);

  // Install the path-override map. Intended to be called exactly once from
  // the settings bootstrap, before any other code resolves paths through
  // this manager. Subsequent calls replace the previous overrides.
  void set_overrides(paths_type overrides);

  // Maximum recursion depth for ${var} substitution. Caps the cycle defence
  // ("${a}" -> "${b}" -> "${a}") so a misconfiguration cannot stack-overflow
  // the service. 32 is comfortably more than any sane chain - real templates
  // nest 2-4 levels at most.
  static constexpr int kMaxExpandDepth = 32;

 private:
  std::string get_path_for_key(const std::string& key);
  boost::filesystem::path get_app_data_path();
  boost::filesystem::path getBasePath();
  boost::filesystem::path getTempPath();
  std::string expand_path_impl(std::string file, int depth);
  logging::log_client_accessor get_logger() { return log_instance_; }
};
typedef std::shared_ptr<path_manager> path_instance;
}  // namespace core

}  // namespace nsclient
