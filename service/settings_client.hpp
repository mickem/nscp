#pragma once
#include <string>

#include "NSClient++.h"

#include <settings/settings_core.hpp>

namespace nsclient_core {
	class settings_client {
		bool started_;
		NSClient* core_;
		bool default_;
		bool remove_default_;
		bool load_all_;
		bool use_samples_;

		settings::settings_core* get_core() const;

	public:
		settings_client(NSClient* core, bool update_defaults, bool remove_defaults, bool load_all, bool use_samples);

		~settings_client();

		void startup();

		void terminate();

		int migrate_from(std::string src);
		int migrate_to(std::string target);

		void dump_path(std::string root);

		int generate(std::string target);


		void switch_context(std::string contect);
		std::string expand_context(const std::string &key) const;

		int set(std::string path, std::string key, std::string val);
		int show(std::string path, std::string key);
		int list(std::string path);
		int validate();
		void error_msg(std::string msg);
		void debug_msg(std::string msg);

		void list_settings_info();
		void activate(const std::string &module);
	};
}
