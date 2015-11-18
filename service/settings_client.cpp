#include "settings_client.hpp"


#ifdef HAVE_JSON_SPIRIT
#include <json_spirit.h>
#endif

#include "../libs/settings_manager/settings_manager_impl.h"

#include <settings/config.hpp>

settings::settings_core* nsclient_core::settings_client::get_core() const {
	return settings_manager::get_core();
}

nsclient_core::settings_client::settings_client(NSClient* core, bool update_defaults, bool remove_defaults, bool load_all, bool use_samples) 
	: started_(false), core_(core), default_(update_defaults), remove_default_(remove_defaults), load_all_(load_all), use_samples_(use_samples)
{
	startup();
}


nsclient_core::settings_client::~settings_client() {
	terminate();
}

void nsclient_core::settings_client::startup() {
	if (started_)
		return;
	if (!core_->boot_init(true)) {
		std::cout << "boot::init failed" << std::endl;
		return;
	}
	if (load_all_)
		core_->preboot_load_all_plugin_files();

	if (!core_->boot_load_all_plugins()) {
		std::cout << "boot::load_all_plugins failed!" << std::endl;
		return;
	}
	if (!core_->boot_start_plugins(false)) {
		std::cout << "boot::start_plugins failed!" << std::endl;
		return;
	}
	if (default_) {
		get_core()->update_defaults();
	}
	if (remove_default_) {
		std::cout << "Removing default values" << std::endl;
		get_core()->remove_defaults();
	}
	started_ = true;
}

std::string nsclient_core::settings_client::expand_context(const std::string &key) const {
	return get_core()->expand_context(key);
}
void nsclient_core::settings_client::terminate() {
	if (!started_)
		return;
	core_->stop_unload_plugins_pre();
	core_->stop_exit_pre();
	core_->stop_exit_post();
	started_ = false;
}

int nsclient_core::settings_client::migrate_from(std::string src) {
	try {
		debug_msg("Migrating from: " + expand_context(src));
		get_core()->migrate_from(expand_context(src));
		return 1;
	} catch (settings::settings_exception e) {
		error_msg("Failed to initialize settings: " + e.reason());
	} catch (...) {
		error_msg("FATAL ERROR IN SETTINGS SUBSYTEM");
	}
	return -1;
}
int nsclient_core::settings_client::migrate_to(std::string target) {
	try {
		debug_msg("Migrating to: " + expand_context(target));
		get_core()->migrate_to(expand_context(target));
		return 1;
	} catch (settings::settings_exception e) {
		error_msg("Failed to initialize settings: " + e.reason());
	} catch (...) {
		error_msg("FATAL ERROR IN SETTINGS SUBSYTEM");
	}
	return -1;
}

void nsclient_core::settings_client::dump_path(std::string root) {
	BOOST_FOREACH(const std::string &path, get_core()->get()->get_sections(root)) {
		if (!root.empty())
			dump_path(root + "/" + path);
		else
			dump_path(path);
	}
	BOOST_FOREACH(std::string key, get_core()->get()->get_keys(root)) {
		settings::settings_interface::op_string val = get_core()->get()->get_string(root, key);
		if (val)
			std::cout << root << "." << key << "=" << *val << std::endl;
	}
}

int nsclient_core::settings_client::generate(std::string target) {
	try {
		if (target == "settings" || target.empty()) {
			get_core()->get()->save();
		} else if (target.empty()) {
			get_core()->get()->save();
		} else {
			get_core()->get()->save_to(expand_context(target));
		}
		return 0;
	} catch (settings::settings_exception e) {
		error_msg("Failed to initialize settings: " + e.reason());
		return 1;
	} catch (NSPluginException &e) {
		error_msg("Failed to load plugins: " + utf8::utf8_from_native(e.what()));
		return 1;
	} catch (std::exception &e) {
		error_msg("Failed to initialize settings: " + utf8::utf8_from_native(e.what()));
		return 1;
	} catch (...) {
		error_msg("FATAL ERROR IN SETTINGS SUBSYTEM");
		return 1;
	}
}


void nsclient_core::settings_client::switch_context(std::string contect) {
	get_core()->set_primary(expand_context(contect));
}

int nsclient_core::settings_client::set(std::string path, std::string key, std::string val) {
	settings::settings_core::key_type type = get_core()->get()->get_key_type(path, key);
	if (type == settings::settings_core::key_string) {
		get_core()->get()->set_string(path, key, val);
	} else if (type == settings::settings_core::key_integer) {
		get_core()->get()->set_int(path, key, strEx::s::stox<int>(val));
	} else if (type == settings::settings_core::key_bool) {
		get_core()->get()->set_bool(path, key, settings::settings_interface::string_to_bool(val));
	} else {
		error_msg("Failed to set key (not found)");
		return -1;
	}
	get_core()->get()->save();
	return 0;
}
void list_settings_context_info(int padding, settings::instance_ptr instance) {
	std::string pad = std::string(padding, ' ');
	std::cout << pad << instance->get_info() << std::endl;
	BOOST_FOREACH(settings::instance_ptr child, instance->get_children()) {
		list_settings_context_info(padding + 2, child);
	}
}

int nsclient_core::settings_client::show(std::string path, std::string key) {
	if (path.empty() && key.empty())
		list_settings_context_info(2, settings_manager::get_settings());
	else {
		settings::settings_interface::op_string val = get_core()->get()->get_string(path, key);
		if (val)
			std::cout << *val;
	}
	return 0;
}
int nsclient_core::settings_client::list(std::string path) {
	try {
		dump_path(path);
	} catch (settings::settings_exception e) {
		error_msg("Settings error: " + e.reason());
	} catch (...) {
		error_msg("FATAL ERROR IN SETTINGS SUBSYTEM");
	}

	return 0;
}
int nsclient_core::settings_client::validate() {
	settings::error_list errors = get_core()->validate();
	BOOST_FOREACH(const std::string &e, errors) {
		std::cerr << e << std::endl;
	}
	return 0;
}

void nsclient_core::settings_client::error_msg(std::string msg) {
	nsclient::logging::logger::get_logger()->error("client", __FILE__, __LINE__, msg.c_str());
}
void nsclient_core::settings_client::debug_msg(std::string msg) {
	nsclient::logging::logger::get_logger()->debug("client", __FILE__, __LINE__, msg.c_str());
}

void nsclient_core::settings_client::list_settings_info() {
	std::cout << "Current settings instance loaded: " << std::endl;
	list_settings_context_info(2, settings_manager::get_settings());
}
void nsclient_core::settings_client::activate(const std::string &module) 
{
	if (!core_->boot_load_plugin(module)) {
		std::cerr << "Failed to load module (Wont activate): " << module << std::endl;
	}
	core_->boot_start_plugins(false);
	get_core()->get()->set_string(MAIN_MODULES_SECTION, module, "enabled");
	if (default_) {
		get_core()->update_defaults();
	}
	get_core()->get()->save();
}
