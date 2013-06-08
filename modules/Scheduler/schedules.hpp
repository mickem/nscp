#pragma once

#include <map>
#include <string>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <settings/client/settings_client.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/settings_object.hpp>
#include <nscapi/functions.hpp>
#include <nscapi/nscapi_helper.hpp>

namespace sh = nscapi::settings_helper;

namespace schedules {
	struct schedule_object {

		schedule_object() : is_template(false), report(0), id(0) {}
		schedule_object(const schedule_object &other) 
			: path(other.path)
			, alias(other.alias)
			, value(other.value)
			, parent(other.parent)
			, is_template(other.is_template)
			, target_id(other.target_id)
			, duration(other.duration)
			, channel(other.channel)
			, report(other.report)
			, command(other.command)
			, arguments(other.arguments)
			, id(other.id)
		{}
		const schedule_object& operator =(const schedule_object &other) {
			path = other.path;
			alias = other.alias;
			value = other.value;
			parent = other.parent;
			target_id = other.target_id;
			duration = other.duration;
			channel = other.channel;
			report = other.report;
			command = other.command;
			arguments = other.arguments;
			id = other.id;
			is_template = other.is_template;
			return *this;
		}
	
		// Object keys (managed by object handler)
		std::string path;
		std::string alias;
		std::string value;
		std::string parent;
		bool is_template;

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
			ss << alias << "[" << id << "] = "
				<< "{alias: " << alias
				<< ", command: " << command 
				<< ", channel: " << channel 
				<< ", target_id: " << target_id 
				<< ", duration: " << duration.total_seconds()
				<< "}";
			return ss.str();
		}
	};
	typedef boost::optional<schedule_object> optional_schedule_object;

	template<class T>
	inline void import_string(T &object, T &parent) {
		if (object.empty() && !parent.empty())
			object = parent;
	}


	struct schedule_reader {
		typedef schedule_object object_type;

		static void post_process_object(object_type &object) {}


		static void read_object(boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool oneliner, bool is_sample) {
			object.set_command(object.value);
			if (object.alias == "default") {
				object.set_duration("5m");
				object.set_report("all");
				object.channel = "NSCA";
			}
			std::string alias;

			nscapi::settings_helper::settings_registry settings(proxy);
			nscapi::settings_helper::path_extension root_path = settings.path(object.path);
			if (is_sample)
				root_path.set_sample();

			root_path.add_path()
				("SCHEDULE DEFENITION", "Schedule definition for: " + object.alias)
				;

			root_path.add_key()

				("command", sh::string_fun_key<std::string>(boost::bind(&object_type::set_command, &object, _1)),
				"SCHEDULE COMMAND", "Command to execute", object.alias == "default")

				("alias", sh::string_key(&alias),
				"SCHEDULE ALIAS", "The alias (service name) to report to server", object.alias == "default")

				("target", sh::string_key(&object.target_id),
				"TARGET", "The target to send the message to (will be resolved by the consumer)", true)

				("parent", nscapi::settings_helper::string_key(&object.parent, "default"),
				"TARGET PARENT", "The parent the target inherits from", object.alias == "default")

				("is template", nscapi::settings_helper::bool_key(&object.is_template, false),
				"IS TEMPLATE", "Declare this object as a template (this means it will not be available as a separate object)", true)

				;
			if (object.alias == "default") {
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

			settings.register_all();
			settings.notify();

			if (!alias.empty())
				object.alias = alias;
			/*
			BOOST_FOREACH(const object_type::options_type::value_type &kvp, options) {
				if (!object.has_option(kvp.first))
					object.options[kvp.first] = kvp.second;
			}
			*/

		}

		static void apply_parent(object_type &object, object_type &parent) {
			import_string(object.target_id, parent.target_id);
			import_string(object.command, parent.command);
			//import_string(object.arguments, parent.arguments);
			if (object.duration.total_seconds() == 0 && parent.duration.total_seconds() != 0)
				object.duration = parent.duration;
			import_string(object.channel, parent.channel);
			if (object.report == 0 && parent.report != 0)
				object.report = parent.report;
			/*
			object.address.import(parent.address);
			BOOST_FOREACH(object_type::options_type::value_type i, parent.options) {
				if (object.options.find(i.first) == object.options.end())
					object.options[i.first] = i.second;
			}
			*/
		}

	};
	typedef nscapi::settings_objects::object_handler<schedule_object, schedule_reader > schedule_handler;
}

