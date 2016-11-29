#pragma once

#include <boost/shared_ptr.hpp>

#include <settings/settings_core.hpp>
#include <settings/client/settings_client_interface.hpp>
#include "settings_handler_impl.hpp"

namespace settings_manager {
	struct provider_interface {
		virtual std::string expand_path(std::string file) = 0;
		virtual nsclient::logging::logger_instance get_logger() const = 0;
	};

	class NSCSettingsImpl : public settings::settings_handler_impl {
	private:
		boost::filesystem::path boot_;
		provider_interface *provider_;
	public:
		NSCSettingsImpl(provider_interface *provider) : settings::settings_handler_impl(provider->get_logger()), provider_(provider) {}
		virtual ~NSCSettingsImpl() {}

		std::string expand_simple_context(const std::string &key);
		void boot(std::string file);
		std::string find_file(std::string file, std::string fallback = "");
		std::string expand_path(std::string file);
		std::string expand_context(const std::string &key);

		settings::instance_raw_ptr create_instance(std::string alias, std::string key);
		void change_context(std::string file);
		bool context_exists(std::string key);
		bool create_context(std::string key);
		bool has_boot_conf();
		void set_primary(std::string key);
		bool supports_edit(const std::string key);
	};

	// Alias to make handling "compatible" with old syntax
	settings::instance_ptr get_settings();
	settings::instance_ptr get_settings_no_wait();
	settings::settings_core* get_core();
	boost::shared_ptr<nscapi::settings_helper::settings_impl_interface>  get_proxy();
	void destroy_settings();
	bool init_settings(provider_interface *provider, std::string context = "");
	bool init_installer_settings(provider_interface *provider, std::string context = "");
	void change_context(std::string context);
	bool has_boot_conf();
	bool context_exists(std::string key);
	bool create_context(std::string key);
}
