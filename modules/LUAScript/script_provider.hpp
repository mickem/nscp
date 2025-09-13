#pragma once

#include <boost/filesystem/path.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <string>

struct script_provider {
 private:
  int id_;
  nscapi::core_wrapper *core_;
  boost::filesystem::path root_;

 public:
  script_provider(int id, nscapi::core_wrapper *core, boost::filesystem::path root);

  unsigned int get_id();
  nscapi::core_wrapper *get_core();
  boost::shared_ptr<nscapi::settings_proxy> get_settings_proxy();

  boost::filesystem::path get_root();
  boost::optional<boost::filesystem::path> find_file(std::string file);
};
