// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <nscapi/nscapi_helper.hpp>
#include <nscapi/protobuf/functions_status.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <set>
#include <sstream>

namespace modern_filter {

std::string data_container::build_number_format(str::number_format &fmt) const {
  fmt.decimals = decimals;
  fmt.byte_unit = byte_unit;
  // An explicitly emptied separator means "the default radix character".
  fmt.decimal_separator = decimal_separator.empty() ? "." : decimal_separator;
  fmt.thousands_separator = thousands_separator;
  if (!byte_unit.empty() && str::format::byte_unit_index(byte_unit) < 0) {
    return "Invalid byte-unit: " + byte_unit + " (expected one of B, KB, MB, GB, TB, PB, EB)";
  }
  if (decimals < -1) return "Invalid decimals: " + str::xtos(decimals) + " (expected 0 or more, or -1 to leave the rendering alone)";
  // An unbounded decimals would make render_fixed allocate a huge string and
  // crash the check; past max_decimals the digits are noise anyway.
  if (decimals > str::max_decimals) {
    return "Invalid decimals: " + str::xtos(decimals) + " (expected at most " + str::xtos(str::max_decimals) + ")";
  }
  return "";
}

void perf_writer::write(const parsers::where::performance_data &data) {
  PB::Common::PerformanceData *perf = line.add_perf();
  perf->set_alias(data.alias);
  if (data.float_value) {
    const parsers::where::performance_data::perf_value &value = data.float_value.value();
    PB::Common::PerformanceData::FloatValue *perfData = perf->mutable_float_value();
    if (!data.unit.empty()) perfData->set_unit(data.unit);
    perfData->set_value(value.value);
    if (value.warn) perfData->mutable_warning()->set_value(value.warn.value());
    if (value.crit) perfData->mutable_critical()->set_value(value.crit.value());
    if (value.minimum) perfData->mutable_minimum()->set_value(value.minimum.value());
    if (value.maximum) perfData->mutable_maximum()->set_value(value.maximum.value());
  } else if (data.string_value) {
    const std::string value = data.string_value.value();
    PB::Common::PerformanceData::StringValue *perfData = perf->mutable_string_value();
    perfData->set_value(value);
  }
}

cli_helper_base::cli_helper_base(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                                 data_container &data)
    : data(data),
      desc("Allowed options for " + request.command()),
      request(request),
      response(response),
      show_all(false),
      has_warmup_option(false),
      short_thresholds_declared(false),
      short_thresholds_registered(false),
      short_thresholds_disabled(false),
      short_warn_owned(false),
      short_crit_owned(false),
      show_query_registered(false),
      show_query_owned(false),
      show_query_requested(false) {}

void cli_helper_base::add_filter_option(const std::string &filter) {
  typedef boost::program_options::typed_value<std::vector<std::string> > filter_op_type;
  filter_op_type *filter_op = boost::program_options::value<std::vector<std::string> >(&data.filter_string);
  if (!filter.empty()) {
    std::vector<std::string> def;
    def.push_back(filter);
    filter_op->default_value(def, filter);
  }

  // clang-format off
  desc.add_options()
    ("filter", filter_op,
    (std::string("Filter which marks interesting items.\nInteresting items are items which will be included in the check.\nThey do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.") + common_option_marker).c_str())
    ;
  // clang-format on
}

void cli_helper_base::add_warn_option(const std::string &warn) {
  typedef boost::program_options::typed_value<std::vector<std::string> > filter_op_type;
  filter_op_type *warn_op = boost::program_options::value<std::vector<std::string> >(&data.warn_string);
  if (!warn.empty()) {
    std::vector<std::string> def;
    def.push_back(warn);
    warn_op->default_value(def, warn);
  }

  // clang-format off
  desc.add_options()
    ("warning", warn_op,
    (std::string("Filter which marks items which generates a warning state.\nIf anything matches this filter the return status will be escalated to warning.\n") + common_option_marker).c_str())
    ("warn", boost::program_options::value<std::vector<std::string> >(),
    (std::string("Short alias for warning") + common_option_marker).c_str())
    ;
  // clang-format on
}

void cli_helper_base::add_warn_option(const std::string &warn1, const std::string &warn2) {
  typedef boost::program_options::typed_value<std::vector<std::string> > filter_op_type;
  filter_op_type *warn_op = boost::program_options::value<std::vector<std::string> >(&data.warn_string);
  std::vector<std::string> def;
  def.push_back(warn1);
  def.push_back(warn2);
  warn_op->default_value(def, warn1 + ", " + warn2);

  // clang-format off
  desc.add_options()
    ("warning", warn_op,
    (std::string("Filter which marks items which generates a warning state.\nIf anything matches this filter the return status will be escalated to warning.\n") + common_option_marker).c_str())
    ("warn", boost::program_options::value<std::vector<std::string> >(),
    (std::string("Short alias for warning") + common_option_marker).c_str())
    ;
  // clang-format on
}

void cli_helper_base::add_crit_option(const std::string &crit) {
  typedef boost::program_options::typed_value<std::vector<std::string> > filter_op_type;
  filter_op_type *crit_op = boost::program_options::value<std::vector<std::string> >(&data.crit_string);
  if (!crit.empty()) {
    std::vector<std::string> def;
    def.push_back(crit);
    crit_op->default_value(def, crit);
  }

  // clang-format off
  desc.add_options()
    ("critical", crit_op,
    (std::string("Filter which marks items which generates a critical state.\nIf anything matches this filter the return status will be escalated to critical.\n") + common_option_marker).c_str())
    ("crit", boost::program_options::value<std::vector<std::string> >(),
      (std::string("Short alias for critical.") + common_option_marker).c_str())
    ;
  // clang-format on
}

void cli_helper_base::add_crit_option(const std::string &crit1, const std::string &crit2) {
  typedef boost::program_options::typed_value<std::vector<std::string> > filter_op_type;
  filter_op_type *crit_op = boost::program_options::value<std::vector<std::string> >(&data.crit_string);
  std::vector<std::string> def;
  def.push_back(crit1);
  def.push_back(crit2);
  crit_op->default_value(def, crit1 + ", " + crit2);

  // clang-format off
  desc.add_options()
    ("critical", crit_op,
    (std::string("Filter which marks items which generates a critical state.\nIf anything matches this filter the return status will be escalated to critical.\n") + common_option_marker).c_str())
    ("crit", boost::program_options::value<std::vector<std::string> >(),
      (std::string("Short alias for critical.") + common_option_marker).c_str())
    ;
  // clang-format on
}

void cli_helper_base::add_ok_option(const std::string &ok) {
  typedef boost::program_options::typed_value<std::vector<std::string> > filter_op_type;
  filter_op_type *ok_op = boost::program_options::value<std::vector<std::string> >(&data.ok_string);
  if (!ok.empty()) {
    std::vector<std::string> def;
    def.push_back(ok);
    ok_op->default_value(def, ok);
  }

  // clang-format off
  desc.add_options()
    ("ok", ok_op,
      (std::string("Filter which marks items which generates an ok state.\nIf anything matches this any previous state for this item will be reset to ok.") + common_option_marker).c_str())
    ;
  // clang-format on
}

void cli_helper_base::add_misc_options(const std::string &empty_state) {
  boost::program_options::typed_value<std::string> *empty_state_op = boost::program_options::value<std::string>(&data.empty_state);
  boost::program_options::typed_value<std::string> *perf_config_op = boost::program_options::value<std::string>(&data.perf_config);

  if (!empty_state.empty()) empty_state_op->default_value(empty_state);
  if (!data.perf_config.empty()) perf_config_op->default_value(data.perf_config);
  // Boolean options must NOT be bool_switch: checks are driven over REST,
  // which passes each flag as the single token `x=true`, and bool_switch
  // rejects that with "does not take any arguments". implicit_value keeps
  // the bare `--show-all` CLI form working.
  // clang-format off
  desc.add_options()
    ("debug", boost::program_options::value<bool>(&data.debug)->implicit_value(true)->default_value(false),
      (std::string("Show debugging information in the log") + common_option_marker).c_str())
    ("show-all", boost::program_options::value<bool>(&show_all)->implicit_value(true)->default_value(false),
      (std::string("Show details for all matches regardless of status (normally details are only showed for warnings and criticals).") + common_option_marker).c_str())
    ("empty-state", empty_state_op,
	(std::string("Return status to use when nothing matched filter.\nIf no filter is specified this will never happen unless the file is empty.") + common_option_marker).c_str())
    ("perf-config", perf_config_op,
	(std::string("Performance data generation configuration\nTODO: obj ( key: value; key: value) obj (key:valuer;key:value)") + common_option_marker).c_str())
    ("escape-html", boost::program_options::value<bool>(&data.escape_html)->implicit_value(true)->default_value(false),
	(std::string("Escape any < and > characters to prevent HTML encoding") + common_option_marker).c_str())
    ("list-separator", boost::program_options::value<std::string>(&data.list_separator)->default_value(", "),
	(std::string("String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).\n"
	"Accepts the escapes \\n, \\r, \\t and \\\\ (a configuration file value is a single line, so a real newline cannot be written).\n"
	"Set to \\n to render one item per line, which most Nagios compatible frontends show as long output below the summary line.\n"
	"The top-syntax decides what precedes the first item; templates are never escape-decoded, so reference the decoded separator as %(sep) to break "
	"before it too: --top-syntax \"%(status): %(count) items:%(sep)%(list)\".") + common_option_marker).c_str())
    ("decimals", boost::program_options::value<int>(&data.decimals)->default_value(-1),
	(std::string("Number of decimals to render the numbers of the message with, for instance 1 to turn \"25.191GB\" into \"25.2GB\".\n"
	"Applies to the byte and percentage keywords and to format_bytes()/format_number(); -1 (the default) keeps the historical rendering of up to three "
	"decimals with the trailing zeros stripped.\n"
	"Performance data is unaffected - it is generated from the raw values so that graphs keep their full precision.") + common_option_marker).c_str())
    ("byte-unit", boost::program_options::value<std::string>(&data.byte_unit),
	(std::string("Unit to render every byte value of the message in: B, KB, MB, GB, TB, PB or EB.\n"
	"By default each value scales on its own, which is why a single line can read \"140.293GB/0.983TB\"; pinning the unit makes the values comparable "
	"(\"140.29GB/1006.85GB\").\n"
	"Performance data is unaffected - its unit is chosen separately and can be set with perf-config.") + common_option_marker).c_str())
    // No program_options default: the "." lives on data_container instead, so
    // that the docs extractor does not unexpand the literal "." into the
    // ${data-path} token when it resolves path variables in defaults.
    ("decimal-separator", boost::program_options::value<std::string>(&data.decimal_separator),
	(std::string("Character to use as the decimal separator of the message, for instance \",\" for the European rendering (default \".\").\n"
	"Only the message is affected: performance data and the numbers you write in a filter or threshold always use \".\", as their consumers require.") +
	common_option_marker).c_str())
    ("thousands-separator", boost::program_options::value<std::string>(&data.thousands_separator),
	(std::string("Character to group the thousands of the message with, for instance \".\" to render 1006.85 GB as \"1.006,85\" (together with "
	"decimal-separator=,). Empty (the default) means no grouping.\n"
	"Only the message is affected: performance data and the numbers you write in a filter or threshold are never grouped.") + common_option_marker).c_str())
    ;
  // clang-format on
  nscapi::program_options::add_help(desc);
}

void cli_helper_base::add_options(const std::string &warn, const std::string &crit, const std::string &filter,
                                  const std::map<std::string, std::string> &filter_syntax, const std::string &empty_state) {
  set_filter_syntax(filter_syntax);
  add_filter_option(filter);
  add_warn_option(warn);
  add_crit_option(crit);
  add_ok_option();
  add_misc_options(empty_state);
}

void cli_helper_base::add_options(const std::map<std::string, std::string> &filter_syntax, const std::string &empty_state) {
  set_filter_syntax(filter_syntax);
  add_ok_option();
  add_misc_options(empty_state);
}

void cli_helper_base::add_short_threshold(const std::string &variable, const shorthand::direction dir, const shorthand::unit value_unit,
                                          const std::string &warn_default, const std::string &crit_default) {
  short_thresholds.push_back(shorthand::spec(variable, dir, value_unit, warn_default, crit_default));
  short_thresholds_declared = true;
}

void cli_helper_base::register_short_thresholds() {
  if (short_thresholds_registered || short_thresholds_disabled) return;
  short_thresholds_registered = true;
  const bool is_default = !short_thresholds_declared;
  if (is_default) short_thresholds.push_back(shorthand::spec("count", shorthand::above));
  const std::string usage = shorthand::describe_usage(short_thresholds);
  std::string tail =
      "\nTakes a number or a range in the format of the monitoring plugins (10: is below 10, ~:10 is above 10, 10:20 is outside 10 to 20 and @10:20 is "
      "inside it); anything else is used as an expression as it is.";
  if (short_thresholds.size() > 1) tail += "\nTakes one value per keyword, separated by commas; one that is left out keeps its default.";
  const std::string unit_text = shorthand::describe_unit(short_thresholds.front());
  if (!unit_text.empty()) tail += "\n" + unit_text;
  // Only the form every check without a keyword of its own shares is a
  // common option; a check-specific one stays on the command's own page.
  if (is_default) tail += common_option_marker;
  const std::string warn_text = "Short form of warning: -w " + usage + " is the same as warning=\"" +
                                shorthand::describe(short_thresholds, short_warn_templates) + "\"" + short_effect("warning", short_warn_templates) + tail;
  const std::string crit_text = "Short form of critical: -c " + usage + " is the same as critical=\"" +
                                shorthand::describe(short_thresholds, short_crit_templates) + "\"" + short_effect("critical", short_crit_templates) + tail;
  // A check that already has an option of that name keeps it, and its value
  // is then not ours to read (apply_short_threshold).
  if (desc.find_nothrow("w", false) == nullptr) {
    desc.add_options()("w", short_option(short_warn, shorthand::warning), warn_text.c_str());
    short_warn_owned = true;
  }
  if (desc.find_nothrow("c", false) == nullptr) {
    desc.add_options()("c", short_option(short_crit, shorthand::critical), crit_text.c_str());
    short_crit_owned = true;
  }
}

void cli_helper_base::register_show_query() {
  if (show_query_registered) return;
  show_query_registered = true;
  // A check that already has an option of that name keeps it.
  if (desc.find_nothrow("show-query", false) != nullptr) return;
  const std::string text =
      "Show the query the check would run with the given arguments, instead of running it.\n"
      "Prints every option the check evaluates with its final value - the defaults, -w and -c resolved into warning and critical, and whatever "
      "the check derives from its other options - as a line that can be passed back. The expressions are still parsed, so an invalid one is "
      "reported as it would be on a real run." +
      common_option_marker;
  desc.add_options()("show-query", boost::program_options::value<bool>()->implicit_value(true)->default_value(false), text.c_str());
  show_query_owned = true;
}

std::string cli_helper_base::short_effect(const std::string &option, const std::vector<std::string> &templates) {
  if (templates.empty()) return " and replaces the default " + option + ".";
  return ": the default " + option + " with its bound moved, the other conditions stay.";
}

bool cli_helper_base::is_requested(const std::string &name) const {
  bool requested = false;
  for (int i = 0; i < request.arguments_size(); ++i) {
    std::string arg = request.arguments(i);
    if (boost::starts_with(arg, "--")) arg = arg.substr(2);
    const std::string::size_type eq = arg.find('=');
    const std::string key = arg.substr(0, eq);
    if (key != name && !is_abbreviation_of(key, name)) continue;
    if (eq == std::string::npos) {
      requested = true;
    } else {
      const std::string value = boost::to_lower_copy(arg.substr(eq + 1));
      requested = value == "true" || value == "1" || value == "yes" || value == "on";
    }
  }
  return requested;
}

bool cli_helper_base::is_abbreviation_of(const std::string &key, const std::string &name) const {
  if (key.empty() || key.size() >= name.size() || !boost::starts_with(name, key)) return false;
  for (const auto &op : desc.options()) {
    const std::string &other = op->long_name();
    if (other != name && boost::starts_with(other, key)) return false;
  }
  return true;
}

bool cli_helper_base::answer_default_queries() {
  if (is_requested("show-default")) {
    // Rendered before -w / -c and show-query are registered, which is what
    // leaves them out.
    const std::string defaults = nscapi::program_options::help_show_default(desc);
    register_short_thresholds();
    register_show_query();
    nscapi::protobuf::functions::set_response_good(*response, defaults);
    return true;
  }
  register_short_thresholds();
  register_show_query();
  return false;
}

bool cli_helper_base::is_query_meta_option(const std::string &name) const {
  if (name == "w") return short_warn_owned;
  if (name == "c") return short_crit_owned;
  return name == "show-query" || name == "show-default" || name == "help" || name == "help-pb" || name == "help-short" || name == "warn" || name == "crit";
}

bool cli_helper_base::final_filter_setting(const std::string &name, std::vector<std::string> &values) const {
  values.clear();
  const std::vector<std::string> *list = nullptr;
  const std::string *value = nullptr;
  if (name == "filter")
    list = &data.filter_string;
  else if (name == "warning")
    list = &data.warn_string;
  else if (name == "critical")
    list = &data.crit_string;
  else if (name == "ok")
    list = &data.ok_string;
  else if (name == "top-syntax")
    value = &data.syntax_top;
  else if (name == "ok-syntax")
    value = &data.syntax_ok;
  else if (name == "empty-syntax")
    value = &data.syntax_empty;
  else if (name == "detail-syntax")
    value = &data.syntax_detail;
  else if (name == "perf-syntax")
    value = &data.syntax_perf;
  else if (name == "unique-index")
    value = &data.syntax_unique;
  else if (name == "empty-state")
    value = &data.empty_state;
  else if (name == "perf-config")
    value = &data.perf_config;
  else
    return false;
  if (list != nullptr) {
    for (const std::string &v : *list)
      if (!v.empty()) values.push_back(v);
  } else if (!value->empty()) {
    values.push_back(*value);
  }
  return true;
}

const std::vector<std::string> &cli_helper_base::filter_setting_names() {
  static const std::vector<std::string> names = {"filter",       "warning",       "critical",    "ok",           "top-syntax",  "ok-syntax",
                                                 "empty-syntax", "detail-syntax", "perf-syntax", "unique-index", "empty-state", "perf-config"};
  return names;
}

bool cli_helper_base::render_option_value(const boost::any &value, std::vector<std::string> &out) {
  out.clear();
  if (value.empty()) return false;
  if (const std::string *s = boost::any_cast<std::string>(&value)) {
    if (!s->empty()) out.push_back(*s);
  } else if (const std::vector<std::string> *l = boost::any_cast<std::vector<std::string> >(&value)) {
    for (const std::string &v : *l)
      if (!v.empty()) out.push_back(v);
  } else if (const bool *b = boost::any_cast<bool>(&value)) {
    out.push_back(*b ? "true" : "false");
  } else if (const int *i = boost::any_cast<int>(&value)) {
    out.push_back(std::to_string(*i));
  } else if (const unsigned int *u = boost::any_cast<unsigned int>(&value)) {
    out.push_back(std::to_string(*u));
  } else if (const unsigned short *us = boost::any_cast<unsigned short>(&value)) {
    out.push_back(std::to_string(*us));
  } else if (const short *ss = boost::any_cast<short>(&value)) {
    out.push_back(std::to_string(*ss));
  } else if (const long *lo = boost::any_cast<long>(&value)) {
    out.push_back(std::to_string(*lo));
  } else if (const unsigned long *ul = boost::any_cast<unsigned long>(&value)) {
    out.push_back(std::to_string(*ul));
  } else if (const long long *ll = boost::any_cast<long long>(&value)) {
    out.push_back(std::to_string(*ll));
  } else if (const unsigned long long *ull = boost::any_cast<unsigned long long>(&value)) {
    out.push_back(std::to_string(*ull));
  } else if (const double *d = boost::any_cast<double>(&value)) {
    out.push_back(boost::lexical_cast<std::string>(*d));
  } else if (const float *f = boost::any_cast<float>(&value)) {
    out.push_back(boost::lexical_cast<std::string>(*f));
  } else {
    return false;
  }
  return true;
}

bool cli_helper_base::is_sensitive_option(const std::string &name) {
  const std::string lower = boost::to_lower_copy(name);
  return lower.find("password") != std::string::npos || lower.find("token") != std::string::npos || lower.find("secret") != std::string::npos ||
         lower == "connection-string" || lower == "header";
}

void cli_helper_base::remember_query(const boost::program_options::variables_map &vm, const std::vector<std::string> &extra) {
  show_query_requested = false;
  query_options.clear();
  query_extra.clear();
  if (!show_query_owned || vm.count("show-query") == 0) return;
  show_query_requested = vm["show-query"].as<bool>();
  if (!show_query_requested) return;
  std::vector<std::string> ignored;
  for (const auto &op : desc.options()) {
    const std::string &name = op->long_name();
    if (is_query_meta_option(name) || final_filter_setting(name, ignored) || vm.count(name) == 0) continue;
    // show-all is not a setting of its own: parse_options_post() has
    // already folded it into the top-syntax, which is printed as rewritten.
    // Echoed next to it, the pasted line would rewrite that syntax again.
    if (name == "show-all") continue;
    std::vector<std::string> values;
    if (!render_option_value(vm[name].value(), values) || values.empty()) continue;
    // The line is a check result: it travels to the monitoring server and
    // into logs, so a credential is shown as given but never with its value.
    if (is_sensitive_option(name)) values.assign(values.size(), "***");
    query_options[name] = values;
  }
  query_extra = extra;
}

std::string cli_helper_base::render_query() const {
  std::stringstream ret;
  const auto token = [&ret](const std::string &name, const std::vector<std::string> &values) {
    for (const std::string &v : values) ret << "\"" << name << "=" << v << "\" ";
  };
  std::set<std::string> done;
  for (const auto &op : desc.options()) {
    const std::string &name = op->long_name();
    if (is_query_meta_option(name)) continue;
    std::vector<std::string> values;
    if (final_filter_setting(name, values)) {
      done.insert(name);
      if (values.empty() && (name == "filter" || name == "warning" || name == "critical" || name == "ok")) {
        boost::any def;
        std::vector<std::string> defaults;
        if (op->semantic()->apply_default(def) && render_option_value(def, defaults) && !defaults.empty()) values.push_back("none");
      }
      token(name, values);
      continue;
    }
    const auto it = query_options.find(name);
    if (it != query_options.end()) token(name, it->second);
  }
  for (const std::string &name : filter_setting_names()) {
    if (done.count(name) > 0) continue;
    std::vector<std::string> values;
    final_filter_setting(name, values);
    token(name, values);
  }
  for (const std::string &arg : query_extra) ret << "\"" << arg << "\" ";
  return ret.str();
}

boost::program_options::typed_value<std::vector<std::string> > *cli_helper_base::short_option(std::vector<std::string> &store,
                                                                                              const shorthand::side which) const {
  boost::program_options::typed_value<std::vector<std::string> > *option = boost::program_options::value<std::vector<std::string> >(&store);
  const std::string value = shorthand::default_value(short_thresholds, which);
  if (!value.empty()) option->default_value(std::vector<std::string>(1, value), value);
  return option;
}

PB::Commands::QueryRequestMessage::Request cli_helper_base::normalized_request() const {
  PB::Commands::QueryRequestMessage::Request copy(request);
  if (short_thresholds_disabled) return copy;
  const std::vector<std::string> args(request.arguments().begin(), request.arguments().end());
  copy.clear_arguments();
  for (const std::string &arg : shorthand::normalize_arguments(args)) copy.add_arguments(arg);
  return copy;
}

bool cli_helper_base::check_short_values(const PB::Commands::QueryRequestMessage::Request &normalized) {
  if (short_thresholds_disabled) return true;
  const std::vector<std::string> args(normalized.arguments().begin(), normalized.arguments().end());
  const char flag = shorthand::find_missing_value(args);
  if (flag == 0 || !(flag == 'w' ? short_warn_owned : short_crit_owned)) return true;
  nscapi::protobuf::functions::set_response_bad(*response, shorthand::missing_value_error(flag));
  return false;
}

bool cli_helper_base::apply_short_thresholds(const boost::program_options::variables_map &vm) {
  // An option of that name the check declared itself is not a threshold:
  // reading it as one would find no values and clear the default.
  return (!short_warn_owned || apply_short_threshold(vm, "w", "warning", shorthand::warning, short_warn, short_warn_templates, data.warn_string)) &&
         (!short_crit_owned || apply_short_threshold(vm, "c", "critical", shorthand::critical, short_crit, short_crit_templates, data.crit_string));
}

bool cli_helper_base::apply_short_threshold(const boost::program_options::variables_map &vm, const std::string &short_name, const std::string &option,
                                            const shorthand::side which, const std::vector<std::string> &values, const std::vector<std::string> &templates,
                                            std::vector<std::string> &target) {
  // The option's default is documentation: the check's own default is what
  // runs until -w / -c is actually given.
  if (vm.count(short_name) == 0 || vm[short_name].defaulted()) return true;
  const std::string alias = option == "warning" ? "warn" : "crit";
  const bool defaulted = (vm.count(option) == 0 || vm[option].defaulted()) && vm.count(alias) == 0;
  std::vector<std::string> expressions;
  for (const std::string &value : values) {
    std::vector<std::string> expanded;
    std::string error;
    if (!shorthand::expand(short_thresholds, templates, which, value, expanded, error)) {
      nscapi::protobuf::functions::set_response_bad(*response, error);
      return false;
    }
    expressions.insert(expressions.end(), expanded.begin(), expanded.end());
  }
  if (defaulted) target.clear();
  target.insert(target.end(), expressions.begin(), expressions.end());
  return true;
}

void cli_helper_base::add_warmup_option() {
  has_warmup_option = true;
  // clang-format off
  desc.add_options()
    ("warmup-state", boost::program_options::value<std::string>(&data.warmup_state)->default_value("unknown"),
      "Return status to use while the background collector has not produced its first sample yet (shortly after the agent or module starts): "
      "ok, warning, critical or unknown.\n"
      "The message still says that the collector is initializing, so a warm-up result is never mistaken for a reading. "
      "A collector that has tried to sample and failed is not warming up: that is always UNKNOWN.")
    ;
  // clang-format on
}

void cli_helper_base::set_warmup_response(const std::string &message) const {
  response->add_lines()->set_message(message);
  if (response->command().empty()) response->set_command("unknown");
  response->set_result(warmup_result(data.warmup_state));
}

bool cli_helper_base::answer_unless_sampled(const sampling::status &status, const std::string &what) const {
  switch (status.state) {
    case sampling::readiness::ready:
      return false;
    case sampling::readiness::warming_up:
      set_warmup_response("No " + what + " data available yet (collector still initializing)");
      return true;
    case sampling::readiness::failed:
    default:
      if (status.recovering) {
        nscapi::protobuf::functions::set_response_bad(
            *response, "No " + what + " data available: the collector could read it again just now, after failing; the first sample follows next");
      } else {
        nscapi::protobuf::functions::set_response_bad(
            *response, "No " + what + " data available: the collector failed to sample it" + (status.error.empty() ? "" : ": " + status.error));
      }
      return true;
  }
}

bool cli_helper_base::is_valid_warmup_state(const std::string &state) {
  const std::string s = boost::to_lower_copy(state);
  return s == "unknown" || s == "u" || s == "3" || nscapi::protobuf::functions::parse_nagios(s) != PB::Common::ResultCode::UNKNOWN;
}

PB::Common::ResultCode cli_helper_base::warmup_result(const std::string &state) { return nscapi::protobuf::functions::parse_nagios(state); }

bool cli_helper_base::validate_options() const {
  if (has_warmup_option && !is_valid_warmup_state(data.warmup_state)) {
    nscapi::protobuf::functions::set_response_bad(*response,
                                                  "Invalid warmup-state: " + data.warmup_state + " (expected one of ok, warning, critical, unknown)");
    return false;
  }
  return true;
}

void cli_helper_base::parse_options_post(const boost::program_options::variables_map &vm) const {
  data.warmup_state_given = vm.count("warmup-state") > 0 && !vm["warmup-state"].defaulted();
  if (show_all) {
    if (data.syntax_top.find("${problem_list}") != std::string::npos)
      boost::replace_all(data.syntax_top, "${problem_list}", "${detail_list}");
    else if (data.syntax_top.find("%(problem_list)") != std::string::npos)
      boost::replace_all(data.syntax_top, "%(problem_list)", "%(detail_list)");
    else if (data.syntax_top.find("%(list)") != std::string::npos)
      boost::replace_all(data.syntax_top, "%(list)", "%(list)");
    else if (data.syntax_top.find("${list}") != std::string::npos)
      boost::replace_all(data.syntax_top, "${list}", "%(list)");
    else
      data.syntax_top = +"%(detail_list)";
  }
  if (boost::contains(data.syntax_top, "detail_list") || boost::contains(data.syntax_top, "(list)") || boost::contains(data.syntax_top, "{list}") ||
      boost::contains(data.syntax_top, "match_list") || boost::contains(data.syntax_top, "lines"))
    data.syntax_ok = "";
  if (vm.count("warn")) data.warn_string = vm["warn"].as<std::vector<std::string> >();
  if (vm.count("crit")) data.crit_string = vm["crit"].as<std::vector<std::string> >();
}

bool cli_helper_base::parse_options(const boost::program_options::positional_options_description &p) {
  if (answer_default_queries()) return false;
  const PB::Commands::QueryRequestMessage::Request normalized = normalized_request();
  if (!check_short_values(normalized)) return false;
  boost::program_options::variables_map vm;
  if (!nscapi::program_options::process_arguments_from_request(vm, desc, fields, normalized, *response, p)) return false;
  parse_options_post(vm);
  remember_query(vm);
  return apply_short_thresholds(vm) && validate_options();
}

bool cli_helper_base::parse_options() {
  if (answer_default_queries()) return false;
  const PB::Commands::QueryRequestMessage::Request normalized = normalized_request();
  if (!check_short_values(normalized)) return false;
  boost::program_options::variables_map vm;
  if (!nscapi::program_options::process_arguments_from_request(vm, desc, fields, normalized, *response)) return false;
  parse_options_post(vm);
  remember_query(vm);
  return apply_short_thresholds(vm) && validate_options();
}

bool cli_helper_base::parse_options(std::vector<std::string> &extra) {
  if (answer_default_queries()) return false;
  const PB::Commands::QueryRequestMessage::Request normalized = normalized_request();
  if (!check_short_values(normalized)) return false;
  boost::program_options::variables_map vm;
  if (!nscapi::program_options::process_arguments_from_request(vm, desc, fields, normalized, *response, true, extra)) return false;
  parse_options_post(vm);
  remember_query(vm, extra);
  return apply_short_thresholds(vm) && validate_options();
}

void cli_helper_base::append_all_filters(const std::string &verb, const std::string &filter) const {
  if (data.filter_string.empty()) {
    data.filter_string.push_back(filter);
  }
  for (std::string &f : data.filter_string) {
    f = "(" + f + ")" + verb + " " + filter;
  }
}

void cli_helper_base::remove_disabled_expressions() const {
  for (std::vector<std::string> *list : {&data.filter_string, &data.ok_string, &data.warn_string, &data.crit_string})
    list->erase(std::remove(list->begin(), list->end(), "none"), list->end());
}

bool cli_helper_base::answer_show_query() {
  if (!show_query_requested) return false;
  remove_disabled_expressions();
  nscapi::protobuf::functions::set_response_good(*response, render_query());
  return true;
}

void cli_helper_base::add_syntax(const std::string &default_top_syntax, const std::string &default_detail_syntax, const std::string &default_perf_syntax,
                                 const std::string &default_empty_syntax, const std::string &default_ok_syntax) {
  const std::string tk =
      "Top level syntax.\n"
      "Used to format the message to return can include text as well as special keywords which will include information from the checks.\n"
      "To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be "
      "difficult to escape on linux)." +
      common_option_marker;
  const std::string dk =
      "Detail level syntax.\n"
      "Used to format each resulting item in the message.\n"
      "%(list) will be replaced with all the items formatted by this syntax string in the top-syntax.\n"
      "To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be "
      "difficult to escape on linux)." +
      common_option_marker;
  const std::string pk =
      "Performance alias syntax.\n"
      "This is the syntax for the base names of the performance data." +
      common_option_marker;
  const std::string ek =
      "Empty syntax.\n"
      "DEPRECATED! This is the syntax for when nothing matches the filter." +
      common_option_marker;
  const std::string ok =
      "ok syntax.\n"
      "DEPRECATED! This is the syntax for when an ok result is returned.\n"
      "This value will not be used if your syntax contains %(list) or %(count)." +
      common_option_marker;

  // clang-format off
  desc.add_options()
    ("top-syntax", boost::program_options::value<std::string>(&data.syntax_top)->default_value(default_top_syntax), tk.c_str())
    ("ok-syntax", boost::program_options::value<std::string>(&data.syntax_ok)->default_value(default_ok_syntax), ok.c_str())
    ("empty-syntax", boost::program_options::value<std::string>(&data.syntax_empty)->default_value(default_empty_syntax), ek.c_str())
    ("detail-syntax", boost::program_options::value<std::string>(&data.syntax_detail)->default_value(default_detail_syntax), dk.c_str())
    ("perf-syntax", boost::program_options::value<std::string>(&data.syntax_perf)->default_value(default_perf_syntax), pk.c_str())
  ;
  // clang-format on
}

void cli_helper_base::add_index(const std::string &default_unique_syntax) {
  const std::string tk = "Unique syntax.\nUsed to filter unique items (counted will still increase but messages will not repeated)" + common_option_marker;

  desc.add_options()("unique-index", boost::program_options::value<std::string>(&data.syntax_unique)->default_value(default_unique_syntax), tk.c_str());
}

bool cli_helper_base::prepare_number_format(str::number_format &number_format) const {
  const std::string number_format_error = data.build_number_format(number_format);
  if (!number_format_error.empty()) {
    nscapi::protobuf::functions::set_response_bad(*response, number_format_error);
    return false;
  }
  return true;
}

void cli_helper_base::set_build_error(const std::string &message) const { nscapi::protobuf::functions::set_response_bad(*response, message); }

bool cli_helper_base::answer_built_query() const {
  if (!show_query_requested) return false;
  nscapi::protobuf::functions::set_response_good(*response, render_query());
  return true;
}

std::string cli_helper_base::escape_message(std::string msg) const {
  if (data.escape_html) {
    boost::replace_all(msg, "<", "&lt;");
    boost::replace_all(msg, ">", "&gt;");
  }
  return msg;
}

void cli_helper_base::set_result_code(const int ret_code) const {
  if (ret_code == NSCAPI::query_return_codes::returnOK) {
    response->set_result(PB::Common::ResultCode::OK);
  } else if (ret_code == NSCAPI::query_return_codes::returnWARN) {
    response->set_result(PB::Common::ResultCode::WARNING);
  } else if (ret_code == NSCAPI::query_return_codes::returnCRIT) {
    response->set_result(PB::Common::ResultCode::CRITICAL);
  } else {
    response->set_result(PB::Common::ResultCode::UNKNOWN);
  }
}

int cli_helper_base::empty_state_result(const bool has_matched, const int ret_code) const {
  if (data.empty_state != "ignored" && !has_matched) return nscapi::plugin_helper::translateReturn(data.empty_state);
  return ret_code;
}

}  // namespace modern_filter
