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

namespace sh = nscapi::settings_helper;

namespace schedules {
	struct schedule_object : public nscapi::settings_objects::object_instance_interface {

		typedef nscapi::settings_objects::object_instance_interface parent;

		schedule_object(std::string alias, std::string path) : parent(alias, path), report(0), id(0) {}

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
			ss <<alias << "[" << id << "] = "
				<< "{tpl: " << parent::to_string()
				<< ", command: " << command 
				<< ", channel: " << channel 
				<< ", source_id: " << source_id
				<< ", target_id: " << target_id 
				<< ", duration: " << duration.total_seconds()
				<< "}";
			return ss.str();
		}

/*
		static void init_default(object_type& object) {
			object.set_duration("5m");
			object.set_report("all");
			object.channel = "NSCA";
		}
		*/
		virtual void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample) {

			parent::read(proxy, oneliner, is_sample);

			set_command(value);
			bool is_def = is_default();
			std::string alias;

			nscapi::settings_helper::settings_registry settings(proxy);
			nscapi::settings_helper::path_extension root_path = settings.path(path);
			if (is_sample)
				root_path.set_sample();

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

	typedef nscapi::settings_objects::object_handler<schedule_object> schedule_handler;
}

