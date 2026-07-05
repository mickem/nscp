// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/shared_mutex.hpp>
#include <list>
#include <memory>
#include <nsclient/logger/logger.hpp>
#include <vector>

#include "plugin_interface.hpp"

namespace nsclient {
namespace core {
class master_plugin_list {
 public:
  typedef std::shared_ptr<plugin_interface> plugin_type;

 private:
  typedef std::vector<plugin_type> pluginList;
  pluginList plugins_;
  boost::shared_mutex m_mutexRW;
  unsigned int next_plugin_id_;
  logging::log_client_accessor log_instance_;

 public:
  master_plugin_list(logging::log_client_accessor log_instance);
  virtual ~master_plugin_list();

  void append_plugin(plugin_type plugin);
  void remove(std::size_t id);
  void clear();

  std::list<plugin_type> get_plugins();

  plugin_type find_by_module(std::string module);
  plugin_type find_by_alias(std::string module);
  plugin_type find_by_id(unsigned int plugin_id);

  plugin_type find_duplicate(boost::filesystem::path file, std::string alias);

  unsigned int get_next_id() { return next_plugin_id_++; }

 private:
  logging::log_client_accessor get_logger() { return log_instance_; }
};
}  // namespace core
}  // namespace nsclient
