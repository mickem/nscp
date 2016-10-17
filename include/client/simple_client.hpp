/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <boost/shared_ptr.hpp>

#include <nscapi/log_handler.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <metrics/metrics_store_map.hpp>

namespace client {
	struct cli_handler : public nscapi::log_handler {
		virtual void output_message(const std::string &msg) = 0;
		virtual int get_plugin_id() const = 0;
		virtual const nscapi::core_wrapper* get_core() const = 0;
	};
	class cli_client {
		typedef boost::shared_ptr<cli_handler> cli_handler_ptr;
		cli_handler_ptr handler;
		metrics::metrics_store metrics_store;
		

	public:
		cli_client(cli_handler_ptr handler) : handler(handler) {}
		void handle_command(const std::string &command);
		void push_metrics(const Plugin::MetricsMessage &response);
	};
	typedef boost::shared_ptr<cli_handler> cli_handler_ptr;
}