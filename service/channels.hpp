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
}