#include "script_provider.hpp"

#include <str/utils.hpp>
#include <file_helpers.hpp>

#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

script_provider::script_provider(int id, nscapi::core_wrapper *core, std::string settings_path, boost::filesystem::path root)
	: core_(core)
	, id_(id)
	, root_(root)
{
	//commands_.set_path(settings_path);
}

unsigned int script_provider::get_id() {
	return id_;
}

nscapi::core_wrapper* script_provider::get_core() {
	return core_;
}

boost::shared_ptr<nscapi::settings_proxy> script_provider::get_settings_proxy() {
	return boost::shared_ptr<nscapi::settings_proxy>(new nscapi::settings_proxy(get_id(), get_core()));
}

boost::filesystem::path script_provider::get_root() {
	return root_ / "scripts" / "python";
}

boost::optional<boost::filesystem::path> script_provider::find_file(std::string file) {
	std::list<boost::filesystem::path> checks;
	checks.push_back(file);
	checks.push_back(file + ".py");
	checks.push_back(root_ / "scripts" / "python" / file);
	checks.push_back(root_ / "scripts" / "python" / (file + ".py"));
	checks.push_back(root_ / "scripts" / file);
	checks.push_back(root_ / "scripts" / (file + ".py"));
	checks.push_back(root_ / file);
	BOOST_FOREACH(boost::filesystem::path c, checks) {
		if (boost::filesystem::exists(c) && boost::filesystem::is_regular(c))
			return boost::optional<boost::filesystem::path>(c);
	}
	get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Script not found: " + file);
	return boost::optional<boost::filesystem::path>();
}

void script_provider::add_command(std::string script_alias, std::string script, std::string plugin_alias) {
	try {
		if (script.empty()) {
			script = script_alias;
			script_alias = "";
		}
		boost::optional<boost::filesystem::path> ofile = find_file(script);
		if (!ofile) {
			get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to find script: " + script);
			return;
		}
		std::string script_file = ofile->string();
		get_core()->log(NSCAPI::log_level::debug, __FILE__, __LINE__, "Adding script: " + script_alias + " (" + script_file + ")");


		boost::shared_ptr<python_script> instance = boost::make_shared<python_script>(get_id(), root_.string(), plugin_alias, script_alias, script_file);
		{
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(30));
			if (!writeLock.owns_lock()) {
				get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to get mutex: add_command");
				return;
			}
			instances_.push_back(instance);
		}
	} catch (...) {
		get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to add script: " + script);
	}
}

// commands::command_object_instance script_provider::find_command(std::string alias) {
// 	boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
// 	if (!readLock.owns_lock()) {
// 		get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to get mutex: find_command");
// 		return commands::command_object_instance();
// 	}
// 	return commands_.find_object(alias);
// }

void script_provider::remove_command(std::string alias) {
	boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(30));
	if (!writeLock.owns_lock()) {
		get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to get mutex: remove_command");
		return;
	}
	//commands_.remove(alias);
}

void script_provider::clear() {
	boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(30));
	if (!writeLock.owns_lock()) {
		get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to get mutex: remove_command");
		return;
	}
	instances_.clear();
}

