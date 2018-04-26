/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "settings_client.hpp"

#ifdef HAVE_JSON_SPIRIT
#include <json_spirit.h>
#endif

#include "../libs/settings_manager/settings_manager_impl.h"

#include <config.h>

settings::settings_core* nsclient_core::settings_client::get_core() const {
	return settings_manager::get_core();
}

nsclient_core::settings_client::settings_client(NSClient* core, bool update_defaults, bool remove_defaults, bool load_all, bool use_samples)
	: started_(false), core_(core), default_(update_defaults), remove_default_(remove_defaults), load_all_(load_all), use_samples_(use_samples) {
	startup();
}

nsclient_core::settings_client::~settings_client() {
	terminate();
}

void nsclient_core::settings_client::startup() {
	if (started_)
		return;
	if (!core_->load_configuration(true)) {
		std::cout << "boot::init failed" << std::endl;
		return;
	}
	if (load_all_)
		core_->boot_load_all_plugin_files();

	if (!core_->boot_load_active_plugins()) {
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
	core_->stop_nsclient();
	started_ = false;
}

int nsclient_core::settings_client::migrate_from(std::string src) {
	try {
		debug_msg(__FILE__, __LINE__, "Migrating from: " + expand_context(src));
		get_core()->migrate_from("master", expand_context(src));
		return 1;
	} catch (settings::settings_exception e) {
		error_msg(__FILE__, __LINE__, "Failed to initialize settings: " + e.reason());
	} catch (...) {
		error_msg(__FILE__, __LINE__, "FATAL ERROR IN SETTINGS SUBSYTEM");
	}
	return -1;
}
int nsclient_core::settings_client::migrate_to(std::string target) {
	try {
		debug_msg(__FILE__, __LINE__, "Migrating to: " + expand_context(target));
		get_core()->migrate_to("master", expand_context(target));
		return 1;
	} catch (const settings::settings_exception &e) {
		error_msg(e.file(), e.line(), "Failed to initialize settings: " + e.reason());
	} catch (...) {
		error_msg(__FILE__, __LINE__, "FATAL ERROR IN SETTINGS SUBSYTEM");
	}
	return -1;
}

void nsclient_core::settings_client::dump_path(std::string root) {
	BOOST_FOREACH(const std::string &path, get_core()->get()->get_sections(root)) {
		if (!root.empty()) {
			dump_path(root + "/" + path);
		} else if (!path.empty()) {
			dump_path(path);
		}
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
			get_core()->get()->save_to("master", expand_context(target));
		}
		return 0;
	} catch (settings::settings_exception e) {
		error_msg(__FILE__, __LINE__, "Failed to initialize settings: " + e.reason());
		return 1;
	} catch (nsclient::core::plugin_exception &e) {
		error_msg(__FILE__, __LINE__, "Failed to load plugins: " + e.reason());
		return 1;
	} catch (std::exception &e) {
		error_msg(__FILE__, __LINE__, "Failed to initialize settings: " + utf8::utf8_from_native(e.what()));
		return 1;
	} catch (...) {
		error_msg(__FILE__, __LINE__, "FATAL ERROR IN SETTINGS SUBSYTEM");
		return 1;
	}
}

void nsclient_core::settings_client::switch_context(std::string contect) {
	get_core()->set_primary(expand_context(contect));
}

int nsclient_core::settings_client::set(std::string path, std::string key, std::string val) {
	get_core()->get()->set_string(path, key, val);
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
		error_msg(__FILE__, __LINE__, "Settings error: " + e.reason());
	} catch (...) {
		error_msg(__FILE__, __LINE__, "FATAL ERROR IN SETTINGS SUBSYTEM");
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

void nsclient_core::settings_client::error_msg(const char* file, const int line, std::string msg) {
	core_->get_logger()->error("client", file, line, msg.c_str());
}
void nsclient_core::settings_client::debug_msg(const char* file, const int line, std::string msg) {
	core_->get_logger()->debug("client", file, line, msg.c_str());
}

void nsclient_core::settings_client::list_settings_info() {
	std::cout << "Current settings instance loaded: " << std::endl;
	list_settings_context_info(2, settings_manager::get_settings());
}
void nsclient_core::settings_client::activate(const std::string &module) {
	if (!core_->boot_load_single_plugin(module)) {
		std::cerr << "Failed to load module (Wont activate): " << module << std::endl;
	}
	core_->boot_start_plugins(false);
	get_core()->get()->set_string(MAIN_MODULES_SECTION, module, "enabled");
	if (default_) {
		get_core()->update_defaults();
	}
	get_core()->get()->save();
}