#pragma once

#include <map>
#include <string>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <settings/client/settings_client.hpp>
#include <nscapi/settings_proxy.hpp>
#include <nscapi/settings_object.hpp>
#include <nscapi/functions.hpp>
#include <nscapi/nscapi_helper.hpp>

namespace sh = nscapi::settings_helper;

namespace schedules {
	struct schedule_object {

		schedule_object() : id(0), report(0), is_template(false) {}
		schedule_object(const schedule_object &other) 
			: path(other.path)
			, alias(other.alias)
			, value(other.value)
			, parent(other.parent)
			, target_id(other.target_id)
			, duration(other.duration)
			, channel(other.channel)
			, report(other.report)
			, command(other.command)
			, arguments(other.arguments)
			, id(other.id)
			, is_template(other.is_template)
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
		std::wstring path;
		std::wstring alias;
		std::wstring value;
		std::wstring parent;
		bool is_template;

		// Schedule keys
		std::wstring target_id;
		boost::posix_time::time_duration duration;
		std::wstring  channel;
		unsigned int report;
		std::wstring command;
		std::list<std::wstring> arguments;

		// Others keys (Managed by application)
		int id;

		void set_report(std::wstring str) {
			report = nscapi::report::parse(str);
		}
		void set_duration(std::wstring str) {
			duration = boost::posix_time::seconds(strEx::stoui_as_time_sec(str, 1));
		}
		void set_command(std::wstring str) {
			if (!str.empty()) {
				strEx::parse_command(str, command, arguments);
			}
		}

		std::wstring to_wstring() const {

			std::wstringstream ss;
			ss << alias << _T("[") << id << _T("] = ") 
				<< _T("{alias: ") << alias
				<< _T(", command: ") << command 
				<< _T(", channel: ") << channel 
				<< _T(", target_id: ") << target_id 
				<< _T(", duration: ") << duration.total_seconds()
				<< _T("}");
			return ss.str();
		}
		//std::wstring to_string() {

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


		static void read_object(boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool oneliner) {
			object.set_command(object.value);
			if (object.alias == _T("default")) {
				object.set_duration(_T("5m"));
				object.set_report(_T("all"));
				object.channel = _T("NSCA");
			}
			std::wstring alias;

			nscapi::settings_helper::settings_registry settings(proxy);

			settings.path(object.path).add_path()
				(_T("SCHEDULE DEFENITION"), _T("Schedule definition for: ") + object.alias)
				;

			settings.path(object.path).add_key()

				(_T("command"), sh::string_fun_key<std::wstring>(boost::bind(&object_type::set_command, &object, _1)),
				_T("SCHEDULE COMMAND"), _T("Command to execute"), object.alias == _T("default"))

				(_T("alias"), sh::wstring_key(&alias),
				_T("SCHEDULE ALIAS"), _T("The alias (service name) to report to server"), object.alias == _T("default"))

				(_T("target"), sh::wstring_key(&object.target_id),
				_T("TARGET"), _T("The target to send the message to (will be resolved by the consumer)"), true)

				(_T("parent"), nscapi::settings_helper::wstring_key(&object.parent, _T("default")),
				_T("TARGET PARENT"), _T("The parent the target inherits from"), object.alias == _T("default"))

				(_T("is template"), nscapi::settings_helper::bool_key(&object.is_template, false),
				_T("IS TEMPLATE"), _T("Declare this object as a template (this means it will not be avalible as a separate object)"), true)

				;
			if (object.alias == _T("default")) {
				settings.path(object.path).add_key()

					(_T("channel"), sh::wstring_key(&object.channel, _T("NSCA")),
					_T("SCHEDULE CHANNEL"), _T("Channel to send results on"))

					(_T("interval"), sh::string_fun_key<std::wstring>(boost::bind(&object_type::set_duration, &object, _1), _T("5m")),
					_T("SCHEDULE INTERAVAL"), _T("Time in seconds between each check"))

					(_T("report"), sh::string_fun_key<std::wstring>(boost::bind(&object_type::set_report, &object, _1), _T("all")),
					_T("REPORT MODE"), _T("What to report to the server (any of the following: all, critical, warning, unknown, ok)"))

					;
			} else {
				settings.path(object.path).add_key()
					(_T("channel"), sh::wstring_key(&object.channel),
					_T("SCHEDULE CHANNEL"), _T("Channel to send results on"))

					(_T("interval"), sh::string_fun_key<std::wstring>(boost::bind(&object_type::set_duration, &object, _1)),
					_T("SCHEDULE INTERAVAL"), _T("Time in seconds between each check"), true)

					(_T("report"), sh::string_fun_key<std::wstring>(boost::bind(&object_type::set_report, &object, _1)),
					_T("REPORT MODE"), _T("What to report to the server (any of the following: all, critical, warning, unknown, ok)"), true)

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

