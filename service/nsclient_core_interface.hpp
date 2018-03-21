#pragma once

#include "plugin_manager.hpp"
#include "plugin_cache.hpp"
#include "path_manager.hpp"
#include "storage_manager.hpp"

#include <nsclient/logger/logger.hpp>

namespace nsclient {
	namespace core {
		struct core_interface {
 			virtual nsclient::logging::logger_instance get_logger() = 0;
			virtual nsclient::core::plugin_mgr_instance get_plugin_manager() = 0;
			virtual nsclient::core::path_instance get_path() = 0;
 			virtual nsclient::core::plugin_cache* get_plugin_cache() = 0;
			virtual nsclient::core::storage_manager_instance get_storage_manager() = 0;
		};
	}
}
