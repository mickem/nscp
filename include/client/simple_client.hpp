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