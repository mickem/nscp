#pragma once

#include "NSCPlugin.h"
#include "plugin_list.hpp"

namespace nsclient {
	struct routers : plugins_list_with_listener {
		routers(nsclient::logging::logger_instance logger) : plugins_list_with_listener(logger) {}

		void add_plugin(plugin_type plugin) {
			if (!plugin || !plugin->has_routing_handler())
				return;
			plugins_list_with_listener::add_plugin(plugin);
		}
	};
}