#pragma once

#include <map>
#include <string>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <settings/client/settings_client.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/nscapi_settings_object.hpp>
#include <nscapi/functions.hpp>
#include <nscapi/nscapi_helper.hpp>

namespace sh = nscapi::settings_helper;

namespace schedules {
	struct schedule_object {

		schedule_object() : report(0), id(0) {}

		nscapi::settings_objects::template_object tpl;

		// Schedule keys
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
			ss << tpl.alias << "[" << id << "] = "
				<< "{tpl: " << tpl.to_string()
				<< ", command: " << command 
				<< ", channel: " << channel 
				<< ", target_id: " << target_id 
				<< ", duration: " << duration.total_seconds()
				<< "}";
			return ss.str();
		}
	};
	typedef boost::optional<schedule_object> optional_schedule_object;

	struct schedule_reader {
		typedef schedule_object object_type;

		static void post_process_object(object_type &object) {}

		static void init_default(object_type& object) {
			object.set_duration("5m");
			object.set_report("all");
			object.channel = "NSCA";
		}

		static void read_object(boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool oneliner, bool is_sample) {
			object.set_command(object.tpl.value);
			bool is_def = object.tpl.is_default();
			std::string alias;

			nscapi::settings_helper::settings_registry settings(proxy);
			nscapi::settings_helper::path_extension root_path = settings.path(object.tpl.path);
			if (is_sample)
				root_path.set_sample();

			root_path.add_path()
				("SCHEDULE DEFENITION", "Schedule definition for: " + object.tpl.alias)
				;

			root_path.add_key()

				("command", sh::string_fun_key<std::string>(boost::bind(&object_type::set_command, &object, _1)),
				"SCHEDULE COMMAND", "Command to execute", is_def)

				("target", sh::string_key(&object.target_id),
				"TARGET", "The target to send the message to (will be resolved by the consumer)", true)

				;
			if (is_def) {
				root_path.add_key()

					("channel", sh::string_key(&object.channel, "NSCA"),
					"SCHEDULE CHANNEL", "Channel to send results on")

					("interval", sh::string_fun_key<std::string>(boost::bind(&object_type::set_duration, &object, _1), "5m"),
					"SCHEDULE INTERAVAL", "Time in seconds between each check")

					("report", sh::string_fun_key<std::string>(boost::bind(&object_type::set_report, &object, _1), "all"),
					"REPORT MODE", "What to report to the server (any of the following: all, critical, warning, unknown, ok)")

					;
			} else {
				root_path.add_key()
					("channel", sh::string_key(&object.channel),
					"SCHEDULE CHANNEL", "Channel to send results on")

					("interval", sh::string_fun_key<std::string>(boost::bind(&object_type::set_duration, &object, _1)),
					"SCHEDULE INTERAVAL", "Time in seconds between each check", true)

					("report", sh::string_fun_key<std::string>(boost::bind(&object_type::set_report, &object, _1)),
					"REPORT MODE", "What to report to the server (any of the following: all, critical, warning, unknown, ok)", true)

					;

			}

			object.tpl.read_object(root_path);

			settings.register_all();
			settings.notify();

			if (!alias.empty())
				object.tpl.alias = alias;
		}

		static void apply_parent(object_type &object, object_type &parent) {
			using namespace nscapi::settings_objects;
			import_string(object.target_id, parent.target_id);
			import_string(object.command, parent.command);
			//import_string(object.arguments, parent.arguments);
			if (object.duration.total_seconds() == 0 && parent.duration.total_seconds() != 0)
				object.duration = parent.duration;
			import_string(object.channel, parent.channel);
			if (object.report == 0 && parent.report != 0)
				object.report = parent.report;
		}

	};
	typedef nscapi::settings_objects::object_handler<schedule_object, schedule_reader > schedule_handler;
}

