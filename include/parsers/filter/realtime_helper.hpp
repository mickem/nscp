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

			typedef typename runtime_data::filter_type filter_type;
			typedef typename runtime_data::transient_data_type transient_data_type;
			typedef boost::optional<boost::posix_time::time_duration> op_duration;
			struct container {
				std::string alias;
				std::string target;
				std::string command;
				std::string timeout_msg;
				NSCAPI::nagiosReturn severity;
				runtime_data data;
				filter_type filter;
				boost::optional<boost::posix_time::time_duration> max_age;
				boost::posix_time::ptime next_ok_;

				void touch(const boost::posix_time::ptime &now) {
					if (max_age)
						next_ok_ = now + (*max_age);
					data.touch(now);
				}

				bool has_timedout(const boost::posix_time::ptime &now) const {
					return max_age && next_ok_ <= now;
				}

			};

			typedef boost::shared_ptr<container> container_type;

			std::list<container_type> items;

			bool add_item(const config_object &object, const runtime_data &source_data) {
				container_type item(new container);
				item->alias = object.tpl.alias;
				item->data = source_data;
				item->target = object.filter.target;
				item->command = item->alias;
				item->timeout_msg = object.filter.timeout_msg;
				item->severity = object.filter.severity;
				item->max_age = object.filter.max_age;
				if (!object.filter.command.empty())
					item->command = object.filter.command;
				std::string message;

				if (!item->filter.build_syntax(object.filter.syntax_top, object.filter.syntax_detail, object.filter.perf_data, message)) {
					NSC_LOG_ERROR("Failed to build strings " + object.tpl.alias + ": " + message);
					return false;
				}
				if (!item->filter.build_engines(object.filter.debug, object.filter.filter_string, object.filter.filter_ok, object.filter.filter_warn, object.filter.filter_crit)) {
					NSC_LOG_ERROR("Failed to build filters: " + object.tpl.alias);
					return false;
				}

				if (!item->filter.validate()) {
					NSC_LOG_ERROR("Failed validate: " + object.tpl.alias);
					return false;
				}
				item->data.boot();
				items.push_back(item);
				return true;
			}

			void process_timeout(const container_type item) {
				std::string response;
				if (!nscapi::core_helper::submit_simple_message(item->target, item->command, NSCAPI::returnOK, item->timeout_msg, "", response)) {
					NSC_LOG_ERROR("Failed to submit result: " + response);
				}
			}

			bool process_item(container_type item, transient_data_type data) {
				std::string response;
				item->filter.start_match();
				if (item->severity != -1)
					item->filter.returnCode = item->severity;

				if (!item->data.process_item(item->filter, data))
					return false;

				std::string message = item->filter.get_message();
				if (message.empty())
					message = "Nothing matched";
				if (!nscapi::core_helper::submit_simple_message(item->target, item->command, item->filter.returnCode, message, "", response)) {
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

					// Process all items matching this event
					BOOST_FOREACH(container_type item, items) {
						if (item->data.has_changed(data)) {
							if (process_item(item, data))
								item->touch(current_time);
						}
					}

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

			op_duration find_minimum_timeout() {
				op_duration ret;
				boost::posix_time::ptime current_time = boost::posix_time::second_clock::local_time();
				boost::posix_time::ptime minNext = current_time;;
				bool first = true;
				BOOST_FOREACH(const container_type item, items) {
					if (item->max_age && (first || item->next_ok_ < minNext) ) {
						first = false;
						minNext = item->next_ok_;
					}
				}

				boost::posix_time::time_duration dur;
				if (first) {
					NSC_DEBUG_MSG("Next miss time is in: no timeout specified");
				} else {
					boost::posix_time::time_duration dur = minNext - current_time;
					NSC_DEBUG_MSG("Next miss time is in: " + strEx::s::xtos(dur.total_seconds()) + "s");
					ret = dur;
				}
				return ret;
			}
		};
	}
}