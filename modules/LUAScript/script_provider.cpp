#include "script_provider.hpp"

#include <boost/thread.hpp>
#include <file_helpers.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <str/utils.hpp>

script_provider::script_provider(int id, nscapi::core_wrapper* core, boost::filesystem::path root) : core_(core), id_(id), root_(root) {}

unsigned int script_provider::get_id() { return id_; }

nscapi::core_wrapper* script_provider::get_core() { return core_; }

boost::shared_ptr<nscapi::settings_proxy> script_provider::get_settings_proxy() {
  return boost::shared_ptr<nscapi::settings_proxy>(new nscapi::settings_proxy(get_id(), get_core()));
}

boost::filesystem::path script_provider::get_root() { return root_ / "scripts" / "lua"; }

boost::optional<boost::filesystem::path> script_provider::find_file(std::string file) {
  std::list<boost::filesystem::path> checks;
  checks.push_back(file);
  checks.push_back(file + ".lua");
  checks.push_back(root_ / "scripts" / "lua" / file);
  checks.push_back(root_ / "scripts" / "lua" / (file + ".lua"));
  checks.push_back(root_ / "scripts" / file);
  checks.push_back(root_ / "scripts" / (file + ".lua"));
  checks.push_back(root_ / file);
  for (boost::filesystem::path c : checks) {
    if (boost::filesystem::exists(c) && boost::filesystem::is_regular_file(c)) return boost::optional<boost::filesystem::path>(c);
  }
  get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Script not found: " + file + " looked relative to " + root_.string());
  return boost::optional<boost::filesystem::path>();
}
