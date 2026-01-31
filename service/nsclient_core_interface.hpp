#pragma once

#include <nsclient/logger/logger.hpp>

#include "path_manager.hpp"
#include "plugins/plugin_cache.hpp"
#include "plugins/plugin_manager.hpp"
#include "storage_manager.hpp"

namespace nsclient {
namespace core {
struct core_interface {
  virtual ~core_interface() = default;
  virtual logging::logger_instance get_logger() = 0;
  virtual plugin_mgr_instance get_plugin_manager() = 0;
  virtual path_instance get_path() = 0;
  virtual plugin_cache* get_plugin_cache() = 0;
  virtual storage_manager_instance get_storage_manager() = 0;
};
}  // namespace core
}  // namespace nsclient
