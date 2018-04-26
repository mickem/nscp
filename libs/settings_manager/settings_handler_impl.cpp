#include "settings_handler_impl.hpp"

#include <str/xtos.hpp>

settings::instance_ptr settings::settings_handler_impl::get() {
	boost::unique_lock<boost::timed_mutex> mutex(instance_mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!mutex.owns_lock())
		throw settings_exception(__FILE__, __LINE__, "Failed to get mutex, cant get settings instance");
	if (!instance_)
		throw settings_exception(__FILE__, __LINE__, "Failed initialize settings instance");
	return instance_ptr(instance_);
}

settings::instance_ptr settings::settings_handler_impl::get_no_wait() {
	boost::unique_lock<boost::timed_mutex> mutex(instance_mutex_, boost::try_to_lock);
	if (!mutex.owns_lock())
		throw settings_exception(__FILE__, __LINE__, "Failed to get mutex, cant get settings instance");
	if (!instance_)
		throw settings_exception(__FILE__, __LINE__, "Failed initialize settings instance");
	return instance_;
}

void settings::settings_handler_impl::update_defaults() {
	BOOST_FOREACH(const std::string &path, get_reg_sections("", false)) {
		get()->add_path(path);
		BOOST_FOREACH(const std::string &key, get_reg_keys(path, false)) {
			settings_core::key_description desc = get_registred_key(path, key);
			if (!desc.advanced) {
				if (!get()->has_key(path, key)) {
					get_logger()->debug("settings", __FILE__, __LINE__, "Adding: " + key_to_string(path, key));
					get()->set_string(path, key, desc.default_value);
				} else {
					settings_interface::op_string val = get()->get_string(path, key);
					if (val) {
						get_logger()->debug("settings", __FILE__, __LINE__, "Setting old (already exists): " + key_to_string(path, key));
						get()->set_string(path, key, *val);
					}
				}
			} else {
				get_logger()->debug("settings", __FILE__, __LINE__, "Skipping (advanced): " + key_to_string(path, key));
			}
		}
	}
}

void settings::settings_handler_impl::remove_defaults() {
	BOOST_FOREACH(std::string path, get_reg_sections("", false)) {
		BOOST_FOREACH(std::string key, get_reg_keys(path, false)) {
			settings_core::key_description desc = get_registred_key(path, key);
			if (get()->has_key(path, key)) {
				try {
					if (get()->get_string(path, key) == desc.default_value) {
						get()->remove_key(path, key);
					}
				} catch (const std::exception &) {
					get_logger()->error("settings", __FILE__, __LINE__, "invalid default value for: " + key_to_string(path, key));
				}
			}
		}
		if (get()->get_keys(path).size() == 0 && get()->get_sections(path).size() == 0) {
			get()->remove_path(path);
		}
	}
}

void settings::settings_handler_impl::destroy_all_instances() {
	boost::unique_lock<boost::timed_mutex> mutex(instance_mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!mutex.owns_lock())
		throw settings_exception(__FILE__, __LINE__, "destroy_all_instances Failed to get mutex, cant get access settings");
	instance_.reset();
}

void settings::settings_handler_impl::house_keeping() {
	boost::unique_lock<boost::timed_mutex> mutex(instance_mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!mutex.owns_lock())
		throw settings_exception(__FILE__, __LINE__, "destroy_all_instances Failed to get mutex, cant get access settings");
	instance_->house_keeping();
}

settings::error_list settings::settings_handler_impl::validate() {
	return get()->validate();
}