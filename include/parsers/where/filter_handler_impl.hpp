// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <NSCAPI.h>

#include <boost/function.hpp>
#include <boost/optional.hpp>
#include <boost/unordered_map.hpp>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <nscapi/nscapi_helper.hpp>
#include <parsers/where/engine_impl.hpp>
#include <parsers/where/helpers.hpp>
#include <parsers/where/variable.hpp>
#include <str/format.hpp>
#include <string>

namespace parsers {
namespace where {

template <class T>
struct function_registry;

template <class T>
struct filter_variable {
  std::string name;
  value_type type;
  std::string description;
  typedef std::shared_ptr<number_performance_generator_interface<T, long long>> int_perf_generator_type;
  typedef std::shared_ptr<number_performance_generator_interface<T, double>> float_perf_generator_type;
  typedef boost::function<std::string(T, evaluation_context)> str_fun_type;
  typedef boost::function<std::string(T)> str_fun_type_no_context;
  typedef boost::function<long long(T, evaluation_context)> int_fun_type;
  typedef boost::function<long long(T)> int_fun_type_no_context;
  typedef boost::function<double(T, evaluation_context)> float_fun_type;
  typedef boost::function<boost::optional<long long>(T, evaluation_context)> opt_int_fun_type;
  str_fun_type s_function;
  int_fun_type i_function;
  float_fun_type f_function;
  // Optional number: when set, this variable is created as an
  // optional_int_variable_node and o_function/no_value take precedence over
  // the plain accessors above. no_value is the string the variable renders
  // (and compares equal to) when the getter returns an empty optional.
  opt_int_fun_type o_function;
  std::string no_value;
  std::list<int_perf_generator_type> int_perf;
  std::list<float_perf_generator_type> float_perf;
  bool add_default_perf;
  void set_no_perf() { add_default_perf = false; }

  filter_variable(const std::string& name, const value_type type, const std::string& description)
      : name(name), type(type), description(description), add_default_perf(true) {}
};
template <class T>
struct filter_converter : binary_function_impl {
  value_type type;
  typedef boost::function<node_type(T, evaluation_context, node_type)> converter_fun_type;
  typedef boost::function<node_type(T, node_type)> converter_fun_type_no_context;
  converter_fun_type function;
  filter_converter(const value_type type, converter_fun_type function) : type(type), function(function) {}
  explicit filter_converter(const value_type type) : type(type) {}

  node_type evaluate(value_type type, evaluation_context context, node_type subject) const override;
};

struct filter_function {
  std::string name;
  std::string description;
  typedef boost::function<node_type(value_type, evaluation_context, node_type)> generic_fun_type;
  generic_fun_type function;

  value_type type;
  explicit filter_function(const std::string& name) : name(name), type(type_tbd) {}
};

template <class T>
struct registry_adders_variables_int {
  typedef typename filter_variable<T>::int_perf_generator_type perf_generator_type;

  explicit registry_adders_variables_int(function_registry<T>* owner_, bool human = false) : owner(owner_), human(human) {}

  registry_adders_variables_int& operator()(std::string key, typename filter_variable<T>::int_fun_type i_fun, typename filter_variable<T>::str_fun_type s_fun,
                                            std::string description) {
    std::shared_ptr<filter_variable<T>> var(new filter_variable<T>(key, type_int, description));
    var->s_function = s_fun;
    var->i_function = i_fun;
    add_variables(var);
    return *this;
  }
  registry_adders_variables_int& operator()(std::string key, value_type type, typename filter_variable<T>::int_fun_type i_fun,
                                            typename filter_variable<T>::str_fun_type s_fun, std::string description) {
    std::shared_ptr<filter_variable<T>> var(new filter_variable<T>(key, type, description));
    var->i_function = i_fun;
    var->s_function = s_fun;
    add_variables(var);
    return *this;
  }
  registry_adders_variables_int& operator()(std::string key, value_type type, typename filter_variable<T>::int_fun_type i_fun, std::string description) {
    std::shared_ptr<filter_variable<T>> var(new filter_variable<T>(key, type, description));
    var->i_function = i_fun;
    add_variables(var);
    return *this;
  }
  registry_adders_variables_int& add_int(std::string key, value_type type, typename filter_variable<T>::int_fun_type_no_context i_fun,
                                         std::string description) {
    std::shared_ptr<filter_variable<T>> var(new filter_variable<T>(key, type, description));
    var->i_function = [i_fun](auto obj, auto) { return i_fun(obj); };
    add_variables(var);
    return *this;
  }
  registry_adders_variables_int& operator()(std::string key, typename filter_variable<T>::int_fun_type i_fun, std::string description) {
    std::shared_ptr<filter_variable<T>> var(new filter_variable<T>(key, type_int, description));
    var->i_function = i_fun;
    add_variables(var);
    return *this;
  }
  registry_adders_variables_int& add_int(std::string key, typename filter_variable<T>::int_fun_type_no_context i_fun, std::string description) {
    std::shared_ptr<filter_variable<T>> var(new filter_variable<T>(key, type_int, description));
    var->i_function = [i_fun](auto obj, auto) { return i_fun(obj); };
    add_variables(var);
    return *this;
  }
  registry_adders_variables_int& add_perf(std::string unit = "", std::string prefix = "", std::string suffix = "") {
    get_last()->int_perf.push_back(perf_generator_type(new simple_number_performance_generator<T, long long>(unit, prefix, suffix)));
    return *this;
  }
  registry_adders_variables_int& no_perf() {
    get_last()->set_no_perf();
    return *this;
  }
  typedef boost::function<long long(T, evaluation_context)> maxfun_type;
  registry_adders_variables_int& add_percentage(maxfun_type maxfun, std::string prefix = "", std::string suffix = "") {
    get_last()->int_perf.push_back(perf_generator_type(new percentage_int_performance_generator<T>(maxfun, prefix, suffix)));
    return *this;
  }

  typedef boost::function<long long(T, evaluation_context)> scale_type;
  registry_adders_variables_int& add_scaled_byte(std::string prefix = "", std::string suffix = "") {
    get_last()->int_perf.push_back(perf_generator_type(new scaled_byte_int_performance_generator<T>(prefix, suffix)));
    return *this;
  }
  registry_adders_variables_int& add_scaled_byte(scale_type minfun, scale_type maxfun, std::string prefix = "", std::string suffix = "") {
    get_last()->int_perf.push_back(perf_generator_type(new scaled_byte_int_performance_generator<T>(minfun, maxfun, prefix, suffix)));
    return *this;
  }
  registry_adders_variables_int& add_scaled_byte(scale_type maxfun, std::string prefix = "", std::string suffix = "") {
    get_last()->int_perf.push_back(perf_generator_type(new scaled_byte_int_performance_generator<T>(maxfun, prefix, suffix)));
    return *this;
  }

 private:
  std::shared_ptr<filter_variable<T>> get_last();
  void add_variables(std::shared_ptr<filter_variable<T>> d);
  function_registry<T>* owner;
  bool human;
};

template <class T>
struct function_registry {
  typedef std::map<std::string, std::shared_ptr<filter_variable<T>>> variable_type;
  typedef std::map<std::string, std::shared_ptr<filter_function>> function_type;
  typedef std::map<value_type, std::shared_ptr<filter_converter<T>>> converter_type;
  variable_type variables;
  variable_type human_variables;
  function_type functions;
  converter_type converters;
  std::shared_ptr<filter_variable<T>> last_variable;

  function_registry<T>& add_int_var(std::string key, std::function<long long(T)> i_fun, std::string description) {
    std::shared_ptr<filter_variable<T>> var(new filter_variable<T>(key, type_int, description));
    var->i_function = [i_fun](auto obj, auto) { return i_fun(obj); };
    add(var, false);
    return *this;
  }
  function_registry<T>& add_int_var(std::string key, std::function<long long(T)> i_fun, std::function<std::string(T)> s_fun, std::string description) {
    std::shared_ptr<filter_variable<T>> var(new filter_variable<T>(key, type_int, description));
    var->i_function = [i_fun](auto obj, auto) { return i_fun(obj); };
    var->s_function = [s_fun](auto obj, auto) { return s_fun(obj); };
    add(var, false);
    return *this;
  }
  function_registry<T>& add_int_var(std::string key, value_type type, std::function<long long(T)> i_fun, std::string description) {
    std::shared_ptr<filter_variable<T>> var(new filter_variable<T>(key, type, description));
    var->i_function = [i_fun](auto obj, auto) { return i_fun(obj); };
    add(var, false);
    return *this;
  }
  function_registry<T>& add_int_var(std::string key, value_type type, std::function<long long(T)> i_fun, std::function<std::string(T)> s_fun,
                                    std::string description) {
    std::shared_ptr<filter_variable<T>> var(new filter_variable<T>(key, type, description));
    var->i_function = [i_fun](auto obj, auto) { return i_fun(obj); };
    var->s_function = [s_fun](auto obj, auto) { return s_fun(obj); };
    add(var, false);
    return *this;
  }
  // An optional number: a variable that can have no value. `fun` returns an
  // empty optional when there is nothing to report; the variable then renders
  // as `no_value` (which is also what it compares equal to as a string, e.g.
  // `jitter = 'unknown'`), every numeric comparison on it is false, and no
  // perfdata is emitted. Chain .add_int_perf()/.no_perf() as usual.
  function_registry<T>& add_optional_int_var(std::string key, std::function<boost::optional<long long>(T)> fun, std::string no_value,
                                             std::string description) {
    return add_optional_int_var(key, type_int, fun, no_value, description);
  }
  function_registry<T>& add_optional_int_var(std::string key, value_type type, std::function<boost::optional<long long>(T)> fun, std::string no_value,
                                             std::string description) {
    std::shared_ptr<filter_variable<T>> var(new filter_variable<T>(key, type, description));
    var->o_function = [fun](auto obj, auto) { return fun(obj); };
    var->no_value = no_value;
    add(var, false);
    return *this;
  }
  // Context-taking form of add_optional_int_var, for optional values that can
  // only be computed against the evaluation context (e.g. check_drivesize's
  // full_in, which projects from the lazily-fetched current free space).
  function_registry<T>& add_optional_int_var_w_context(std::string key, value_type type, typename filter_variable<T>::opt_int_fun_type fun,
                                                       std::string no_value, std::string description) {
    std::shared_ptr<filter_variable<T>> var(new filter_variable<T>(key, type, description));
    var->o_function = fun;
    var->no_value = no_value;
    add(var, false);
    return *this;
  }
  function_registry<T>& add_int_var_w_context(std::string key, std::function<long long(T, evaluation_context)> i_fun, std::string description) {
    std::shared_ptr<filter_variable<T>> var(new filter_variable<T>(key, type_int, description));
    var->i_function = i_fun;
    add(var, false);
    return *this;
  }
  function_registry<T>& add_int_var_w_context(std::string key, value_type type, std::function<long long(T, evaluation_context)> i_fun,
                                              std::string description) {
    std::shared_ptr<filter_variable<T>> var(new filter_variable<T>(key, type, description));
    var->i_function = i_fun;
    add(var, false);
    return *this;
  }
  function_registry<T>& add_numbers(std::string key, value_type type, std::function<long long(T)> i_fun, std::function<double(T)> f_fun,
                                    std::string description) {
    std::shared_ptr<filter_variable<T>> var(new filter_variable<T>(key, type, description));
    var->i_function = [i_fun](auto obj, auto) { return i_fun(obj); };
    var->f_function = [f_fun](auto obj, auto) { return f_fun(obj); };
    add(var, false);
    return *this;
  }
  function_registry<T>& add_float(std::string key, std::function<double(T)> f_fun, std::string description) {
    std::shared_ptr<filter_variable<T>> var(new filter_variable<T>(key, type_float, description));
    var->f_function = [f_fun](auto obj, auto) { return f_fun(obj); };
    add(var, false);
    return *this;
  }

  registry_adders_variables_int<T> add_int_legacy() { return registry_adders_variables_int<T>(this); }
  function_registry<T>& add_string_var(std::string key, std::function<std::string(T)> s_fun, std::string description) {
    std::shared_ptr<filter_variable<T>> var(new filter_variable<T>(key, type_string, description));
    var->s_function = [s_fun](auto obj, auto context) { return s_fun(obj); };
    add(var, false);
    return *this;
  }
  function_registry<T>& add_string_var_w_context(std::string key, std::function<std::string(T, evaluation_context)> s_fun, std::string description) {
    std::shared_ptr<filter_variable<T>> var(new filter_variable<T>(key, type_string, description));
    var->s_function = [s_fun](auto obj, auto context) { return s_fun(obj, context); };
    add(var, false);
    return *this;
  }
  function_registry<T>& add_string_var(std::string key, std::function<std::string(T)> s_fun, std::function<long long(T)> i_fun, std::string description) {
    std::shared_ptr<filter_variable<T>> var(new filter_variable<T>(key, type_string, description));
    var->s_function = [s_fun](auto obj, auto context) { return s_fun(obj); };
    var->i_function = [i_fun](auto obj, auto context) { return i_fun(obj); };
    add(var, false);
    return *this;
  }
  function_registry<T>& add_human_string(std::string key, std::function<std::string(T)> s_fun, std::string description) {
    std::shared_ptr<filter_variable<T>> var(new filter_variable<T>(key, type_string, description));
    var->s_function = [s_fun](auto obj, auto context) { return s_fun(obj); };
    add(var, true);
    return *this;
  }
  function_registry<T>& add_human_string_context(std::string key, std::function<std::string(T, evaluation_context)> s_fun, std::string description) {
    std::shared_ptr<filter_variable<T>> var(new filter_variable<T>(key, type_string, description));
    var->s_function = s_fun;
    add(var, true);
    return *this;
  }
  function_registry<T>& add_custom_fun(const std::string& key, const value_type type_, const filter_function::generic_fun_type& fun,
                                       const std::string& description) {
    const auto var = std::make_shared<filter_function>(key);
    var->function = fun;
    var->type = type_;
    var->description = description;
    add(var);
    return *this;
  }
  function_registry<T>& add_string_fun(const std::string& key, const filter_function::generic_fun_type& fun, const std::string& description) {
    return add_custom_fun(key, type_string, fun, description);
  }
  function_registry<T>& add_int_fun(const std::string& key, const filter_function::generic_fun_type& fun, const std::string& description) {
    return add_custom_fun(key, type_int, fun, description);
  }

  function_registry<T>& add_converter(value_type type, typename filter_converter<T>::converter_fun_type fun) {
    std::shared_ptr<filter_converter<T>> var(new filter_converter<T>(type, fun));
    add(var);
    return *this;
  }

  typedef typename filter_variable<T>::int_perf_generator_type int_perf_generator_type;
  function_registry<T>& add_int_perf(std::string unit = "", std::string prefix = "", std::string suffix = "") {
    get_last_variable()->int_perf.push_back(int_perf_generator_type(new simple_number_performance_generator<T, long long>(unit, prefix, suffix)));
    return *this;
  }
  typedef typename filter_variable<T>::float_perf_generator_type float_perf_generator_type;
  function_registry<T>& add_float_perf(std::string unit = "", std::string prefix = "", std::string suffix = "") {
    get_last_variable()->float_perf.push_back(float_perf_generator_type(new simple_number_performance_generator<T, double>(unit, prefix, suffix)));
    return *this;
  }
  function_registry<T>& no_perf() {
    get_last_variable()->set_no_perf();
    return *this;
  }

  bool has_converter(const value_type type) const { return converters.find(type) != converters.end(); }
  bool has_variable(const std::string& key) const { return variables.find(key) != variables.end() || human_variables.find(key) != human_variables.end(); }
  bool has_function(const std::string& key) const { return functions.find(key) != functions.end(); }
  std::shared_ptr<filter_variable<T>> get_variable(const std::string& key, bool human_readable) const {
    if (human_readable) {
      typename variable_type::const_iterator cit = human_variables.find(key);
      if (cit != human_variables.end()) {
        return cit->second;
      }
    }
    typename variable_type::const_iterator cit = variables.find(key);
    if (cit != variables.end()) {
      return cit->second;
    }
    return std::make_shared<filter_variable<T>>("dummy", type_tbd, "dummy");
  }
  std::shared_ptr<filter_converter<T>> get_converter(const value_type type) const {
    typename converter_type::const_iterator cit = converters.find(type);
    if (cit != converters.end()) {
      return cit->second;
    }
    return std::make_shared<filter_converter<T>>(type_tbd);
  }
  std::list<std::string> get_variables() const {
    std::list<std::string> ret;
    for (const typename variable_type::value_type& v : variables) {
      ret.push_back(v.first);
    }
    return ret;
  }
  std::shared_ptr<filter_function> get_function(const std::string& key) const {
    function_type::const_iterator cit = functions.find(key);
    if (cit != functions.end()) {
      return cit->second;
    }
    return std::make_shared<filter_function>("dummy");
  }
  void add(std::shared_ptr<filter_variable<T>> d, bool human) {
    if (human)
      human_variables[d->name] = d;
    else
      variables[d->name] = d;
    last_variable = d;
  }
  std::shared_ptr<filter_variable<T>> get_last_variable() { return last_variable; }
  void add(const std::shared_ptr<filter_function>& d) { functions[d->name] = d; }
  void add(std::shared_ptr<filter_converter<T>> d) { converters[d->type] = d; }
};

template <class T>
void registry_adders_variables_int<T>::add_variables(std::shared_ptr<filter_variable<T>> d) {
  owner->add(d, human);
}
template <class T>
std::shared_ptr<filter_variable<T>> registry_adders_variables_int<T>::get_last() {
  return owner->get_last_variable();
}

template <class TObject>
struct generic_summary {
  long long count_match;
  long long count_ok;
  long long count_warn;
  long long count_crit;
  long long count_total;
  std::string list_match;
  std::string list_ok;
  std::string list_crit;
  std::string list_warn;
  std::string list_problem;
  NSCAPI::nagiosReturn returnCode;
  // What joins the items of %(list), %(ok_list), %(warn_list), %(crit_list),
  // %(problem_list) and %(detail_list). Configuration, not state: reset()
  // leaves it alone. Set from the `list-separator` option (issue #1370) so a
  // result with many items can be rendered one per line, which is what a
  // Nagios-compatible frontend wants - first line summary, rest long output.
  std::string list_separator;

  generic_summary()
      : count_match(0), count_ok(0), count_warn(0), count_crit(0), count_total(0), returnCode(NSCAPI::query_return_codes::returnOK), list_separator(", ") {}

  void move_hits_crit() {
    list_crit = list_match;
    list_problem = list_match;
  }
  void move_hits_warn() {
    list_warn = list_match;
    list_problem = list_match;
  }
  void reset() {
    count_match = count_ok = count_warn = count_crit = count_total = 0;
    // list_problem too: the real-time path reuses one filter instance and
    // resets it per event batch, so a list left behind here leaks previous
    // batches' items into every later %(problem_list).
    list_match = list_ok = list_warn = list_crit = list_problem = "";
    returnCode = NSCAPI::query_return_codes::returnOK;
  }
  void count() { count_total++; }
  void matched(const std::string& line) {
    str::format::append_list(list_match, line, list_separator);
    count_match++;
  }
  void matched_unique() { count_match++; }
  bool has_matched() const { return count_match > 0; }
  void matched_ok(const std::string& line) {
    str::format::append_list(list_ok, line, list_separator);
    count_ok++;
  }
  void matched_warn(const std::string& line) {
    str::format::append_list(list_warn, line, list_separator);
    str::format::append_list(list_problem, line, list_separator);
    count_warn++;
  }
  void matched_crit(const std::string& line) {
    str::format::append_list(list_crit, line, list_separator);
    str::format::append_list(list_problem, line, list_separator);
    count_crit++;
  }
  void matched_ok_unique() { count_ok++; }
  void matched_warn_unique() { count_warn++; }
  void matched_crit_unique() { count_crit++; }

  std::string get_status() const { return nscapi::plugin_helper::translateReturn(returnCode); }
  std::string get_list_separator() { return list_separator; }
  std::string get_list_match() { return list_match; }
  std::string get_list_ok() { return list_ok; }
  std::string get_list_warn() { return list_warn; }
  std::string get_list_crit() { return list_crit; }
  std::string get_list_problem() { return list_problem; }
  // The severity groups of %(detail_list) are joined with the same separator
  // as the items inside them: with a newline separator the groups have to
  // break too, or the "critical(...)" wrapper would hold a multi-line block
  // while the groups themselves stayed on one line.
  void append_list(std::string& result, const std::string& tag, const std::string& value) const {
    if (!value.empty()) {
      if (!result.empty()) result += list_separator;
      result += tag + "(" + value + ")";
    }
  }
  std::string get_list_detail() const {
    std::string ret;
    append_list(ret, "critical", list_crit);
    append_list(ret, "warning", list_warn);
    str::format::append_list(ret, list_ok, list_separator);
    return ret;
  }
  long long get_count_match() const { return count_match; }
  long long get_count_ok() const { return count_ok; }
  long long get_count_warn() const { return count_warn; }
  long long get_count_crit() const { return count_crit; }
  long long get_count_problem() const { return count_warn + count_crit; }
  long long get_count_total() const { return count_total; }
  std::map<std::string, std::string> get_filter_syntax() const {
    std::map<std::string, std::string> ret;
    ret["count"] = "Number of items matching the filter. Common option for all checks.";
    ret["total"] = "Total number of items. Common option for all checks.";
    ret["ok_count"] = "Number of items matched the ok criteria. Common option for all checks.";
    ret["warn_count"] = "Number of items matched the warning criteria. Common option for all checks.";
    ret["crit_count"] = "Number of items matched the critical criteria. Common option for all checks.";
    ret["problem_count"] = "Number of items matched either warning or critical criteria. Common option for all checks.";
    ret["list"] = "A list of all items which matched the filter. Common option for all checks.";
    ret["ok_list"] = "A list of all items which matched the ok criteria. Common option for all checks.";
    ret["warn_list"] = "A list of all items which matched the warning criteria. Common option for all checks.";
    ret["crit_list"] = "A list of all items which matched the critical criteria. Common option for all checks.";
    ret["problem_list"] = "A list of all items which matched either the critical or the warning criteria. Common option for all checks.";
    ret["detail_list"] = "A special list with critical, then warning and finally ok. Common option for all checks.";
    ret["status"] = "The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.";
    ret["sep"] =
        "The decoded list-separator, for use in the top-syntax: templates are never escape-decoded (a literal C:\\temp must stay a literal C:\\temp), so "
        "reference %(sep) to break the line before the first list item, e.g. top-syntax=%(status): %(count) items:%(sep)%(list). Common option for all "
        "checks.";
    return ret;
  }

  bool has_variable(const std::string& name) {
    return name == "count" || name == "total" || name == "ok_count" || name == "warn_count" || name == "crit_count" || name == "problem_count" ||
           name == "list" || name == "ok_list" || name == "warn_list" || name == "crit_list" || name == "problem_list" || name == "detail_list" ||
           name == "lines" || name == "status" || name == "sep";
  }

  node_type create_variable(const std::string& name, bool human_readable = false) const;
};

template <class TObject>
struct filter_handler_impl : evaluation_context_impl<TObject> {
  typedef TObject object_type;
  typedef boost::function<std::string(object_type, evaluation_context)> bound_string_type;
  typedef boost::function<long long(object_type, evaluation_context)> bound_int_type;
  typedef boost::function<double(object_type, evaluation_context)> bound_float_type;
  typedef boost::function<node_type(value_type, evaluation_context, node_type)> bound_function_type;
  typedef function_registry<object_type> registry_type;

  registry_type registry_;

  // When set, summary keywords (status, list, count, ...) take precedence over
  // same-named record variables in create_variable. Enabled while parsing the
  // summary-level templates (top/ok/empty syntax): those render with no record
  // attached, so a record variable named e.g. "status" (battery charge status,
  // network link state) would otherwise shadow the overall %(status) and
  // always render as an empty string.
  bool prefer_summary_ = false;

  void set_prefer_summary(const bool prefer_summary) { prefer_summary_ = prefer_summary; }

  std::map<std::string, std::string> get_filter_syntax() const {
    std::map<std::string, std::string> ret;
    for (const typename registry_type::variable_type::value_type& var : registry_.variables) {
      ret[var.first] = var.second->description;
    }
    for (const typename registry_type::function_type::value_type& var : registry_.functions) {
      ret[var.first + "()"] = var.second->description;
    }
    return ret;
  }

  bool can_convert(std::string name, node_type subject, value_type to) override { return registry_.has_converter(to); }

  std::shared_ptr<binary_function_impl> create_converter(std::string name, node_type subject, value_type to) override { return registry_.get_converter(to); }

  bool has_variable(const std::string& name) override {
    return registry_.has_variable(name) || evaluation_context_impl<TObject>::get_summary()->has_variable(name);
  }
  node_type create_variable(const std::string& name, bool human_readable) override {
    if (prefer_summary_ && evaluation_context_impl<TObject>::get_summary()->has_variable(name)) {
      return evaluation_context_impl<TObject>::get_summary()->create_variable(name);
    }
    if (registry_.has_variable(name)) {
      std::shared_ptr<filter_variable<object_type>> var = registry_.get_variable(name, human_readable);
      if (var) {
        // Optional number: takes precedence over the plain accessors — the
        // node handles numeric + string + rendering + perf-suppression
        // itself (see optional_int_variable_node).
        if (var->o_function) {
          if (var->int_perf.empty() && var->add_default_perf) {
            typename filter_variable<object_type>::int_perf_generator_type gen(
                new simple_number_performance_generator<object_type, long long>("", "", "_" + var->name));
            var->int_perf.push_back(gen);
          }
          return node_type(new optional_int_variable_node<filter_handler_impl>(name, var->type, var->o_function, var->no_value, var->int_perf));
        }
        // A dynamically-typed value (int + float + string accessors all set,
        // e.g. check_http's --json-path aliases): route each comparison to the
        // matching accessor so numeric thresholds keep float precision while
        // string comparisons still work.
        if (var->i_function && var->f_function && var->s_function) {
          if (var->int_perf.empty() && var->add_default_perf) {
            typename filter_variable<object_type>::int_perf_generator_type gen(
                new simple_number_performance_generator<object_type, long long>("", "", "_" + var->name));
            var->int_perf.push_back(gen);
          }
          return node_type(
              new dual_variable_node<filter_handler_impl>(name, var->type, var->i_function, var->f_function, var->s_function, var->int_perf));
        }
        if (var->f_function) {
          if (var->float_perf.empty() && var->add_default_perf) {
            typename filter_variable<object_type>::float_perf_generator_type gen(
                new simple_number_performance_generator<object_type, double>("", "", "_" + var->name));
            var->float_perf.push_back(gen);
          }
          return node_type(new float_variable_node<filter_handler_impl>(name, var->type, var->f_function, var->float_perf));
        }
        if (var->i_function) {
          if (var->int_perf.empty() && var->add_default_perf) {
            typename filter_variable<object_type>::int_perf_generator_type gen(
                new simple_number_performance_generator<object_type, long long>("", "", "_" + var->name));
            var->int_perf.push_back(gen);
          }
          if (var->s_function) return node_type(new dual_variable_node<filter_handler_impl>(name, var->type, var->i_function, var->s_function, var->int_perf));
          if (var->f_function) return node_type(new dual_variable_node<filter_handler_impl>(name, var->type, var->i_function, var->f_function, var->int_perf));
          return node_type(new int_variable_node<filter_handler_impl>(name, var->type, var->i_function, var->int_perf));
        }
        if (var->s_function) return node_type(new str_variable_node<filter_handler_impl>(name, var->type, var->s_function));
      }
    } else if (evaluation_context_impl<TObject>::get_summary()->has_variable(name)) {
      return evaluation_context_impl<TObject>::get_summary()->create_variable(name);
    }
    this->error("Failed to find variable: " + name);
    return factory::create_false();
  }

  bool has_function(const std::string& name) override { return registry_.has_function(name); }
  virtual node_type create_text_function(const std::string& name) { return create_function(name, factory::create_list()); }

  virtual variable_list_type get_variables() { return registry_.get_variables(); }

  node_type create_function(const std::string& name, node_type subject) override {
    if (!registry_.has_function(name)) return factory::create_false();
    std::shared_ptr<filter_function> var = registry_.get_function(name);
    // type_is_float covers the integer types too, so this accepts every
    // numeric return type. Float-returning functions used to fall through to
    // create_false() here, which silently disabled them wherever they were
    // registered - `convert_bytes(value, 'MB') > 100` and `scale(...)` never
    // evaluated (#1392; they were added for #281).
    if (var && var->function && (helpers::type_is_float(var->type) || helpers::type_is_string(var->type))) {
      return std::make_shared<custom_function_node>(name, var->function, subject, var->type);
    }
    return factory::create_false();
  }
  bool can_convert(value_type, value_type to) override {
    if (registry_.has_converter(to)) return true;
    return false;
  }

  typedef std::map<std::string, std::string> perf_object_options_type;
  typedef boost::unordered_map<std::string, perf_object_options_type> perf_options_type;
  perf_options_type perf_options;

  virtual bool has_performance_config_for_object(const std::string obj) const { return perf_options.find(obj) != perf_options.end(); }
  std::string get_performance_config_key(const std::string prefix, const std::string obj, const std::string suffix, const std::string key,
                                         const std::string v) const override {
    std::string value = v;
    bool has_p = !prefix.empty();
    bool has_s = !suffix.empty();
    if (has_p && has_s && get_performance_config_value(prefix + "." + obj + "." + suffix, key, value)) return value;
    if (has_p && get_performance_config_value(prefix + "." + obj, key, value)) return value;
    if (has_s && get_performance_config_value(obj + "." + suffix, key, value)) return value;
    if (has_p && get_performance_config_value(prefix, key, value)) return value;
    if (has_s && get_performance_config_value(suffix, key, value)) return value;
    if (get_performance_config_value(obj, key, value)) return value;
    if (get_performance_config_value("*", key, value)) return value;
    return value;
  }
  virtual bool get_performance_config_value(const std::string obj, const std::string key, std::string& value) const {
    perf_options_type::const_iterator cit = perf_options.find(obj);
    if (cit == perf_options.end()) return false;
    perf_object_options_type::const_iterator cit2 = cit->second.find(key);
    if (cit2 == cit->second.end()) return false;
    value = cit2->second;
    return true;
  }
  virtual void add_perf_config(const std::string& key, const std::map<std::string, std::string>& options) { perf_options[key] = options; }
};

template <class TObject>
node_type generic_summary<TObject>::create_variable(const std::string& key, bool) const {
  if (key == "count")
    return node_type(new summary_int_variable_node<evaluation_context_impl<TObject>>(key, [](auto value) { return value->get_count_match(); }));
  if (key == "total")
    return node_type(new summary_int_variable_node<evaluation_context_impl<TObject>>(key, [](auto value) { return value->get_count_total(); }));
  if (key == "ok_count")
    return node_type(new summary_int_variable_node<evaluation_context_impl<TObject>>(key, [](auto value) { return value->get_count_ok(); }));
  if (key == "warn_count")
    return node_type(new summary_int_variable_node<evaluation_context_impl<TObject>>(key, [](auto value) { return value->get_count_warn(); }));
  if (key == "crit_count")
    return node_type(new summary_int_variable_node<evaluation_context_impl<TObject>>(key, [](auto value) { return value->get_count_crit(); }));
  if (key == "problem_count")
    return node_type(new summary_int_variable_node<evaluation_context_impl<TObject>>(key, [](auto value) { return value->get_count_problem(); }));
  if (key == "list" || key == "match_list" || key == "lines")
    return node_type(new summary_string_variable_node<evaluation_context_impl<TObject>>(key, [](auto value) { return value->get_list_match(); }));
  if (key == "ok_list")
    return node_type(new summary_string_variable_node<evaluation_context_impl<TObject>>(key, [](auto value) { return value->get_list_ok(); }));
  if (key == "warn_list")
    return node_type(new summary_string_variable_node<evaluation_context_impl<TObject>>(key, [](auto value) { return value->get_list_warn(); }));
  if (key == "crit_list")
    return node_type(new summary_string_variable_node<evaluation_context_impl<TObject>>(key, [](auto value) { return value->get_list_crit(); }));
  if (key == "problem_list")
    return node_type(new summary_string_variable_node<evaluation_context_impl<TObject>>(key, [](auto value) { return value->get_list_problem(); }));
  if (key == "detail_list")
    return node_type(new summary_string_variable_node<evaluation_context_impl<TObject>>(key, [](auto value) { return value->get_list_detail(); }));
  if (key == "status")
    return node_type(new summary_string_variable_node<evaluation_context_impl<TObject>>(key, [](auto value) { return value->get_status(); }));
  if (key == "sep")
    return node_type(new summary_string_variable_node<evaluation_context_impl<TObject>>(key, [](auto value) { return value->get_list_separator(); }));
  return factory::create_false();
}

template <class T>
node_type filter_converter<T>::evaluate(value_type, evaluation_context context, const node_type subject) const {
  try {
    typedef filter_handler_impl<T>* native_context_type;
    native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
    if (!native_context->has_object()) {
      context->error("No object attached");
      return factory::create_false();
    }
    if (!function) {
      context->error("No function attached");
      return factory::create_false();
    }
    T obj = native_context->get_object();
    return function(obj, context, subject);
  } catch (const std::exception& e) {
    context->error("Failed to evaluate function: " + utf8::utf8_from_native(e.what()));
    return factory::create_false();
  }
}
}  // namespace where
}  // namespace parsers