#include "StdAfx.h"

#include "settings_handler_impl.hpp"


settings::instance_ptr settings::settings_handler_impl::get() {
	boost::unique_lock<boost::timed_mutex> mutex(instance_mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!mutex.owns_lock())
		throw settings_exception(_T("Failed to get mutex, cant get settings instance"));
	if (!instance_)
		throw settings_exception(_T("Failed initialize settings instance"));
	return instance_ptr(instance_);
}

settings::instance_ptr settings::settings_handler_impl::get_no_wait() {
	boost::unique_lock<boost::timed_mutex> mutex(instance_mutex_, boost::try_to_lock);
	if (!mutex.owns_lock())
		throw settings_exception(_T("Failed to get mutex, cant get settings instance"));
	if (!instance_)
		throw settings_exception(_T("Failed initialize settings instance"));
	return instance_;
}

void settings::settings_handler_impl::update_defaults() {
	BOOST_FOREACH(std::wstring path, get_reg_sections()) {
		get()->add_path(path);
		BOOST_FOREACH(std::wstring key, get_reg_keys(path)) {
			settings_core::key_description desc = get_registred_key(path, key);
			if (!desc.advanced) {
				if (!get()->has_key(path, key)) {
					get_logger()->debug(__FILE__, __LINE__, _T("Adding: ") + path + _T(".") + key);
					if (desc.type == key_string)
						get()->set_string(path, key, desc.defValue);
					else if (desc.type == key_bool)
						get()->set_bool(path, key, settings::settings_interface::string_to_bool(desc.defValue));
					else if (desc.type == key_integer) {
						try {
							get()->set_int(path, key, strEx::stoi(desc.defValue));
						} catch (const std::exception &e) {
							get_logger()->error(__FILE__, __LINE__, _T("invalid default value for: ") + path + _T(".") + key);
						}
					} else
						get_logger()->error(__FILE__, __LINE__, _T("Unknown keytype for: ") + path + _T(".") + key);
				} else {
					std::wstring val = get()->get_string(path, key);
					get_logger()->debug(__FILE__, __LINE__, _T("Setting old (already exists): ") + path + _T(".") + key + _T(" = ") + val);
					if (desc.type == key_string)
						get()->set_string(path, key, val);
					else if (desc.type == key_bool)
						get()->set_bool(path, key, settings::settings_interface::string_to_bool(val));
					else if (desc.type == key_integer)
						get()->set_int(path, key, strEx::stoi(val));
					else
						get_logger()->error(__FILE__, __LINE__, _T("Unknown keytype for: ") + path + _T(".") + key);
				}
			} else {
				get_logger()->debug(__FILE__, __LINE__, _T("Skipping (advanced): ") + path + _T(".") + key);
			}
		}
	}
	get_logger()->info(__FILE__, __LINE__, _T("DONE Updating settings with default values!"));
}


void settings::settings_handler_impl::destroy_all_instances() {
	boost::unique_lock<boost::timed_mutex> mutex(instance_mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!mutex.owns_lock())
		throw settings_exception(_T("destroy_all_instances Failed to get mutext, cant get access settings"));
	instance_.reset();
}


settings::error_list settings::settings_handler_impl::validate() {
	return get()->validate();
}
