#pragma once

#include "NSCPlugin.h"
#include "plugin_list.hpp"

using namespace nscp::helpers;

namespace nsclient {
	struct routers : plugins_list_with_listener {

		routers(nsclient::logger *logger) : plugins_list_with_listener(logger) {}

		void add_plugin(plugin_type plugin) {
			if (!plugin || !plugin->has_routing_handler())
				return;
			plugins_list::add_plugin(plugin);
		}
	};
}