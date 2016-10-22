#pragma once

#include "nsclient_core_interface.hpp"

#include "commands.hpp"
#include "channels.hpp"
#include "routers.hpp"

#include <timer.hpp>
#include <nscapi/nscapi_protobuf.hpp>
#include <nsclient/logger/logger.hpp>


namespace nsclient {

	namespace core {

		class registry_query_handler {

		private:
			nsclient::logging::logger_instance logger_;
			const Plugin::RegistryRequestMessage &request_;
			core_interface *core_;
			timer t;

		public:
			registry_query_handler(nsclient::core::core_interface *core, const Plugin::RegistryRequestMessage &request);

			void parse(Plugin::RegistryResponseMessage &response);

			void parse_inventory(const Plugin::RegistryRequestMessage::Request::Inventory &q, Plugin::RegistryResponseMessage &response);
			void parse_registration(const Plugin::RegistryRequestMessage::Request::Registration &q, Plugin::RegistryResponseMessage &response);
			void parse_control(const Plugin::RegistryRequestMessage::Request::Control &q, Plugin::RegistryResponseMessage &response);


			nsclient::logging::logger_instance get_logger() const {
				return logger_;
			}


		};

	}
}
