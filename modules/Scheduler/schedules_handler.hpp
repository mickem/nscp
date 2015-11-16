#pragma once

#include <map>
#include <string>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/nscapi_settings_object.hpp>
#include <nscapi/functions.hpp>
#include <nscapi/nscapi_helper.hpp>


#include <scheduler/simple_scheduler.hpp>

namespace sh = nscapi::settings_helper;

namespace schedules {
	struct schedule_object : public nscapi::settings_objects::object_instance_interface {
		typedef nscapi::settings_objects::object_instance_interface parent;

		schedule_object(std::string alias, std::string path) : parent(alias, path), report(0), id(0) {}
		schedule_object(const schedule_object& other)
			: parent(other)
			, source_id(other.source_id)
			, target_id(other.target_id)
			, duration(other.duration)
			, channel(other.channel)
			, report(other.report)
			, command(other.command)
			, arguments(other.arguments) {}

		// Schedule keys
		std::string source_id;
		std::string target_id;
		boost::posix_time::time_duration duration;
		std::string  channel;
		unsigned int report;
		std::string command;
		std::list<std::string> arguments;

		// Others keys (Managed by application)
		int id;

		void set_report(std::string str) {
			report = nscapi::report::parse(str);
		}
		void set_duration(std::string str) {
			duration = boost::posix_time::seconds(strEx::stoui_as_time_sec(str, 1));
		}
		void set_command(std::string str) {
			if (!str.empty()) {
				strEx::parse_command(str, command, arguments);
			}
		}

		std::string to_string() const {
			std::stringstream ss;
			ss << alias << "[" << id << "] = "
				<< "{tpl: " << parent::to_string()
				<< ", command: " << command
				<< ", channel: " << channel
				<< ", source_id: " << source_id
				<< ", target_id: " << target_id
				<< ", duration: " << duration.total_seconds()
				<< "}";
			return ss.str();
		}

		virtual void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample) {
			parent::read(proxy, oneliner, is_sample);

			set_command(value);
			bool is_def = is_default();

			nscapi::settings_helper::settings_registry settings(proxy);
			nscapi::settings_helper::path_extension root_path = settings.path(get_path());
			if (is_sample)
				root_path.set_sample();
			if (oneliner)
				return;

			root_path.add_path()
				("SCHEDULE DEFENITION", "Schedule definition for: " + alias)
				;

			root_path.add_key()

				("command", sh::string_fun_key<std::string>(boost::bind(&schedule_object::set_command, this, _1)),
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

					("interval", sh::string_fun_key<std::string>(boost::bind(&schedule_object::set_duration, this, _1), "5m"),
						"SCHEDULE INTERAVAL", "Time in seconds between each check")

					("report", sh::string_fun_key<std::string>(boost::bind(&schedule_object::set_report, this, _1), "all"),
						"REPORT MODE", "What to report to the server (any of the following: all, critical, warning, unknown, ok)")

					;
			} else {
				root_path.add_key()
					("channel", sh::string_key(&channel),
						"SCHEDULE CHANNEL", "Channel to send results on")

					("interval", sh::string_fun_key<std::string>(boost::bind(&schedule_object::set_duration, this, _1)),
						"SCHEDULE INTERAVAL", "Time in seconds between each check", true)

					("report", sh::string_fun_key<std::string>(boost::bind(&schedule_object::set_report, this, _1)),
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
		virtual void on_error(std::string error) = 0;
		virtual void on_trace(std::string error) = 0;

	};

	struct scheduler : public simple_scheduler::handler {
		typedef boost::unordered_map<int, target_object> metadata_map;
		metadata_map metadata;
		simple_scheduler::scheduler tasks;
		task_handler *handler_;

		target_object get(int id);

		void start();
		void stop();

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
		int get_threads() const {
			return tasks.get_threads();
		}

		void add_task(const target_object target);

		bool handle_schedule(simple_scheduler::task item) {
			if (handler_) {
				if (!handler_->handle_schedule(get(item.id)))
					tasks.remove_task(item.id);

			}
			return true;
		}
		void on_error(std::string error) {
			if (handler_)
				handler_->on_error(error);
		}
		void on_trace(std::string error) {
			if (handler_)
				handler_->on_error(error);
		}
	};

}