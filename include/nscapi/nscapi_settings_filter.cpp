#include <nscapi/nscapi_settings_filter.hpp>

namespace nscapi {
	namespace settings_filters {
		void filter_object::read_object(nscapi::settings_helper::path_extension &path, const bool is_default) {
			namespace sh = nscapi::settings_helper;
			path.add_key()
				("filter", sh::string_key(&filter_string),
					"FILTER", "Scan files for matching rows for each matching rows an OK message will be submitted")

				("warning", sh::string_key(&filter_warn),
					"WARNING FILTER", "If any rows match this filter severity will escalated to WARNING")

				("critical", sh::string_key(&filter_crit),
					"CRITICAL FILTER", "If any rows match this filter severity will escalated to CRITICAL")

				("ok", sh::string_key(&filter_ok),
					"OK FILTER", "If any rows match this filter severity will escalated down to OK")

				("top syntax", sh::string_key(&syntax_top),
					"SYNTAX", "Format string for dates", !is_default)

				("ok syntax", sh::string_key(&syntax_ok),
					"SYNTAX", "Format string for dates", !is_default)

				("detail syntax", sh::string_key(&syntax_detail),
					"SYNTAX", "Format string for dates", !is_default)
				("perf config", sh::string_key(&perf_config),
					"PERF CONFIG", "Performance data configuration", true)

				("debug", nscapi::settings_helper::bool_key(&debug),
					"DEBUG", "Enable this to display debug information for this match filter", true)

				("destination", nscapi::settings_helper::string_key(&target),
					"DESTINATION", "The destination for intercepted messages", !is_default)

				("target", nscapi::settings_helper::string_key(&target),
					"DESTINATION", "Same as destination", false)

				("maximum age", sh::string_fun_key<std::string>(boost::bind(&filter_object::set_max_age, this, _1), "5m"),
					"MAGIMUM AGE", "How long before reporting \"ok\".\nIf this is set to \"false\" no periodic ok messages will be reported only errors.")

				("empty message", nscapi::settings_helper::string_key(&timeout_msg, "eventlog found no records"),
					"EMPTY MESSAGE", "The message to display if nothing matches the filter (generally considered the ok state).", !is_default)

				("severity", nscapi::settings_helper::string_fun_key<std::string>(boost::bind(&filter_object::set_severity, this, _1)),
					"SEVERITY", "THe severity of this message (OK, WARNING, CRITICAL, UNKNOWN)", !is_default)

				("command", nscapi::settings_helper::string_key(&command),
					"COMMAND NAME", "The name of the command (think nagios service name) to report up stream (defaults to alias if not set)", !is_default)

				("target id", nscapi::settings_helper::string_key(&target_id),
					"TARGET ID", "The target to send the message to (will be resolved by the consumer)", true)

				("source id", nscapi::settings_helper::string_key(&source_id),
					"SOURCE ID", "The name of the source system, will automatically use the remote system if a remote system is called. Almost most sending systems will replace this with current systems hostname if not present. So use this only if you need specific source systems for specific schedules and not calling remote systems.", true)

				("escape html", nscapi::settings_helper::bool_key(&escape_html),
					"ESCAPE HTML", "Escape HTML characters (< and >).", true)

				;
		}

		void filter_object::apply_parent(const filter_object &parent) {
			using namespace nscapi::settings_objects;

			import_string(syntax_detail, parent.syntax_detail);
			import_string(syntax_top, parent.syntax_top);
			import_string(filter_string, parent.filter_string);
			import_string(filter_warn, parent.filter_warn);
			import_string(filter_crit, parent.filter_crit);
			import_string(filter_ok, parent.filter_ok);
			if (parent.debug)
				debug = parent.debug;
			import_string(target, parent.target);
			import_string(target_id, parent.target_id);
			import_string(source_id, parent.source_id);
			import_string(target, parent.target);
			import_string(timeout_msg, parent.timeout_msg);
			if (parent.severity != -1 && severity == -1)
				severity = parent.severity;
			import_string(command, parent.command);
		}
	}
}