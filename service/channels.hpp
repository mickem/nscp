#pragma once

#include "NSCPlugin.h"
#include "plugin_list.hpp"

using namespace nscp::helpers;

namespace nsclient {
	struct channels : public plugins_list_with_listener {

		channels(nsclient::logger *logger) : plugins_list_with_listener(logger) {}

		void add_plugin(plugin_type plugin) {
			if (!plugin || !plugin->hasNotificationHandler())
				return;
			plugins_list_with_listener::add_plugin(plugin);
		}

	};
}