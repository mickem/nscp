#pragma once

#include <boost/thread/shared_mutex.hpp>
#include <map>
#include <string>

#include "script_interface.hpp"

struct script_provider final : script_provider_interface {
 private:
  int id_;
  nscapi::core_wrapper* core_;
  boost::filesystem::path root_;
  std::map<std::string, std::string> wrappings_;
  commands::command_handler commands_;
  boost::shared_mutex mutex_;

 public:
  script_provider(int id, nscapi::core_wrapper* core, const std::string& settings_path, const boost::filesystem::path& root,
                  const std::map<std::string, std::string>& wrappings);

  unsigned int get_id() override;
  nscapi::core_wrapper* get_core() override;
  boost::shared_ptr<nscapi::settings_proxy> get_settings_proxy() override;

  boost::filesystem::path get_root() override;
  std::string generate_wrapped_command(std::string command) override;

  void setup_commands();
  void add_command(std::string alias, std::string script) override;
  commands::command_object_instance find_command(std::string alias) override;
  void remove_command(std::string alias) override;
  std::list<std::string> get_commands() override;
};
