#pragma once

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

#include <parsers/expression/expression.hpp>
#include <parsers/where/engine_impl.hpp>

#include <NSCAPI.h>
#include <nscapi/nscapi_helper.hpp>

namespace parsers {
	namespace where {

		template<class runtime_data, class config_object>
		struct realtime_filter_helper {

			typedef typename runtime_data::filter_type filter_type;
			typedef boost::optional<boost::posix_time::time_duration> op_duration;
			struct container {
				std::string alias;
				std::string target;
				std::string command;
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
					return max_age && next_ok_ < now;
				}

			};

			std::list<container> items;

			bool add_item(const config_object &object, const runtime_data &source_data) {
				container item;
				item.alias = object.alias;
				item.data = source_data;
				item.target = object.target;
				item.command = item.alias;
				item.severity = object.severity;
				if (!object.command.empty())
					item.command = object.command;
				std::string message;

				if (!item.filter.build_syntax(object.syntax_top, object.syntax_detail, object.perf_data, message)) {
					NSC_LOG_ERROR("Failed to build strings " + object.alias + ": " + message);
					return false;
				}
				if (!item.filter.build_engines(object.debug, object.filter_string, object.filter_ok, object.filter_warn, object.filter_crit)) {
					NSC_LOG_ERROR("Failed to build filters: " + object.alias);
					return false;
				}

				if (!item.filter.validate()) {
					NSC_LOG_ERROR("Failed validate: " + object.alias);
					return false;
				}
				item.data.boot();
				items.push_back(item);
				return true;
			}

			void process_timeout(const container &item) {
				std::string response;
				if (!nscapi::core_helper::submit_simple_message(item.target, item.command, NSCAPI::returnOK, "TODO" /*object.empty_msg*/, "", response)) {
					NSC_LOG_ERROR("Failed to submit result: " + response);
				}
			}

			void process_object(container &item) {
				std::string response;
				item.filter.start_match();
				if (item.severity != -1)
					item.filter.returnCode = item.severity;

				if (!item.data.process_item(item.filter))
					return;

				std::string message = item.filter.get_message();
				if (message.empty())
					message = "Nothing matched";
				if (!nscapi::core_helper::submit_simple_message(item.target, item.command, item.filter.returnCode, message, "", response)) {
					NSC_LOG_ERROR("Failed to submit '" + message);
				}
			}


			void touch_all() {
				boost::posix_time::ptime current_time = boost::posix_time::ptime();
				BOOST_FOREACH(container &item, items) {
					item.touch(current_time);
				}
			}

			void process_objects() {
				boost::posix_time::ptime current_time = boost::posix_time::ptime();

				// Process all items matching this event
				BOOST_FOREACH(container &item, items) {
					if (item.data.has_changed()) {
						process_object(item);
						item.touch(current_time);
					}
				}

				// Match any stale items and process timeouts
				BOOST_FOREACH(container &item, items) {
					if (item.has_timedout(current_time)) {
						process_timeout(item);
						item.touch(current_time);
					}
				}
			}

			op_duration find_minimum_timeout() {
				op_duration ret;
				boost::posix_time::ptime minNext;
				bool first = true;
				BOOST_FOREACH(const container &item, items) {
					if (item.max_age && (first || item.next_ok_ < minNext) ) {
						first = false;
						minNext = item.next_ok_;
					}
				}

				boost::posix_time::time_duration dur;
				if (first) {
					NSC_DEBUG_MSG("Next miss time is in: no timeout specified");
				} else {
					boost::posix_time::time_duration dur = minNext - boost::posix_time::ptime();
					NSC_DEBUG_MSG("Next miss time is in: " + strEx::s::xtos(dur.total_seconds()) + "s");
					ret = dur;
				}
				return ret;
			}
		};
	}
}