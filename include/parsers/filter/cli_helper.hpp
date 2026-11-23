// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/any.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/noncopyable.hpp>
#include <map>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/functions_convert.hpp>
#include <nscapi/protobuf/functions_exec.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <nscapi/protobuf/functions_status.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/filter/threshold_shorthand.hpp>
#include <sampling_state.hpp>
#include <set>
#include <sstream>
#include <str/format.hpp>
#include <str/utils_no_boost.hpp>
#include <string>
#include <vector>

// Everything in cli_helper that does not depend on the filter type lives in
// cli_helper_base and is compiled once, in cli_helper.cpp. Only build_filter()
// and post_process() touch the filter and stay in the template: a source file
// with several checks instantiates cli_helper<T> once per check, and with the
// whole helper inline that was enough to push check_webserver.cpp past the
// COFF section limit on MSVC (C1128). Keep it that way - a new member that does
// not need T goes in the base.

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
  // Status to answer with while the check's collector has not produced its
  // first sample; only checks that call add_warmup_option() read it.
  std::string warmup_state;
  bool warmup_state_given;
  bool debug, escape_html;
  // How the numbers of the message are rendered (issue #1428); -1 decimals is
  // "leave the rendering alone". These only ever touch the message: the
  // performance data is built from the raw values and stays locale neutral.
  int decimals;
  std::string byte_unit, decimal_separator, thousands_separator;
  // list_separator carries its default here as well as on the option, so a
  // check that builds a filter without registering the misc options still
  // joins lists with ", " instead of with nothing.
  data_container()
      : list_separator(", "), warmup_state("unknown"), warmup_state_given(false), debug(false), escape_html(false), decimals(-1), decimal_separator(".") {}

  // The number format these options describe, or an error message when the
  // pinned unit is not one we know about.
  std::string build_number_format(str::number_format &fmt) const;
};

struct perf_writer final : perf_writer_interface {
  PB::Commands::QueryResponseMessage::Response::Line &line;
  explicit perf_writer(PB::Commands::QueryResponseMessage::Response::Line &line) : line(line) {}
  void write(const parsers::where::performance_data &data) override;
};

struct cli_helper_base : boost::noncopyable {
  data_container &data;
  boost::program_options::options_description desc;
  const PB::Commands::QueryRequestMessage::Request &request;
  PB::Commands::QueryResponseMessage::Response *response;
  bool show_all;
  bool has_warmup_option;
  nscapi::program_options::field_map fields;
  // The -w / -c short thresholds (threshold_shorthand.hpp): which keyword a
  // bare "-w 80" bounds, and what was passed.
  shorthand::spec_list short_thresholds;
  std::vector<std::string> short_warn_templates, short_crit_templates;
  bool short_thresholds_declared;
  bool short_thresholds_registered;
  bool short_thresholds_disabled;
  // Whether "w" / "c" are the short thresholds, rather than an option of that
  // name the check declared for itself.
  bool short_warn_owned, short_crit_owned;
  std::vector<std::string> short_warn, short_crit;
  // show-query: whether it is ours (a check may have an option of that name),
  // whether it was asked for, and what it echoes besides the filter settings -
  // the other options as parse_options() saw them, and the arguments the
  // parser left to the check (check_pdh's counters).
  bool show_query_registered, show_query_owned, show_query_requested;
  std::map<std::string, std::vector<std::string> > query_options;
  std::vector<std::string> query_extra;

  cli_helper_base(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, data_container &data);

  boost::program_options::options_description &get_desc() { return desc; }

  void set_filter_syntax(const nscapi::program_options::field_map &fields_) { fields = fields_; }
  void add_filter_option(const std::string &filter);
  void add_warn_option(const std::string &warn);
  void add_warn_option(const std::string &warn1, const std::string &warn2);
  void add_crit_option(const std::string &crit);
  void add_crit_option(const std::string &crit1, const std::string &crit2);
  void add_ok_option(const std::string &ok = "");
  void add_misc_options(const std::string &empty_state = "ignored");
  void add_options(const std::string &warn, const std::string &crit, const std::string &filter, const std::map<std::string, std::string> &filter_syntax,
                   const std::string &empty_state = "ignored");
  void add_options(const std::map<std::string, std::string> &filter_syntax, const std::string &empty_state = "ignored");

  // Says what "-w 80" means for this check: `variable` is the keyword a bare
  // number bounds and `dir` the side it alerts on, so ("load", above) reads
  // "-w 80" as "warning=load > 80". `warn_default` and `crit_default` are what
  // the check's own defaults amount to in the short form ("80", "90"): they
  // are shown as the defaults of -w and -c, and fill in for a value that is
  // left out of a list. Call it again for a check whose standard counterpart
  // takes a list ("-w 100,20%" for a ping is time, then loss).
  // A check that never calls it bounds the number of matched items (`count`),
  // which every filter check has. Can be called before or after add_options().
  void add_short_threshold(const std::string &variable, shorthand::direction dir, shorthand::unit value_unit = shorthand::unit_plain,
                           const std::string &warn_default = "", const std::string &crit_default = "");

  // What -w / -c stand for when the threshold is more than a bound: a
  // template in which ${keyword} is the bound on that keyword, for instance
  // "${time} or result != 'ok'" or "decisions >= min_requests and
  // ${reject_pct}" (see threshold_shorthand.hpp). Without one the short form
  // is the bound alone. This is the whole statement of what the short form
  // means - nothing is taken from the text of the default expression - so it
  // has to carry every condition and guard that should survive -w / -c.
  // Call it more than once for a check with several expressions on a side.
  void add_short_warning(const std::string &text) { short_warn_templates.push_back(text); }
  void add_short_critical(const std::string &text) { short_crit_templates.push_back(text); }

  // For the few users of this helper that are not a check of anything (the
  // CheckHelpers commands that reshape another check's result): they get no
  // -w / -c at all.
  void disable_short_thresholds() { short_thresholds_disabled = true; }

  // Registered on the way into parse_options() rather than with the other
  // options, so that a check can declare its keyword after add_options().
  void register_short_thresholds();

  // show-query, registered on the way into parse_options() like -w / -c, so
  // that it is there whatever the check added after add_options(), and so
  // that show-default (rendered before it) does not list it. Takes a value
  // like every other boolean here: REST passes a flag as "show-query=true".
  void register_show_query();

  // How the help ends its "is the same as ..." sentence. With a template the
  // expression is the default with another number in it, and saying that it
  // replaces the default would contradict the conditions printed right there.
  static std::string short_effect(const std::string &option, const std::vector<std::string> &templates);

  // Whether the request asks for the flag `name`: bare, or with a value as
  // REST passes it ("name=true"), in either shape. An abbreviation counts when
  // it can only mean this option ("--show-def"): the option parser guesses
  // those too, and one that got past us would be answered by it instead -
  // after -w / -c are registered, which is the listing show-default must not
  // print.
  bool is_requested(const std::string &name) const;

  // Whether `key` is a prefix of `name` that no other option shares, nor is
  // itself the name of one.
  bool is_abbreviation_of(const std::string &key, const std::string &name) const;

  // show-default, answered here rather than by the option parser, and the
  // options it must not list registered right after. "w=80" next to
  // "warning=load > 80" reads as one line to copy, and whoever then edits only
  // the warning is left with the short form adding the old bound back as a
  // second expression; "show-query=false" has no place in such a line either.
  // So show-default prints what it printed before either existed. The values
  // as they resolve are what show-query prints. Returns true when it answered.
  bool answer_default_queries();

  // ---- show-query -------------------------------------------------------

  // Options show-query never echoes: the ones that ask for something other
  // than a run, the aliases folded into warning / critical, and -w / -c, which
  // are already resolved into warning / critical by the time it renders. A
  // "w" or "c" the check declared for itself is the check's and is echoed.
  bool is_query_meta_option(const std::string &name) const;

  // The settings of the filter engine, read off `data` when show-query
  // renders rather than off the parsed options: a check may rewrite any of
  // them between parse_options() and build_filter(). Returns false for any
  // other option.
  bool final_filter_setting(const std::string &name, std::vector<std::string> &values) const;
  static const std::vector<std::string> &filter_setting_names();

  // An option value as it is typed: booleans as true / false (as help and
  // show-default render them), a list as one value per element. Returns false
  // for a type it does not know, which is then left out.
  static bool render_option_value(const boost::any &value, std::vector<std::string> &out);

  // Whether the value of option `name` may be a credential. The rule of
  // failed_check_store::is_sensitive_argument (password, token), plus the
  // options of the filter checks that can carry one inside a longer value: a
  // shared secret, an ODBC connection string (PWD=), an HTTP header
  // (Authorization). Wide on purpose: masking a harmless value costs a
  // retyped argument, printing a secret cannot be taken back.
  static bool is_sensitive_option(const std::string &name);

  // Called by parse_options() once the arguments are parsed: whether
  // show-query was asked for, and if so the values of every option that is
  // not a filter setting, given or defaulted, as they were parsed.
  void remember_query(const boost::program_options::variables_map &vm, const std::vector<std::string> &extra = std::vector<std::string>());

  // The query show-query prints, in the shape of show-default ("key=value"
  // tokens) and in the order the options are declared. The filter settings
  // are the final ones; one that a check set without declaring its option
  // follows the declared ones. A filter, warning, critical or ok that ended up
  // empty although the option has a default is printed as "none", which is
  // how the check is told to run without it: left out, the line would bring
  // the default back.
  std::string render_query() const;

  // The option carries the short form of the check's own default as its
  // default value, so that help and the reference docs show "-w 80" next to
  // "warning=load > 80". show-default deliberately does not, see
  // answer_default_queries().
  boost::program_options::typed_value<std::vector<std::string> > *short_option(std::vector<std::string> &store, shorthand::side which) const;

  // The request with "-w 80" rewritten into the shape the rest of its
  // arguments have, see shorthand::normalize_arguments().
  PB::Commands::QueryRequestMessage::Request normalized_request() const;

  // Refuses a -w / -c that was given no value ("-w -c 90", a trailing "-w",
  // "w="), in the same words whatever shape the arguments have. Returns false
  // when it answered.
  bool check_short_values(const PB::Commands::QueryRequestMessage::Request &normalized);

  // Turns what -w / -c were given into expressions. A short threshold takes
  // the place of the check's default for its side - the template says what
  // that is - and next to an explicit warning= it is one more expression.
  bool apply_short_thresholds(const boost::program_options::variables_map &vm);
  bool apply_short_threshold(const boost::program_options::variables_map &vm, const std::string &short_name, const std::string &option, shorthand::side which,
                             const std::vector<std::string> &values, const std::vector<std::string> &templates, std::vector<std::string> &target);

  // Opt-in for checks that read a background collector: before its first
  // sample there is nothing to evaluate, and this picks the status reported
  // meanwhile. Not a common option - most filter checks have no warm-up.
  void add_warmup_option();

  // Answer with the warm-up status and `message` instead of evaluating the
  // filter. For checks without add_warmup_option() this is UNKNOWN.
  void set_warmup_response(const std::string &message) const;

  // Answers for a collector-backed check whose buffer holds nothing to
  // evaluate, and returns true; returns false without answering when there is
  // data. `what` names the data in the message ("CPU", "memory", ...). Only a
  // collector that has not tried yet is warming up: one that tried and came
  // back empty-handed is UNKNOWN whatever warmup-state says.
  bool answer_unless_sampled(const sampling::status &status, const std::string &what) const;

  // The vocabulary of every other status option (parse_nagios), plus the
  // explicit spelling of the default it maps everything else to.
  static bool is_valid_warmup_state(const std::string &state);
  static PB::Common::ResultCode warmup_result(const std::string &state);

  // Whether warmup-state was passed rather than defaulted, for a check whose
  // current data source cannot tell warm-up apart and has to refuse it.
  bool warmup_state_given() const { return data.warmup_state_given; }

  // Rejected up front: a typo would otherwise only surface as UNKNOWN during
  // the next warm-up, long after the check was configured.
  bool validate_options() const;

  void parse_options_post(const boost::program_options::variables_map &vm) const;

  bool parse_options(const boost::program_options::positional_options_description &p);
  bool parse_options();
  bool parse_options(std::vector<std::string> &extra);

  bool empty() const { return data.warn_string.empty() && data.crit_string.empty(); }
  void append_all_filters(const std::string &verb, const std::string &filter) const;
  void set_default_index(const std::string &filter) const {
    if (data.syntax_unique.empty()) data.syntax_unique = filter;
  }
  // "none" is how an expression is switched off; the engines never see it.
  void remove_disabled_expressions() const;

  // For a check that has to do its work before it can build the filter: one
  // whose keywords are the columns of a query it runs (check_wmi,
  // check_mysql_query, check_mssql_query). Called after parse_options() and
  // before that work, it answers show-query and returns true, so that asking
  // what a check would run never runs it. The expressions are then not
  // validated - their keywords are not known yet. Every other check leaves
  // show-query to build_filter().
  bool answer_show_query();

  void set_default_perf_config(const std::string &conf) const { data.perf_config = conf; }
  void add_syntax(const std::string &default_top_syntax, const std::string &default_detail_syntax, const std::string &default_perf_syntax,
                  const std::string &default_empty_syntax, const std::string &default_ok_syntax);
  void add_index(const std::string &default_unique_syntax);

 protected:
  // The parts of build_filter() that do not need the filter. Each answers the
  // request and returns false when the check has to stop there.

  // Checks the number format options and hands back the format the filter
  // renders the message with.
  bool prepare_number_format(str::number_format &number_format) const;
  // Reports why the filter could not be built.
  void set_build_error(const std::string &message) const;
  // Answers show-query once the expressions are parsed and validated; returns
  // true when it did, and the check must not run.
  bool answer_built_query() const;

  // The parts of post_process() that do not need the filter: the message as
  // it is sent (HTML escaped if asked) and the Nagios status for `ret_code`.
  std::string escape_message(std::string msg) const;
  void set_result_code(int ret_code) const;
  // The status empty-state maps an unmatched filter to, or `ret_code` when
  // the filter matched or the state is "ignored".
  int empty_state_result(bool has_matched, int ret_code) const;
};

template <class T>
struct cli_helper : cli_helper_base {
  cli_helper(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, data_container &data)
      : cli_helper_base(request, response, data) {}

  bool build_filter(T &filter) {
    // The separator joins list items as they are collected, so it has to be in
    // place before the first match is recorded (start_match, below).
    filter.summary.list_separator = str::utils::unescape(data.list_separator);
    // Same story for the number format: every keyword getter reads it off the
    // evaluation context while the rows are rendered.
    str::number_format number_format;
    if (!prepare_number_format(number_format)) return false;
    filter.context->set_number_format(number_format);
    filter.set_human_number_format(!number_format.is_default());
    remove_disabled_expressions();

    if (!filter.build_syntax(data.debug, data.syntax_top, data.syntax_detail, data.syntax_perf, data.perf_config, data.syntax_ok, data.syntax_empty)) {
      set_build_error("Failed to parse syntax");
      return false;
    }
    if (!data.syntax_unique.empty()) {
      std::string tmp_msg;
      if (!filter.build_index(data.syntax_unique, tmp_msg)) {
        set_build_error(tmp_msg);
        return false;
      }
    }
    if (!filter.build_engines(data.debug, data.filter_string, data.ok_string, data.warn_string, data.crit_string)) {
      set_build_error("Failed to build engines");
      return false;
    }

    std::string error;
    if (!filter.validate(error)) {
      set_build_error("Failed to validate filter see log for details: " + error);
      return false;
    }

    // show-query is answered here, not in parse_options(): this is the query
    // as it runs, after whatever the check changed in between, and after the
    // expressions were parsed and validated like on a real run.
    if (answer_built_query()) return false;

    filter.start_match();
    return true;
  }

  void post_process(T &filter) {
    filter.match_post();
    PB::Commands::QueryResponseMessage::Response::Line *line = response->add_lines();
    perf_writer writer(*line);
    filter.summary.returnCode = empty_state_result(filter.summary.has_matched(), filter.summary.returnCode);
    std::string msg = filter.get_message();
    int retCode = empty_state_result(filter.summary.has_matched(), filter.summary.returnCode);
    // An expression that failed on a record gave that record no verdict. A
    // WARNING or CRITICAL the other records reached stands, with the failure
    // beside it; anything else could be the failure hiding a problem, so the
    // check is UNKNOWN and says why.
    if (filter.has_evaluation_errors()) {
      if (retCode == NSCAPI::query_return_codes::returnWARN || retCode == NSCAPI::query_return_codes::returnCRIT) {
        msg = filter.with_evaluation_failure(msg);
      } else {
        retCode = NSCAPI::query_return_codes::returnUNKNOWN;
        msg = filter.get_evaluation_failure();
      }
    }
    if (data.debug) {
      line->set_message(filter.context->get_debug());
    } else {
      line->set_message(escape_message(msg));
    }
    filter.fetch_perf(&writer);
    set_result_code(retCode);
  }
};
}  // namespace modern_filter
