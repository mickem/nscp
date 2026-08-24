// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/noncopyable.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/functions_convert.hpp>
#include <nscapi/protobuf/functions_exec.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <str/format.hpp>
#include <str/utils_no_boost.hpp>

namespace modern_filter {
// Appended to the description of every option that is shared by all
// modern_filter based checks. docs_extract.py keys on this exact text to fold
// the option out of the per-command reference and into the shared
// common-options page -- keep the two in sync (see also the "Common option for
// all commands." marker in nscapi::program_options::add_help and the
// "Common option for all checks." marker on the generic filter keywords).
const std::string common_option_marker = "\nCommon option for all filter checks.";

struct data_container {
  std::vector<std::string> filter_string, warn_string, crit_string, ok_string;
  std::string syntax_empty, syntax_ok, syntax_top, syntax_detail, syntax_perf, perf_config, empty_state, syntax_unique, list_separator;
  bool debug, escape_html;
  // How the numbers of the message are rendered (issue #1428); -1 decimals is
  // "leave the rendering alone". These only ever touch the message: the
  // performance data is built from the raw values and stays locale neutral.
  int decimals;
  std::string byte_unit, decimal_separator, thousands_separator;
  // list_separator carries its default here as well as on the option, so a
  // check that builds a filter without registering the misc options still
  // joins lists with ", " instead of with nothing.
  data_container() : list_separator(", "), debug(false), escape_html(false), decimals(-1), decimal_separator(".") {}

  // The number format these options describe, or an error message when the
  // pinned unit is not one we know about.
  std::string build_number_format(str::number_format &fmt) const {
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
};

struct perf_writer final : perf_writer_interface {
  PB::Commands::QueryResponseMessage::Response::Line &line;
  explicit perf_writer(PB::Commands::QueryResponseMessage::Response::Line &line) : line(line) {}
  void write(const parsers::where::performance_data &data) override {
    PB::Common::PerformanceData *perf = line.add_perf();
    perf->set_alias(data.alias);
    if (data.float_value) {
      const parsers::where::performance_data::perf_value &value = *data.float_value;
      PB::Common::PerformanceData::FloatValue *perfData = perf->mutable_float_value();
      if (!data.unit.empty()) perfData->set_unit(data.unit);
      perfData->set_value(value.value);
      if (value.warn) perfData->mutable_warning()->set_value(*value.warn);
      if (value.crit) perfData->mutable_critical()->set_value(*value.crit);
      if (value.minimum) perfData->mutable_minimum()->set_value(*value.minimum);
      if (value.maximum) perfData->mutable_maximum()->set_value(*value.maximum);
    } else if (data.string_value) {
      const std::string value = *data.string_value;
      PB::Common::PerformanceData::StringValue *perfData = perf->mutable_string_value();
      perfData->set_value(value);
    }
  }
};

template <class T>
struct cli_helper : boost::noncopyable {
  data_container &data;
  boost::program_options::options_description desc;
  const PB::Commands::QueryRequestMessage::Request &request;
  PB::Commands::QueryResponseMessage::Response *response;
  bool show_all;
  nscapi::program_options::field_map fields;

  cli_helper(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, data_container &data)
      : data(data), desc("Allowed options for " + request.command()), request(request), response(response), show_all(false) {}

  ~cli_helper() {}

  boost::program_options::options_description &get_desc() { return desc; }

  void set_filter_syntax(const nscapi::program_options::field_map &fields_) { fields = fields_; }
  void add_filter_option(const std::string &filter) {
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
  void add_warn_option(const std::string &warn) {
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
  void add_warn_option(const std::string &warn1, const std::string &warn2) {
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
  void add_crit_option(const std::string &crit) {
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
  void add_crit_option(const std::string &crit1, const std::string &crit2) {
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
  void add_ok_option(const std::string &ok = "") {
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
  void add_misc_options(const std::string &empty_state = "ignored") {
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
  void add_options(const std::string &warn, const std::string &crit, const std::string &filter, const std::map<std::string, std::string> &filter_syntax,
                   const std::string &empty_state = "ignored") {
    set_filter_syntax(filter_syntax);
    add_filter_option(filter);
    add_warn_option(warn);
    add_crit_option(crit);
    add_ok_option();
    add_misc_options(empty_state);
  }
  void add_options(const std::map<std::string, std::string> &filter_syntax, const std::string &empty_state = "ignored") {
    set_filter_syntax(filter_syntax);
    add_ok_option();
    add_misc_options(empty_state);
  }

  void parse_options_post(const boost::program_options::variables_map &vm) const {
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

  bool parse_options(const boost::program_options::positional_options_description &p) const {
    boost::program_options::variables_map vm;
    if (!nscapi::program_options::process_arguments_from_request(vm, desc, fields, request, *response, p)) return false;
    parse_options_post(vm);
    return true;
  }
  bool parse_options() const {
    boost::program_options::variables_map vm;
    if (!nscapi::program_options::process_arguments_from_request(vm, desc, fields, request, *response)) return false;
    parse_options_post(vm);
    return true;
  }
  bool parse_options(std::vector<std::string> &extra) const {
    boost::program_options::variables_map vm;
    if (!nscapi::program_options::process_arguments_from_request(vm, desc, fields, request, *response, true, extra)) return false;
    parse_options_post(vm);
    return true;
  }

  bool empty() const { return data.warn_string.empty() && data.crit_string.empty(); }
  void append_all_filters(const std::string &verb, const std::string &filter) const {
    if (data.filter_string.empty()) {
      data.filter_string.push_back(filter);
    }
    for (std::string &f : data.filter_string) {
      f = "(" + f + ")" + verb + " " + filter;
    }
  }
  void set_default_index(const std::string &filter) const {
    if (data.syntax_unique.empty()) data.syntax_unique = filter;
  }
  bool build_filter(T &filter) {
    // The separator joins list items as they are collected, so it has to be in
    // place before the first match is recorded (start_match, below).
    filter.summary.list_separator = str::utils::unescape(data.list_separator);
    // Same story for the number format: every keyword getter reads it off the
    // evaluation context while the rows are rendered.
    str::number_format number_format;
    const std::string number_format_error = data.build_number_format(number_format);
    if (!number_format_error.empty()) {
      nscapi::protobuf::functions::set_response_bad(*response, number_format_error);
      return false;
    }
    filter.context->set_number_format(number_format);
    filter.set_human_number_format(!number_format.is_default());
    data.filter_string.erase(std::remove(data.filter_string.begin(), data.filter_string.end(), "none"), data.filter_string.end());
    data.ok_string.erase(std::remove(data.ok_string.begin(), data.ok_string.end(), "none"), data.ok_string.end());
    data.warn_string.erase(std::remove(data.warn_string.begin(), data.warn_string.end(), "none"), data.warn_string.end());
    data.crit_string.erase(std::remove(data.crit_string.begin(), data.crit_string.end(), "none"), data.crit_string.end());

    if (!filter.build_syntax(data.debug, data.syntax_top, data.syntax_detail, data.syntax_perf, data.perf_config, data.syntax_ok, data.syntax_empty)) {
      nscapi::protobuf::functions::set_response_bad(*response, "Failed to parse syntax");
      return false;
    }
    if (!data.syntax_unique.empty()) {
      std::string tmp_msg;
      if (!filter.build_index(data.syntax_unique, tmp_msg)) {
        nscapi::protobuf::functions::set_response_bad(*response, tmp_msg);
        return false;
      }
    }
    if (!filter.build_engines(data.debug, data.filter_string, data.ok_string, data.warn_string, data.crit_string)) {
      nscapi::protobuf::functions::set_response_bad(*response, "Failed to build engines");
      return false;
    }

    std::string error;
    if (!filter.validate(error)) {
      nscapi::protobuf::functions::set_response_bad(*response, "Failed to validate filter see log for details: " + error);
      return false;
    }

    filter.start_match();
    return true;
  }
  void set_default_perf_config(const std::string &conf) const { data.perf_config = conf; }
  void add_syntax(const std::string &default_top_syntax, const std::string &default_detail_syntax, const std::string &default_perf_syntax,
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

  void add_index(const std::string &default_unique_syntax) {
    const std::string tk =
        "Unique syntax.\nUsed to filter unique items (counted will still increase but messages will not repeated)" + common_option_marker;

    desc.add_options()("unique-index", boost::program_options::value<std::string>(&data.syntax_unique)->default_value(default_unique_syntax), tk.c_str());
  }

  void post_process(T &filter) {
    filter.match_post();
    PB::Commands::QueryResponseMessage::Response::Line *line = response->add_lines();
    perf_writer writer(*line);
    if ((data.empty_state != "ignored") && (!filter.summary.has_matched())) {
      filter.summary.returnCode = nscapi::plugin_helper::translateReturn(data.empty_state);
    }
    std::string msg = filter.get_message();
    if (data.escape_html) {
      boost::replace_all(msg, "<", "&lt;");
      boost::replace_all(msg, ">", "&gt;");
    }
    if (data.debug) {
      line->set_message(filter.context->get_debug());
    } else {
      line->set_message(msg);
    }
    filter.fetch_perf(&writer);
    int retCode = filter.summary.returnCode;
    if ((data.empty_state != "ignored") && (!filter.summary.has_matched())) retCode = nscapi::plugin_helper::translateReturn(data.empty_state);
    if (retCode == NSCAPI::query_return_codes::returnOK) {
      response->set_result(PB::Common::ResultCode::OK);
    } else if (retCode == NSCAPI::query_return_codes::returnWARN) {
      response->set_result(PB::Common::ResultCode::WARNING);
    } else if (retCode == NSCAPI::query_return_codes::returnCRIT) {
      response->set_result(PB::Common::ResultCode::CRITICAL);
    } else {
      response->set_result(PB::Common::ResultCode::UNKNOWN);
    }
  }
};
}  // namespace modern_filter