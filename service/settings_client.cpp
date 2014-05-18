#include "settings_client.hpp"


#include <settings/settings_core.hpp>
#include <nsclient/logger.hpp>
#ifdef HAVE_JSON_SPIRIT
#include <json_spirit.h>
#endif
#include "NSClient++.h"

#include "../helpers/settings_manager/settings_manager_impl.h"

settings::settings_core* get_core() {
	return settings_manager::get_core();
}

nsclient_core::settings_client::settings_client(NSClient* core, bool update_defaults, bool remove_defaults, bool load_all, bool use_samples, std::string filter) 
	: started_(false), core_(core), default_(update_defaults), remove_default_(remove_defaults), load_all_(load_all), use_samples_(use_samples), filter_(filter) 
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
		debug_msg("Migrating from: " + src);
		get_core()->migrate_from(src);
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
		debug_msg("Migrating to: " + target);
		get_core()->migrate_to(target);
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
		std::cout << root << "." << key << "=" << get_core()->get()->get_string(root, key) << std::endl;
	}
}

bool nsclient_core::settings_client::match_filter(std::string name) {
	return filter_.empty() || name.find(filter_) != std::string::npos; 
}


int nsclient_core::settings_client::generate(std::string target) {
	try {
		if (target == "settings" || target.empty()) {
			get_core()->get()->save();
		} else if (target.empty()) {
			get_core()->get()->save();
#ifdef HAVE_JSON_SPIRIT
		} else if (target == "json" || target == "json-compact") {
			json_spirit::Object json_root;
			// TODO, allow samples to be generated
			settings::settings_interface::string_list s = get_core()->get_reg_sections(use_samples_);
			BOOST_FOREACH(const std::string &path, s) {

				settings::settings_core::path_description desc = get_core()->get_registred_path(path);
				bool include = filter_.empty();
				json_spirit::Object json_plugins;
				BOOST_FOREACH(unsigned int i, desc.plugins) {
					std::string name = core_->get_plugin_module_name(i);
					if (match_filter(name))
						include = true;
					json_plugins.insert(json_spirit::Object::value_type(strEx::s::xtos(i), name));
				}
				if (!include)
					continue;

				json_spirit::Object json_path;
				json_path.insert(json_spirit::Object::value_type("path", path));
				json_path.insert(json_spirit::Object::value_type("title", desc.title));
				json_path.insert(json_spirit::Object::value_type("description", desc.description));
				json_path.insert(json_spirit::Object::value_type("plugins", json_plugins));
				json_path.insert(json_spirit::Object::value_type("advanced", desc.advanced));
				if (use_samples_)
					json_path.insert(json_spirit::Object::value_type("sample", desc.is_sample));

				json_spirit::Object json_keys;
				BOOST_FOREACH(const std::string &key, get_core()->get_reg_keys(path, use_samples_)) {
					settings::settings_core::key_description desc = get_core()->get_registred_key(path, key);
					json_spirit::Object json_key;
					json_key.insert(json_spirit::Object::value_type("key", key));
					json_key.insert(json_spirit::Object::value_type("title", desc.title));
					json_key.insert(json_spirit::Object::value_type("description", desc.description));
					json_key.insert(json_spirit::Object::value_type("default value", desc.defValue));
					json_key.insert(json_spirit::Object::value_type("advanced", desc.advanced));
					if (use_samples_)
						json_key.insert(json_spirit::Object::value_type("sample", desc.is_sample));
					json_keys.insert(json_spirit::Object::value_type(key, json_key));
				}
				json_path.insert(json_spirit::Object::value_type("keys", json_keys));
				json_root.insert(json_spirit::Object::value_type(path, json_path));
			}
			if (target == "json-compact")
				write(json_root, std::cout);
			else
				write(json_root, std::cout, json_spirit::pretty_print);
#endif
		} else {
			get_core()->get()->save_to(target);
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
	get_core()->set_primary(contect);
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
int nsclient_core::settings_client::show(std::string path, std::string key) {
	std::cout << get_core()->get()->get_string(path, key);
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

void list_settings_context_info(int padding, settings::instance_ptr instance) {
	std::string pad = std::string(padding, ' ');
	std::cout << pad << instance->get_info() << std::endl;
	BOOST_FOREACH(settings::instance_ptr child, instance->get_children()) {
		list_settings_context_info(padding+2, child);
	}
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
	get_core()->get()->set_string("/modules", module, "enabled");
	if (default_) {
		get_core()->update_defaults();
	}
	get_core()->get()->save();
}
