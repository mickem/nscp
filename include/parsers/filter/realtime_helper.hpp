#pragma once

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

#include <parsers/expression/expression.hpp>
#include <parsers/where/engine_impl.hpp>

#include <NSCAPI.h>
#include <nscapi/nscapi_helper.hpp>
#include <nscapi/nscapi_core_helper.hpp>

namespace parsers {
	namespace where {
		template<class runtime_data, class config_object>
		struct realtime_filter_helper {
			nscapi::core_wrapper *core;
			int plugin_id;
			realtime_filter_helper(nscapi::core_wrapper *core, int plugin_id) : core(core), plugin_id(plugin_id) {}

			typedef typename runtime_data::filter_type filter_type;
			typedef typename runtime_data::transient_data_type transient_data_type;
			typedef boost::optional<boost::posix_time::time_duration> op_duration;
			struct container {
				std::string alias;
				std::string target;
				std::string target_id;
				std::string source_id;
				std::string command;
				std::string timeout_msg;
				NSCAPI::nagiosReturn severity;
				runtime_data data;
				filter_type filter;
				boost::optional<boost::posix_time::time_duration> max_age;
				boost::posix_time::ptime next_ok_;
				bool debug;
				bool escape_html;
				container() : debug(false), escape_html(false) {}

				void touch(const boost::posix_time::ptime &now) {
					if (max_age)
						next_ok_ = now + (*max_age);
					data.touch(now);
				}

				bool has_timedout(const boost::posix_time::ptime &now) const {
					return max_age && next_ok_ <= now;
				}

				bool find_minimum_timeout(boost::optional<boost::posix_time::ptime> &minNext) const {
					if (!max_age)
						return false;
					if (!minNext)				// No value yes, lest assign ours.
						minNext = next_ok_;
					if (next_ok_ > *minNext)	// Our value is not interesting: lets ignore us
						return false;
					minNext = next_ok_;
					return true;
				}
			};

			typedef boost::shared_ptr<container> container_type;

			std::list<container_type> items;

			bool add_item(const boost::shared_ptr<config_object> object, const runtime_data &source_data) {
				container_type item(new container);
				item->alias = object->get_alias();
				item->data = source_data;
				item->target = object->filter.target;
				item->target_id = object->filter.target_id;
				item->source_id = object->filter.source_id;
				item->command = item->alias;
				item->timeout_msg = object->filter.timeout_msg;
				item->severity = object->filter.severity;
				item->max_age = object->filter.max_age;
				item->debug = object->filter.debug;
				item->escape_html = object->filter.escape_html;
				if (!object->filter.command.empty())
					item->command = object->filter.command;
				std::string message;

				if (!item->filter.build_syntax(object->filter.syntax_top, object->filter.syntax_detail, object->filter.perf_data, object->filter.perf_config, object->filter.syntax_ok, object->filter.syntax_empty, message)) {
					NSC_LOG_ERROR("Failed to build strings " + object->get_alias() + ": " + message);
					return false;
				}
				if (!item->filter.build_engines(object->filter.debug, object->filter.filter_string, object->filter.filter_ok, object->filter.filter_warn, object->filter.filter_crit)) {
					NSC_LOG_ERROR("Failed to build filters: " + object->get_alias());
					return false;
				}

				if (!item->filter.validate()) {
					NSC_LOG_ERROR("Failed validate: " + object->get_alias());
					return false;
				}
				item->data.boot();
				items.push_back(item);
				return true;
			}

			void process_timeout(const container_type item) {
				std::string response;
				nscapi::core_helper ch(core, plugin_id);
				if (!ch.submit_simple_message(item->target, item->source_id, item->target_id, item->command, NSCAPI::query_return_codes::returnOK, item->timeout_msg, "", response)) {
					NSC_LOG_ERROR("Failed to submit result: " + response);
				}
			}

			bool process_item(container_type item, transient_data_type data) {
				std::string response;
				item->filter.start_match();
				if (item->severity != -1)
					item->filter.summary.returnCode = item->severity;

				if (!item->data.process_item(item->filter, data)) {
					return false;
				}

				nscapi::core_helper ch(core, plugin_id);
				std::string message = item->filter.get_message();
				if (message.empty())
					message = "Nothing matched";
				if (!ch.submit_simple_message(item->target, item->source_id, item->target_id, item->command, item->filter.summary.returnCode, message, "", response)) {
					NSC_LOG_ERROR("Failed to submit '" + message);
				}
				return true;
			}

			void touch_all() {
				boost::posix_time::ptime current_time = boost::posix_time::second_clock::local_time();
				BOOST_FOREACH(container_type item, items) {
					item->touch(current_time);
				}
			}

			void process_items(transient_data_type data) {
				try {
					boost::posix_time::ptime current_time = boost::posix_time::second_clock::local_time();
					bool has_matched = false;
					// Process all items matching this event
					BOOST_FOREACH(container_type item, items) {
						if (item->data.has_changed(data)) {
							if (process_item(item, data)) {
								has_matched = true;
								item->touch(current_time);
							}
						}
					}
					if (!has_matched)
						NSC_DEBUG_MSG("No filters matched an event");
					do_process_no_items(current_time);
				} catch (...) {
					NSC_DEBUG_MSG("Realtime processing faillure");
				}
			}

			void do_process_no_items(boost::posix_time::ptime current_time) {
				try {
					// Match any stale items and process timeouts
					BOOST_FOREACH(container_type item, items) {
						if (item->has_timedout(current_time)) {
							process_timeout(item);
							item->touch(current_time);
						}
					}
				} catch (...) {
					NSC_DEBUG_MSG("Realtime processing faillure");
				}
			}

			void process_no_items() {
				do_process_no_items(boost::posix_time::second_clock::local_time());
			}

			op_duration find_minimum_timeout() {
				op_duration ret;
				boost::posix_time::ptime current_time = boost::posix_time::second_clock::local_time();
				boost::optional<boost::posix_time::ptime> minNext;
				BOOST_FOREACH(const container_type item, items) {
					item->find_minimum_timeout(minNext);
				}

				boost::posix_time::time_duration dur;
				if (!minNext) {
					NSC_DEBUG_MSG("Next miss time is in: no timeout specified");
				} else {
					boost::posix_time::time_duration dur = *minNext - current_time;
					if (dur.total_seconds() <= 0) {
						NSC_LOG_ERROR("Invalid duration for eventlog check, assuming all values stale");
						touch_all();
						minNext = boost::none;
						BOOST_FOREACH(const container_type item, items) {
							item->find_minimum_timeout(minNext);
						}
						dur = *minNext - current_time;
						if (dur.total_seconds() <= 0) {
							NSC_LOG_ERROR("Something is fishy with your periods, returning 30 seconds...");
							dur = boost::posix_time::time_duration(0, 0, 30, 0);
						}
					}
					NSC_DEBUG_MSG("Next miss time is in: " + strEx::s::xtos(dur.total_seconds()) + "s");
					ret = dur;
				}
				return ret;
			}
		};
	}
}