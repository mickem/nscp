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
		void error_msg(const char* file, const int line, std::string msg);
		void debug_msg(const char* file, const int line, std::string msg);

		void list_settings_info();
		void activate(const std::string &module);
	};
}