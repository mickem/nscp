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


#include <parsers/expression/expression.hpp>
#include <parsers/where/engine_impl.hpp>

#include <NSCAPI.h>
#include <nscapi/nscapi_helper.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

#include <nsclient/nsclient_exception.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>


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
			private:
				std::string alias;
				std::string event_name;
				std::string target;
				std::string target_id;
				std::string source_id;
			public:
				std::string command;
				std::string timeout_msg;
				NSCAPI::nagiosReturn severity;
				boost::optional<boost::posix_time::time_duration> max_age;
				boost::optional<boost::posix_time::time_duration> silent_period;

				// should be private...
				filter_type filter;

			private:
				boost::posix_time::ptime next_ok_;
				boost::posix_time::ptime next_alert_;
			public:
				bool debug;
				bool escape_html;
				runtime_data data;

				container(std::string alias, std::string event_name, runtime_data data) : alias(alias), event_name(event_name), command(alias), debug(false), escape_html(false), data(data) {}

				void set_target(std::string new_target, std::string new_target_id, std::string new_source_id) {
					target = new_target;
					target_id = new_target_id;
					source_id = new_source_id;
				}
				bool is_event() const {
					return target == "event";
				}
				bool is_events() const {
					return target == "events";
				}

				std::string get_event_name() const {
					return event_name;
				}
				std::string get_alias() const {
					return alias;
				}
				std::string get_target() const {
					return target;
				}
				std::string get_target_id() const {
					return target_id;
				}
				std::string get_source_id() const {
					return source_id;
				}

				bool build_filters(nscapi::settings_filters::filter_object config, std::string &error) {
					std::string message;
					if (!filter.build_syntax(config.debug, config.syntax_top, config.syntax_detail, config.perf_data, config.perf_config, config.syntax_ok, config.syntax_empty, message)) {
						error = "Failed to build strings " + alias + ": " + message;
						return false;
					}
					if (!filter.build_engines(config.debug, config.filter_string(), config.filter_ok, config.filter_warn, config.filter_crit)) {
						error = "Failed to build filters: " + alias;
						return false;
					}

					if (!filter.validate(message)) {
						error = "Failed to validate filter for " + alias + ": " + message;
						return false;
					}
					return true;
				}

				bool is_silent(const boost::posix_time::ptime &now) {
					if (!silent_period) {
						return false;
					}
					return now < next_alert_;
				}

				void touch(const boost::posix_time::ptime &now, bool alert) {
					if (max_age)
						next_ok_ = now + (*max_age);
					if (alert && silent_period)
						next_alert_ = now + (*silent_period);
					else if (silent_period)
						next_alert_ = now;
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

			bool add_item(const boost::shared_ptr<config_object> object, const runtime_data &source_data, const std::string event_name) {
				container_type item(new container(object->get_alias(), event_name, source_data));
				item->set_target(object->filter.target, object->filter.target_id, object->filter.source_id);
				item->timeout_msg = object->filter.timeout_msg;
				item->severity = object->filter.severity;
				item->max_age = object->filter.max_age;
				item->silent_period = object->filter.silent_period;
				item->debug = object->filter.debug;
				item->escape_html = object->filter.escape_html;
				if (!object->filter.command.empty())
					item->command = object->filter.command;
				std::string message;

				if (!item->build_filters(object->filter, message)) {
					NSC_LOG_ERROR(message);
					return false;
				}

				item->data.boot();
				items.push_back(item);
				return true;
			}

			void process_timeout(const container_type item) {
				std::string response;
				nscapi::core_helper ch(core, plugin_id);
				if (!ch.submit_simple_message(item->get_target(), item->get_source_id(), item->get_target_id(), item->command, NSCAPI::query_return_codes::returnOK, item->timeout_msg, "", response)) {
					NSC_LOG_ERROR("Failed to submit result: " + response);
				}
			}

			bool process_item(container_type item, transient_data_type data, bool is_silent) {
				std::string response;
				if (item->is_events() || item->is_event()) {
					item->filter.fetch_hash(true);
				}
				item->filter.start_match();
				if (item->severity != -1)
					item->filter.summary.returnCode = item->severity;

				modern_filter::match_result result = item->data.process_item(item->filter, data);
				if (!result.matched_filter) {
					return false;
				}

				if (is_silent) {
					NSC_TRACE_MSG("Eventlog filter is silenced " + item->get_alias());
					return true;
				}

				nscapi::core_helper ch(core, plugin_id);
				if (item->is_event()) {
					typedef std::list<std::map<std::string, std::string> > list_type;
					typedef std::map<std::string, std::string> hash_type;

					list_type keys = item->filter.records_;
					BOOST_FOREACH(hash_type &bundle, keys) {
						if (!ch.emit_event(item->get_event_name(), item->get_alias(), bundle, response)) {
							NSC_LOG_ERROR("Failed to submit '" + response);
						}
					}
					return true;
				}
				if (item->is_events()) {
					std::list<std::map<std::string, std::string> > keys = item->filter.records_;
					if (!ch.emit_event(item->get_event_name(), item->get_alias(), keys, response)) {
						NSC_LOG_ERROR("Failed to submit '" + response);
					}
					return true;
				}

				std::string message = item->filter.get_message();
				if (message.empty())
					message = "Nothing matched";
				if (!ch.submit_simple_message(item->get_target(), item->get_source_id(), item->get_target_id(), item->command, item->filter.summary.returnCode, message, "", response)) {
					NSC_LOG_ERROR("Failed to submit '" + message);
				}
				return true;
			}

			void touch_all() {
				boost::posix_time::ptime current_time = boost::posix_time::second_clock::local_time();
				BOOST_FOREACH(container_type item, items) {
					item->touch(current_time, false);
				}
			}

			void process_items(transient_data_type data) {
				try {
					boost::posix_time::ptime current_time = boost::posix_time::second_clock::local_time();
					bool has_matched = false;
					bool has_changed = false;
					// Process all items matching this event
					if (items.size() == 0) {
						NSC_TRACE_MSG("No filters to check for: " + data->to_string());
					}
					BOOST_FOREACH(container_type item, items) {
						if (item->data.has_changed(data)) {
							has_changed = true;
							if (process_item(item, data, item->is_silent(current_time))) {
								has_matched = true;
								item->touch(current_time, true);
							}
						}
					}
					if (!has_changed) {
						NSC_TRACE_MSG("No filters changes detected: " + data->to_string());
					} else if (!has_matched) {
						NSC_TRACE_MSG("No filters matched: " + data->to_string());
					}
					do_process_no_items(current_time);
				} catch (const nsclient::nsclient_exception &e) {
					NSC_DEBUG_MSG("Realtime processing faillure: " + e.reason());
				} catch (const std::exception &e) {
					NSC_DEBUG_MSG("Realtime processing faillure: " + utf8::utf8_from_native(e.what()));
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
							item->touch(current_time, false);
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
					NSC_TRACE_MSG("Next miss time is in: no timeout specified");
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
					NSC_TRACE_MSG("Next miss time is in: " + str::xtos(dur.total_seconds()) + "s");
					ret = dur;
				}
				return ret;
			}
		};
	}
}
