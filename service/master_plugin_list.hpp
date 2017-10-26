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

#include "plugin_interface.hpp"

#include <nsclient/logger/logger.hpp>


#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

namespace nsclient {
	namespace core {
		class master_plugin_list {
		public:
			typedef boost::shared_ptr<nsclient::core::plugin_interface> plugin_type;
		private:

			typedef std::vector<plugin_type> pluginList;
			pluginList plugins_;
			boost::shared_mutex m_mutexRW;
			unsigned int next_plugin_id_;
			nsclient::logging::logger_instance log_instance_;

		public:
			master_plugin_list(nsclient::logging::logger_instance log_instance);
			virtual ~master_plugin_list();

			void append_plugin(plugin_type plugin);
			void remove(const int id);
			void clear();

			std::list<plugin_type> get_plugins();

			plugin_type find_by_module(std::string module);
			plugin_type find_by_alias(const std::string module);
			plugin_type find_by_id(const unsigned int plugin_id);

			plugin_type find_duplicate(boost::filesystem::path file, std::string alias);

			unsigned int get_next_id() {
				return next_plugin_id_++;
			}

		private:
			nsclient::logging::logger_instance get_logger() {
				return log_instance_;
			}

		};
	}
}

