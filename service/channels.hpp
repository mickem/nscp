// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include "plugins/plugin_list.hpp"

namespace nsclient {
struct channels : public plugins_list_with_listener {
  channels(nsclient::logging::logger_instance logger) : plugins_list_with_listener(logger) {}

  void add_plugin(plugin_type plugin) {
    if (!plugin || !plugin->hasNotificationHandler()) return;
    plugins_list_with_listener::add_plugin(plugin);
  }
};
struct event_subscribers : public plugins_list_with_listener {
  event_subscribers(nsclient::logging::logger_instance logger) : plugins_list_with_listener(logger) {}

  void add_plugin(plugin_type plugin) {
    if (!plugin || !plugin->has_on_event()) return;
    plugins_list_with_listener::add_plugin(plugin);
  }
};
}  // namespace nsclient