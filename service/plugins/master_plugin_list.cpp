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

#include "master_plugin_list.hpp"

#include <boost/filesystem/path.hpp>
#include <boost/thread/mutex.hpp>
#include <str/xtos.hpp>

nsclient::core::master_plugin_list::master_plugin_list(logging::log_client_accessor log_instance) : next_plugin_id_(0), log_instance_(log_instance) {}

nsclient::core::master_plugin_list::~master_plugin_list() = default;

void nsclient::core::master_plugin_list::append_plugin(plugin_type plugin) {
  const boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(10));
  if (!writeLock.owns_lock()) {
    LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
    return;
  }
  plugins_.insert(plugins_.end(), plugin);
}

void nsclient::core::master_plugin_list::remove(std::size_t id) {
  const boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!writeLock.owns_lock()) {
    LOG_ERROR_CORE("FATAL ERROR: Could not get write-mutex.");
    return;
  }
  for (auto it = plugins_.begin(); it != plugins_.end();) {
    if ((*it)->get_id() == id) {
      it = plugins_.erase(it);
    } else {
      ++it;
    }
  }
}

void nsclient::core::master_plugin_list::clear() {
  const boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!writeLock.owns_lock()) {
    LOG_ERROR_CORE("FATAL ERROR: Could not get write-mutex.");
    return;
  }
  plugins_.clear();
}

std::list<nsclient::core::master_plugin_list::plugin_type> nsclient::core::master_plugin_list::get_plugins() {
  std::list<plugin_type> ret;
  const boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
  if (!readLock.owns_lock()) {
    LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
    return ret;
  }
  for (plugin_type &plugin : plugins_) {
    ret.push_back(plugin);
  }
  return ret;
}

nsclient::core::master_plugin_list::plugin_type nsclient::core::master_plugin_list::find_by_module(std::string module) {
  if (module.empty()) {
    return {};
  }
  const boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!readLock.owns_lock()) {
    LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
    return {};
  }
  for (plugin_type plugin : plugins_) {
    if (plugin && (plugin->getModule() == module)) {
      return plugin;
    }
  }
  return {};
}

nsclient::core::master_plugin_list::plugin_type nsclient::core::master_plugin_list::find_by_alias(const std::string alias) {
  if (alias.empty()) {
    return {};
  }
  const boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!readLock.owns_lock()) {
    LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
    return {};
  }
  for (plugin_type plugin : plugins_) {
    if (plugin && (plugin->get_alias_or_name() == alias)) {
      return plugin;
    }
  }
  return {};
}

nsclient::core::master_plugin_list::plugin_type nsclient::core::master_plugin_list::find_by_id(const unsigned int plugin_id) {
  const boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
  if (!readLock.owns_lock()) {
    LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
    return {};
  }
  for (plugin_type plugin : plugins_) {
    if (plugin->get_id() == plugin_id) return plugin;
  }
  return {};
}

nsclient::core::master_plugin_list::plugin_type nsclient::core::master_plugin_list::find_duplicate(boost::filesystem::path file, std::string alias) {
  const boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(10));
  if (!writeLock.owns_lock()) {
    LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
    return {};
  }

  for (plugin_type plug : plugins_) {
    if (plug->is_duplicate(file, alias)) {
      LOG_DEBUG_CORE_STD("Found duplicate plugin returning old " + str::xtos(plug->get_id()));
      return plug;
    }
  }
  return {};
}
