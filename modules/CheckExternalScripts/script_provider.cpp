#include "script_provider.hpp"

#include <boost/thread.hpp>
#include <str/utils.hpp>

script_provider::script_provider(const int id, nscapi::core_wrapper* core, const std::string& settings_path, const boost::filesystem::path& root,
                                 const std::map<std::string, std::string>& wrappings)
    : id_(id), core_(core), root_(root), wrappings_(wrappings) {
  commands_.set_path(settings_path);
  setup_commands();
}

unsigned int script_provider::get_id() { return id_; }

nscapi::core_wrapper* script_provider::get_core() { return core_; }

boost::shared_ptr<nscapi::settings_proxy> script_provider::get_settings_proxy() { return boost::make_shared<nscapi::settings_proxy>(get_id(), get_core()); }

boost::filesystem::path script_provider::get_root() { return root_; }

std::string script_provider::generate_wrapped_command(const std::string command) {
  const str::utils::token tok = str::utils::getToken(command, ' ');
  const std::string::size_type pos = tok.first.find_last_of(".");
  std::string type = "none";
  if (pos != std::wstring::npos) type = tok.first.substr(pos + 1);
  std::string tpl = wrappings_[type];
  if (tpl.empty()) {
    get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to find wrapping for type: " + type);
  } else {
    str::utils::replace(tpl, "%SCRIPT%", tok.first);
    str::utils::replace(tpl, "%ARGS%", tok.second);
    return tpl;
  }
  return "";
}

void script_provider::setup_commands() {
  commands_.add_samples(get_settings_proxy());
  commands_.add_missing(get_settings_proxy(), "default", "");
}

void script_provider::add_command(const std::string alias, const std::string script) {
  const boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(30));
  if (!writeLock.owns_lock()) {
    get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to get mutex: add_command");
    return;
  }
  commands_.add(get_settings_proxy(), alias, script);
  nscapi::core_helper core(get_core(), get_id());
  core.register_command(alias, "External script: " + script);
}

commands::command_object_instance script_provider::find_command(const std::string alias) {
  const boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!readLock.owns_lock()) {
    get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to get mutex: find_command");
    return commands::command_object_instance();
  }
  return commands_.find_object(alias);
}

void script_provider::remove_command(const std::string alias) {
  const boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(30));
  if (!writeLock.owns_lock()) {
    get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to get mutex: remove_command");
    return;
  }
  commands_.remove(alias);
}

std::list<std::string> script_provider::get_commands() {
  const boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!readLock.owns_lock()) {
    get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to get mutex: get_commands");
    return std::list<std::string>();
  }
  return commands_.get_alias_list();
}
