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