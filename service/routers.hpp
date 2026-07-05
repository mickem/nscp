// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include "plugins/plugin_list.hpp"

namespace nsclient {
struct routers : plugins_list_with_listener {
  routers(logging::logger_instance logger) : plugins_list_with_listener(logger) {}

  void add_plugin(plugin_type plugin) {
    if (!plugin || !plugin->has_routing_handler()) return;
    plugins_list_with_listener::add_plugin(plugin);
  }
};
}  // namespace nsclient