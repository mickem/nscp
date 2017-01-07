#pragma once

#include "plugin_cache.hpp"
#include "commands.hpp"
#include "channels.hpp"
#include "routers.hpp"

#include <NSCAPI.h>
#include <nsclient/logger/logger.hpp>

#include <nscapi/nscapi_protobuf.hpp>

#include <boost/filesystem.hpp>

#include <string>

namespace nsclient {
	namespace core {
		struct core_interface {

			virtual std::string expand_path(std::string file) = 0;
			virtual nsclient::logging::logger_instance get_logger() = 0;

			virtual NSCAPI::nagiosReturn execute_query(const std::string &request, std::string &reply) = 0;
			virtual Plugin::QueryResponseMessage execute_query(const ::Plugin::QueryRequestMessage &req) = 0;
			
			virtual void load_plugin(const boost::filesystem::path &file, std::string alias) = 0;
			virtual void remove_plugin(const std::string name) = 0;
			virtual unsigned int add_plugin(unsigned int plugin_id) = 0;

			virtual void register_command_alias(unsigned long plugin_id, std::string cmd, std::string desc) = 0;

			virtual nsclient::core::plugin_cache* get_plugin_cache() = 0;

			virtual nsclient::commands* get_commands() = 0;
			virtual nsclient::channels* get_channels() = 0;
			virtual nsclient::event_subscribers* get_event_subscribers() = 0;



		};
	}
}
