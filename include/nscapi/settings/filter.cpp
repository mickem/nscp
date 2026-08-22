// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <nscapi/settings/filter.hpp>
#include <nscapi/settings/object.hpp>

namespace nscapi {
namespace settings_filters {

void filter_object::read_object(settings_helper::path_extension& path, const bool is_default) {
  namespace sh = settings_helper;
  path.add_key()
      .add_string("filter", sh::cstring_fun_key([this](auto value) { this->set_filter_string(value); }), "FILTER",
                  "Scan files for matching rows for each matching rows an OK message will be submitted")
      .add_string("warning", sh::string_key(&filter_warn), "WARNING FILTER", "If any rows match this filter severity will escalated to WARNING")
      .add_string("critical", sh::string_key(&filter_crit), "CRITICAL FILTER", "If any rows match this filter severity will escalated to CRITICAL")
      .add_string("ok", sh::string_key(&filter_ok), "OK FILTER", "If any rows match this filter severity will escalated down to OK")
      .add_string("top syntax", sh::string_key(&syntax_top), "SYNTAX", "Format string for dates", !is_default)

      .add_string("ok syntax", sh::string_key(&syntax_ok), "SYNTAX", "Format string for dates", !is_default)

      .add_string("detail syntax", sh::string_key(&syntax_detail), "SYNTAX", "Format string for dates", !is_default)

      // No default here: like `top syntax` above, an unset value must stay
      // empty so apply_parent can inherit it from the default template
      // (import_string only copies into empty strings). The ", " fallback
      // lives in generic_summary itself.
      .add_string("list separator", sh::string_key(&list_separator), "LIST SEPARATOR",
                  "String used to separate the items of %(list) and friends (default ', '). Accepts the escapes \\n, \\r, \\t and \\\\; set it to \\n to "
                  "render one item per line (most Nagios compatible frontends show everything after the first line as long output). Reference it as %(sep) in "
                  "the top syntax to also break before the first item.",
                  true)
      // Number rendering, mirroring the same-named options of a queried check
      // (#1428). Unset means "leave it alone" so apply_parent can inherit.
      .add_int("decimals", sh::int_key(&decimals, -1), "DECIMALS",
               "Number of decimals to render the numbers of the message with (-1 keeps the historical rendering of up to three decimals with the trailing "
               "zeros stripped). Performance data is unaffected.",
               true)
      .add_string("byte unit", sh::string_key(&byte_unit), "BYTE UNIT",
                  "Unit to render every byte value of the message in: B, KB, MB, GB, TB, PB or EB. Empty (the default) scales each value on its own. "
                  "Performance data is unaffected.",
                  true)
      .add_string("decimal separator", sh::string_key(&decimal_separator), "DECIMAL SEPARATOR",
                  "Character to use as the decimal separator of the message, for instance ',' for the European rendering. Only the message is affected: "
                  "performance data and the numbers written in a filter always use '.'.",
                  true)
      .add_string("thousands separator", sh::string_key(&thousands_separator), "THOUSANDS SEPARATOR",
                  "Character to group the thousands of the message with. Empty (the default) means no grouping. Only the message is affected.", true)

      .add_string("perf config", sh::string_key(&perf_config), "PERF CONFIG", "Performance data configuration", true)

      .add_bool("debug", sh::bool_key(&debug), "DEBUG", "Enable this to display debug information for this match filter", true)

      .add_string("destination", sh::string_key(&target), "DESTINATION", "The destination for intercepted messages", !is_default)

      .add_string("target", sh::string_key(&target), "DESTINATION", "Same as destination", false)

      .add_string("maximum age", sh::string_fun_key([this](const auto& value) { this->set_max_age(value); }, "5m"), "MAXIMUM AGE",
                  "How long before reporting \"ok\".\nIf this is set to \"false\" no periodic ok messages will be reported only errors.")

      .add_string(
          "silent period", sh::string_fun_key([this](const auto& value) { this->set_silent_period(value); }, "false"), "Silent period",
          "How long before a new alert is reported after an alert is reported. In other words whenever an alert is fired and a notification is sent the same "
          "notification will not be sent again until this period has ended.\nIf this is set to \"false\" no periodic ok messages will be reported only errors.")

      .add_string("empty message", sh::string_key(&timeout_msg, "eventlog found no records"), "EMPTY MESSAGE",
                  "The message to display if nothing matches the filter (generally considered the ok state).", !is_default)

      .add_string("severity", sh::string_fun_key([this](const auto& value) { this->set_severity(value); }), "SEVERITY",
                  "THe severity of this message (OK, WARNING, CRITICAL, UNKNOWN)", !is_default)

      .add_string("command", sh::string_key(&command), "COMMAND NAME",
                  "The name of the command (think nagios service name) to report up stream (defaults to alias if not set)", !is_default)

      .add_string("target id", sh::string_key(&target_id), "TARGET ID", "The target to send the message to (will be resolved by the consumer)", true)

      .add_string("source id", sh::string_key(&source_id), "SOURCE ID",
                  "The name of the source system, will automatically use the remote system if a remote system is called. Almost most sending systems will "
                  "replace this with current systems hostname if not present. So use this only if you need specific source systems for specific schedules and "
                  "not calling remote systems.",
                  true)

      .add_bool("escape html", sh::bool_key(&escape_html), "ESCAPE HTML", "Escape HTML characters (< and >).", true);
}

void filter_object::apply_parent(const filter_object& parent) {
  using namespace nscapi::settings_objects;

  import_string(syntax_detail, parent.syntax_detail);
  import_string(syntax_top, parent.syntax_top);
  import_string(list_separator, parent.list_separator);
  if (decimals == -1) decimals = parent.decimals;
  import_string(byte_unit, parent.byte_unit);
  import_string(decimal_separator, parent.decimal_separator);
  import_string(thousands_separator, parent.thousands_separator);
  import_string(filter_string_, parent.filter_string_);
  import_string(filter_warn, parent.filter_warn);
  import_string(filter_crit, parent.filter_crit);
  import_string(filter_ok, parent.filter_ok);
  if (parent.debug) debug = parent.debug;
  import_string(target, parent.target);
  import_string(target_id, parent.target_id);
  import_string(source_id, parent.source_id);
  import_string(timeout_msg, parent.timeout_msg);
  if (parent.severity != -1 && severity == -1) severity = parent.severity;
  import_string(command, parent.command);
}
}  // namespace settings_filters
}  // namespace nscapi