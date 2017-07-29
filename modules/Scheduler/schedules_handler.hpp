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

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/nscapi_settings_object.hpp>
#include <nscapi/functions.hpp>
#include <nscapi/nscapi_helper.hpp>

#include <parsers/cron/cron_parser.hpp>

#include <scheduler/simple_scheduler.hpp>

#include <str/utils.hpp>
#include <str/format.hpp>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <map>
#include <string>

namespace sh = nscapi::settings_helper;

namespace schedules {
	struct schedule_object : public nscapi::settings_objects::object_instance_interface {
		typedef nscapi::settings_objects::object_instance_interface parent;

		schedule_object(std::string alias, std::string path) : parent(alias, path), randomness(0.0), report(0), id(0) {}
		schedule_object(const schedule_object& other)
			: parent(other)
			, source_id(other.source_id)
			, target_id(other.target_id)
			, duration(other.duration)
			, randomness(other.randomness)
			, schedule(other.schedule)
			, channel(other.channel)
			, report(other.report)
			, command(other.command)
			, arguments(other.arguments) {}

		// Schedule keys
		std::string source_id;
		std::string target_id;
		boost::optional<boost::posix_time::time_duration> duration;
		double randomness;
		boost::optional<std::string> schedule;
		std::string  channel;
		unsigned int report;
		std::string command;
		std::list<std::string> arguments;

		// Others keys (Managed by application)
		int id;

		void set_randomness(std::string str) {
			randomness = str::stox<double>(boost::replace_first_copy(str, "%", "")) / 100.0;
		}
		void set_report(std::string str) {
			report = nscapi::report::parse(str);
		}
		void set_duration(std::string str) {
			duration = boost::posix_time::seconds(str::format::stox_as_time_sec<long>(str, "s"));
		}
		void set_schedule(std::string str) {
			schedule = str;
		}
		void set_command(std::string str) {
			if (!str.empty()) {
				arguments.clear();
				str::utils::parse_command(str, command, arguments);
			}
		}

		std::string to_string() const {
			std::stringstream ss;
			ss << get_alias() << "[" << id << "] = "
				<< "{tpl: " << parent::to_string()
				<< ", command: " << command
				<< ", channel: " << channel
				<< ", source_id: " << source_id
				<< ", target_id: " << target_id;
			if (duration) {
				ss << ", duration: " << (*duration).total_seconds() << "s, " << (randomness * 100) << "% randomness";
			}
			if (schedule)
				ss << ", schedule: " << *schedule;
			ss << "}";
			return ss.str();
		}

		virtual void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample) {
			parent::read(proxy, oneliner, is_sample);

			set_command(get_value());
			bool is_def = is_default();

			nscapi::settings_helper::settings_registry settings(proxy);
			nscapi::settings_helper::path_extension root_path = settings.path(get_path());
			if (is_sample)
				root_path.set_sample();
			if (oneliner)
				return;

			root_path.add_path()
				("SCHEDULE DEFENITION", "Schedule definition for: " + get_alias())
				;

			root_path.add_key()

				("command", sh::string_fun_key(boost::bind(&schedule_object::set_command, this, _1)),
					"SCHEDULE COMMAND", "Command to execute", is_def)

				("target", sh::string_key(&target_id),
					"TARGET", "The target to send the message to (will be resolved by the consumer)", true)
				("source", sh::string_key(&source_id),
					"SOURCE", "The name of the source system, will automatically use the remote system if a remote system is called. Almost most sending systems will replace this with current systems hostname if not present. So use this only if you need specific source systems for specific schedules and not calling remote systems.", true)

				;
			if (is_def) {
				root_path.add_key()

					("channel", sh::string_key(&channel, "NSCA"),
						"SCHEDULE CHANNEL", "Channel to send results on")

					("interval", sh::string_fun_key(boost::bind(&schedule_object::set_duration, this, _1)),
					"SCHEDULE INTERAVAL", "Time in seconds between each check")

					("randomness", sh::string_fun_key(boost::bind(&schedule_object::set_randomness, this, _1)),
						"RANDOMNESS", "% of the interval which should be random to prevent overloading server resources")

					("schedule", sh::string_fun_key(boost::bind(&schedule_object::set_schedule, this, _1)),
						"SCHEDULE", "Cron-like statement for when a task is run. Currently limited to only one number i.e. 1 * * * * or * * 1 * * but not 1 1 * * *")

					("report", sh::string_fun_key(boost::bind(&schedule_object::set_report, this, _1), "all"),
						"REPORT MODE", "What to report to the server (any of the following: all, critical, warning, unknown, ok)")

					;
			} else {
				root_path.add_key()
					("channel", sh::string_key(&channel),
						"SCHEDULE CHANNEL", "Channel to send results on")

					("interval", sh::string_fun_key(boost::bind(&schedule_object::set_duration, this, _1)),
						"SCHEDULE INTERAVAL", "Time in seconds between each check", true)

					("randomness", sh::string_fun_key(boost::bind(&schedule_object::set_randomness, this, _1)),
						"RANDOMNESS", "% of the interval which should be random to prevent overloading server resources")

					("schedule", sh::string_fun_key(boost::bind(&schedule_object::set_schedule, this, _1)),
						"SCHEDULE", "Cron-like statement for when a task is run. Currently limited to only one number i.e. 1 * * * * or * * 1 * * but not 1 1 * * *")

					("report", sh::string_fun_key(boost::bind(&schedule_object::set_report, this, _1)),
						"REPORT MODE", "What to report to the server (any of the following: all, critical, warning, unknown, ok)", true)

					;
			}

			settings.register_all();
			settings.notify();
		}
	};


	typedef boost::optional<schedule_object> optional_target_object;
	typedef boost::shared_ptr<schedule_object> target_object;

	typedef nscapi::settings_objects::object_handler<schedule_object> schedule_handler;

	struct task_handler {
		virtual bool handle_schedule(target_object task) = 0;
		virtual void on_error(const char* file, int line, std::string error) = 0;
		virtual void on_trace(const char* file, int line, std::string error) = 0;

	};

	struct scheduler : public simple_scheduler::handler {
		typedef boost::unordered_map<int, target_object> metadata_map;
		metadata_map metadata;
		simple_scheduler::scheduler tasks;
		task_handler *handler_;

		target_object get(int id);

		void start();
		void stop();

		simple_scheduler::scheduler& get_scheduler() {
			return tasks;
		}

		void set_handler(task_handler* handler) {
			handler_ = handler;
		}
		void prepare_shutdown() {
			tasks.prepare_shutdown();
		}
		void unset_handler() {
			handler_ = NULL;
		}
		void clear();

		void set_threads(int count) {
			tasks.set_threads(count);
		}

		void add_task(const target_object target);

		bool handle_schedule(simple_scheduler::task item) {
			task_handler *tmp = handler_;
			if (tmp) {
				if (!tmp->handle_schedule(get(item.id)))
					tasks.remove_task(item.id);

			}
			return true;
		}
		void on_error(const char* file, int line, std::string error) {
			task_handler *tmp = handler_;
			if (tmp)
				tmp->on_error(file, line, error);
		}
		void on_trace(const char* file, int line, std::string error) {
			task_handler *tmp = handler_;
			if (tmp)
				tmp->on_trace(file, line, error);
		}
	};

}