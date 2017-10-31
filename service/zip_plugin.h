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
#include "plugin_manager.hpp"

#include <utf8.hpp>

#include <NSCAPI.h>
#include <dll/dll.hpp>
#include <nsclient/logger/logger.hpp>

#include <boost/algorithm/string.hpp>

#include <string>
#include <list>
#include <set>

/**
 * @ingroup NSClient++
 * NSCPlugin is a wrapper class to wrap all DLL calls and make things simple and clean inside the actual application.<br>
 * Things tend to be one-to-one by which I mean that a call to a function here should call the corresponding function in the plug in (if loaded).
 * If things are "broken" NSPluginException is called to indicate this. Error states are returned for normal "conditions".
 *
 *
 * @version 1.0
 * first version
 *
 * @date 02-12-2005
 *
 * @author mickem
 *
 * @par license
 * This code is absolutely free to use and modify. The code is provided "as is" with
 * no expressed or implied warranty. The author accepts no liability if it causes
 * any damage to your computer, causes your pet to fall ill, increases baldness
 * or makes your car start emitting strange noises when you start it up.
 * This code has no bugs, just undocumented features!
 *
 * @todo
 * getVersion() is not implemented as of yet.
 *
 * @bug
 *
 */
namespace nsclient {
	namespace core {

		struct script_def {
			std::string provider;
			std::string script;
			std::string alias;
			std::string command;
		};

		class zip_plugin : public boost::noncopyable, public nsclient::core::plugin_interface {

			boost::filesystem::path file_;
			nsclient::core::path_instance paths_;
			nsclient::core::plugin_mgr_instance plugins_;
			nsclient::logging::logger_instance logger_;
			std::string name_;
			std::string description_;

			std::list<script_def> scripts_;
			std::set<std::string> modules_;
			std::list<std::string> on_start_;

		public:
			zip_plugin(const unsigned int id, const boost::filesystem::path file, std::string alias, nsclient::core::path_instance paths, nsclient::core::plugin_mgr_instance plugins, nsclient::logging::logger_instance logger);
			virtual ~zip_plugin();

			bool load_plugin(NSCAPI::moduleLoadMode mode);
			void unload_plugin();

			std::string getName();
			std::string getDescription();
			bool hasCommandHandler() { return false; }
			bool hasNotificationHandler() { return false; }
			bool hasMessageHandler() { return false; }
			NSCAPI::nagiosReturn handleCommand(const std::string request, std::string &reply);
			NSCAPI::nagiosReturn handle_schedule(const std::string &request);
			NSCAPI::nagiosReturn handleNotification(const char *channel, std::string &request, std::string &reply);
			bool has_on_event() { return false; }
			NSCAPI::nagiosReturn on_event(const std::string &request);
			NSCAPI::nagiosReturn fetchMetrics(std::string &request);
			NSCAPI::nagiosReturn submitMetrics(const std::string &request);
			void handleMessage(const char* data, unsigned int len);
			int commandLineExec(bool targeted, std::string &request, std::string &reply);
			bool has_command_line_exec() { return false; }
			bool is_duplicate(boost::filesystem::path file, std::string alias);

			bool has_routing_handler() { return false; }

			bool route_message(const char *channel, const char* buffer, unsigned int buffer_len, char **new_channel_buffer, char **new_buffer, unsigned int *new_buffer_len);

			bool hasMetricsFetcher() { return false; }
			bool hasMetricsSubmitter() { return false; }

			std::string getModule();

			void on_log_message(std::string &payload) {}
			std::string get_version();

		private:
			nsclient::logging::logger_instance get_logger() {
				return logger_;
			}
			void read_metadata();
			void read_metadata(std::string string);
		};
	}
}
