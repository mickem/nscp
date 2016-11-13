/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "NSCPlugin.h"
#include "plugin_list.hpp"

namespace nsclient {
	struct channels : public plugins_list_with_listener {
		channels(nsclient::logging::logger_instance logger) : plugins_list_with_listener(logger) {}

		void add_plugin(plugin_type plugin) {
			if (!plugin || !plugin->hasNotificationHandler())
				return;
			plugins_list_with_listener::add_plugin(plugin);
		}
	};
	struct event_subscribers : public plugins_list_with_listener {
		event_subscribers(nsclient::logging::logger_instance logger) : plugins_list_with_listener(logger) {}

		void add_plugin(plugin_type plugin) {
			if (!plugin || !plugin->has_on_event())
				return;
			plugins_list_with_listener::add_plugin(plugin);
		}
	};
}