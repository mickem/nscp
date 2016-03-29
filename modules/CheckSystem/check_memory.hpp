#pragma once

#include <nscapi/nscapi_protobuf.hpp>
#include "filter_config_object.hpp"

namespace memory_checks {

	namespace realtime {

		struct mem_filter_helper_wrapper;
		struct helper {
			mem_filter_helper_wrapper *memory_helper;

			helper(nscapi::core_wrapper *core, int plugin_id);
			void add_obj(boost::shared_ptr<filters::filter_config_object> object);
			void boot();
			void check();

		};
	}
	namespace memory {

		void check(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	}
}