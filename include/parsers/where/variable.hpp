/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <algorithm>
#include <boost/algorithm/string/trim.hpp>
#include <boost/function.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <parsers/where/helpers.hpp>
#include <parsers/where/node.hpp>
#include <str/format.hpp>
#include <str/utf8.hpp>

namespace parsers {
namespace where {
template <class TObject, typename TDataType>
struct number_performance_generator_interface {
  virtual ~number_performance_generator_interface() = default;
  virtual bool is_configured() = 0;
  virtual void configure(std::string key, object_factory context) = 0;
  virtual void eval(perf_list_type &list, evaluation_context context, std::string alias, TDataType current_value, TDataType warn, TDataType crit,
                    TObject object) = 0;
};

// Parse a perf-config string (e.g. "0", "12345", "3.14") as a double. Returns
// an empty optional for empty input or anything that doesn't lex as a number,
// which is the signal to "leave the bound unset" for callers.
inline boost::optional<double> parse_optional_perf_bound(const std::string &s) {
  if (s.empty()) return boost::none;
  try {
    return boost::lexical_cast<double>(boost::trim_copy(s));
  } catch (const boost::bad_lexical_cast &) {
    return boost::none;
  }
}

template <class TContext, typename TDataType>
struct simple_number_performance_generator : number_performance_generator_interface<TContext, TDataType> {
  std::string unit;
  std::string prefix;
  std::string suffix;
  bool configured;
  bool ignored;
  // Optional explicit bounds, populated from perf-config keys
  // `minimum`/`maximum` (with `min`/`max` accepted as aliases). When unset, the
  // emitted Nagios perfdata leaves the corresponding field empty.
  boost::optional<double> minimum;
  boost::optional<double> maximum;
  simple_number_performance_generator(const std::string &unit, const std::string &prefix, const std::string &suffix)
      : unit(unit), prefix(prefix), suffix(suffix), configured(false), ignored(false) {}
  explicit simple_number_performance_generator(const std::string &unit) : unit(unit), configured(false), ignored(false) {}
  bool is_configured() override { return configured; }
  void configure(const std::string key, const object_factory context) override {
    const std::string p = boost::trim_copy(prefix);
    const std::string k = boost::trim_copy(key);
    const std::string s = boost::trim_copy(suffix);
    unit = context->get_performance_config_key(p, k, s, "unit", unit);
    prefix = context->get_performance_config_key(p, k, s, "prefix", prefix);
    suffix = context->get_performance_config_key(p, k, s, "suffix", suffix);
    if (prefix == "none") prefix = "";
    if (suffix == "none") suffix = "";
    if (context->get_performance_config_key(p, k, s, "ignored", "false") == "true") ignored = true;
    // Min / max overrides. `minimum`/`maximum` are the canonical names;
    // `min`/`max` are accepted as shorter aliases for ergonomics. The longer
    // names win if both are set so users can rely on the spelled-out form.
    minimum = parse_optional_perf_bound(context->get_performance_config_key(p, k, s, "minimum", ""));
    if (!minimum) minimum = parse_optional_perf_bound(context->get_performance_config_key(p, k, s, "min", ""));
    maximum = parse_optional_perf_bound(context->get_performance_config_key(p, k, s, "maximum", ""));
    if (!maximum) maximum = parse_optional_perf_bound(context->get_performance_config_key(p, k, s, "max", ""));
    configured = true;
  }
  void eval(perf_list_type &list, evaluation_context context, const std::string alias, TDataType current_value, TDataType warn, TDataType crit,
            TContext object) override {
    if (ignored) return;
    performance_data data;
    performance_data::perf_value int_data;
    int_data.value = static_cast<double>(current_value);
    int_data.warn = static_cast<double>(warn);
    int_data.crit = static_cast<double>(crit);
    if (minimum) int_data.minimum = *minimum;
    if (maximum) int_data.maximum = *maximum;
    data.set(int_data);
    data.alias = prefix + alias + suffix;
    data.unit = unit;
    list.push_back(data);
  }
};

template <class TContext>
struct percentage_int_performance_generator : number_performance_generator_interface<TContext, long long> {
  typedef boost::function<long long(TContext, evaluation_context)> maxfun_type;
  maxfun_type maxfun;
  std::string prefix;
  std::string suffix;
  bool configured;
  bool ignored;
  percentage_int_performance_generator(maxfun_type maxfun, std::string prefix, std::string suffix)
      : maxfun(maxfun), prefix(prefix), suffix(suffix), configured(false), ignored(false) {}
  bool is_configured() override { return configured; }
  void configure(const std::string key, const object_factory context) override {
    const std::string p = boost::trim_copy(prefix);
    const std::string k = boost::trim_copy(key);
    const std::string s = boost::trim_copy(suffix);
    prefix = context->get_performance_config_key(p, k, s, "prefix", prefix);
    suffix = context->get_performance_config_key(p, k, s, "suffix", suffix);
    if (prefix == "none") prefix = "";
    if (suffix == "none") suffix = "";
    if (context->get_performance_config_key(p, k, s, "ignored", "false") == "true") ignored = true;
    configured = true;
  }
  double round(double number) { return number < 0.0 ? ceil(number - 0.5) : floor(number + 0.5); }
  void eval(perf_list_type &list, evaluation_context context, std::string alias, long long current_value, long long warn, long long crit,
            TContext object) override {
    if (ignored) return;
    long long maximum = maxfun(object, context);
    performance_data data;
    performance_data::perf_value double_data;
    if (maximum > 0) {
      double_data.value = round(static_cast<double>(current_value * 100) / maximum);
      double_data.warn = round(static_cast<double>(warn * 100) / maximum);
      double_data.crit = round(static_cast<double>(crit * 100) / maximum);
      double_data.maximum = 100;
      double_data.minimum = 0;
      data.set(double_data);
      data.alias = prefix + alias + suffix;
      data.unit = "%";
      list.push_back(data);
    }
  }
};

template <class TContext>
struct scaled_byte_int_performance_generator : number_performance_generator_interface<TContext, long long> {
  typedef boost::function<long long(TContext, evaluation_context)> maxfun_type;
  maxfun_type minfun;
  maxfun_type maxfun;
  std::string prefix;
  std::string suffix;
  std::string unit;
  bool configured;
  bool ignored;
  scaled_byte_int_performance_generator(maxfun_type minfun, maxfun_type maxfun, const std::string &prefix, const std::string &suffix)
      : minfun(minfun), maxfun(maxfun), prefix(prefix), suffix(suffix), configured(false), ignored(false) {}
  scaled_byte_int_performance_generator(maxfun_type maxfun, const std::string &prefix, const std::string &suffix)
      : maxfun(maxfun), prefix(prefix), suffix(suffix), configured(false), ignored(false) {}
  scaled_byte_int_performance_generator(const std::string &prefix, const std::string &suffix)
      : prefix(prefix), suffix(suffix), configured(false), ignored(false) {}
  bool is_configured() override { return configured; }
  void configure(const std::string key, object_factory context) override {
    const std::string p = boost::trim_copy(prefix);
    const std::string k = boost::trim_copy(key);
    const std::string s = boost::trim_copy(suffix);
    unit = context->get_performance_config_key(p, k, s, "unit", unit);
    prefix = context->get_performance_config_key(p, k, s, "prefix", prefix);
    suffix = context->get_performance_config_key(p, k, s, "suffix", suffix);
    if (prefix == "none") prefix = "";
    if (suffix == "none") suffix = "";
    if (context->get_performance_config_key(p, k, s, "ignored", "false") == "true") ignored = true;
    configured = true;
  }
  void eval(perf_list_type &list, evaluation_context context, const std::string alias, const long long current_value, const long long warn,
            const long long crit, TContext object) override {
    if (ignored) return;
    std::string active_unit = unit;
    long long max_value = 0;
    long long min_value = 0;
    if (maxfun) max_value = maxfun(object, context);
    if (minfun) min_value = minfun(object, context);
    if (active_unit.empty()) {
      long long m = current_value;
      if (warn > 0) m = (std::max)(m, warn);
      if (crit > 0) m = (std::max)(m, crit);
      if (max_value > 0) m = (std::min)(m, max_value);
      if (min_value > 0) m = (std::min)(m, min_value);
      active_unit = str::format::find_proper_unit_BKMG(m);
    }

    performance_data::perf_value double_data;
    if (maxfun) {
      if (max_value > 0)
        double_data.maximum = str::format::convert_to_byte_units(max_value, active_unit);
      else
        double_data.maximum = max_value;
    }
    if (minfun) {
      if (min_value > 0)
        double_data.minimum = str::format::convert_to_byte_units(min_value, active_unit);
      else
        double_data.minimum = min_value;
    }
    double_data.warn = str::format::convert_to_byte_units(warn, active_unit);
    double_data.crit = str::format::convert_to_byte_units(crit, active_unit);
    double_data.value = str::format::convert_to_byte_units(current_value, active_unit);
    performance_data data;
    data.set(double_data);
    data.alias = prefix + alias + suffix;
    data.unit = active_unit;
    list.push_back(data);
  }
};

template <class TContext>
struct int_variable_node : any_node {
  std::string name_;
  typedef TContext *native_context_type;
  typedef typename TContext::bound_int_type function_type;
  typedef typename TContext::object_type object_type;
  typedef std::shared_ptr<number_performance_generator_interface<object_type, long long> > int_performance_generator;

  function_type fun;
  std::list<int_performance_generator> perfgen;

  int_variable_node(const std::string &name, value_type type, function_type fun, std::list<int_performance_generator> perfgen)
      : any_node(type), name_(name), fun(fun), perfgen(perfgen) {}
  // TODO: add c-tors

  std::list<node_type> get_list_value(evaluation_context context) const override { return std::list<node_type>(); }
  bool can_evaluate() const override { return true; }
  std::shared_ptr<any_node> evaluate(evaluation_context context) const override {
    try {
      native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
      if (native_context != nullptr && fun && native_context->has_object()) {
        return factory::create_int(fun(native_context->get_object(), context));
      }
      context->error("Failed to evaluate " + name_ + " no object instance");
    } catch (const std::exception &e) {
      context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
    }
    return factory::create_false();
  }
  bool bind(object_converter context) override { return true; }
  bool static_evaluate(evaluation_context context) const override { return false; }
  bool require_object(evaluation_context context) const override { return true; }
  value_container get_value(evaluation_context context, const value_type vt) const override {
    const bool ti = helpers::type_is_int(vt);
    const bool tf = helpers::type_is_float(vt);
    if (ti || tf) {
      try {
        native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
        if (native_context != nullptr && fun && native_context->has_object()) {
          const long long v = fun(native_context->get_object(), context);
          if (ti) return value_container::create_int(v);
          return value_container::create_float(static_cast<double>(v));
        }
        context->warn("Failed to get " + name_ + " no object instance");
        if (ti) return value_container::create_int(0, true);
        return value_container::create_float(0, true);
      } catch (const std::exception &e) {
        context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
        return value_container::create_nil();
      }
    }
    context->error("Invalid type " + name_ + " we are int but wanted: " + str::xtos(vt));
    return value_container::create_nil();
  }
  std::string to_string(evaluation_context context) const override {
    native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
    if (native_context != nullptr && fun && native_context->has_object()) {
      return str::xtos(fun(native_context->get_object(), context));
    }
    return name_ + "?";
  }
  std::string to_string() const override { return "{int}" + name_; }
  value_type infer_type(object_converter converter, const value_type vt) override {
    if (helpers::type_is_int(vt)) return get_type();
    if (helpers::type_is_float(vt)) set_type(vt);
    return get_type();
  }
  value_type infer_type(object_converter converter) override { return get_type(); }
  bool find_performance_data(evaluation_context context, performance_collector &collector) override {
    collector.set_candidate_variable(name_);
    return false;
  }

  perf_list_type get_performance_data(object_factory context, std::string alias, const node_type warn, const node_type crit, node_type minimum,
                                      node_type maximum) override {
    perf_list_type ret;
    native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
    if (native_context != nullptr && native_context->has_object()) {
      long long warn_value = 0;
      long long crit_value = 0;
      long long current_value = get_int_value(context);
      if (warn) warn_value = warn->get_int_value(context);
      if (crit) crit_value = crit->get_int_value(context);
      for (int_performance_generator &p : perfgen) {
        if (!p->is_configured()) p->configure(name_, context);
        p->eval(ret, context, alias, current_value, warn_value, crit_value, native_context->get_object());
      }
    }
    return ret;
  }
};
template <class TContext>
struct float_variable_node : any_node {
  std::string name_;
  typedef TContext *native_context_type;
  typedef typename TContext::bound_float_type function_type;
  typedef typename TContext::object_type object_type;
  typedef std::shared_ptr<number_performance_generator_interface<object_type, double> > float_performance_generator;

  function_type fun;
  std::list<float_performance_generator> perfgen;

  float_variable_node(const std::string &name, value_type type, function_type fun, std::list<float_performance_generator> perfgen)
      : any_node(type), name_(name), fun(fun), perfgen(perfgen) {}
  // TODO: add c-tors

  std::list<node_type> get_list_value(evaluation_context context) const override { return std::list<node_type>(); }
  bool can_evaluate() const override { return true; }
  std::shared_ptr<any_node> evaluate(evaluation_context context) const override {
    try {
      native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
      if (native_context != nullptr && fun && native_context->has_object()) return factory::create_float(fun(native_context->get_object(), context));
      context->error("Failed to evaluate " + name_ + " no object instance");
    } catch (const std::exception &e) {
      context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
    }
    return factory::create_false();
  }
  bool bind(object_converter context) override { return true; }
  bool static_evaluate(evaluation_context context) const override { return false; }
  bool require_object(evaluation_context context) const override { return true; }
  value_container get_value(evaluation_context context, value_type vt) const override {
    const bool ti = helpers::type_is_int(vt);
    const bool tf = helpers::type_is_float(vt);
    if (ti || tf) {
      try {
        native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
        if (native_context != nullptr && fun && native_context->has_object()) {
          const double v = fun(native_context->get_object(), context);
          if (ti) return value_container::create_int(static_cast<long long>(v));
          return value_container::create_float(v);
        }
        context->warn("Failed to get " + name_ + " no object instance");
        if (ti) return value_container::create_int(0, true);
        return value_container::create_float(0, true);
      } catch (const std::exception &e) {
        context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
        return value_container::create_nil();
      }
    }
    context->error("Invalid type " + name_ + " we are float but wanted: " + str::xtos(vt));
    return value_container::create_nil();
  }
  std::string to_string(evaluation_context context) const override {
    native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
    if (native_context != nullptr && fun && native_context->has_object()) {
      return str::xtos(fun(native_context->get_object(), context));
    }
    return "(float)var:" + name_;
  }
  std::string to_string() const override { return "{float}" + name_; }
  value_type infer_type(object_converter converter, const value_type vt) override {
    if (helpers::type_is_int(vt)) set_type(vt);
    return get_type();
  }
  value_type infer_type(object_converter converter) override { return get_type(); }
  bool find_performance_data(evaluation_context context, performance_collector &collector) override {
    collector.set_candidate_variable(name_);
    return false;
  }

  perf_list_type get_performance_data(object_factory context, std::string alias, const node_type warn, const node_type crit, node_type minimum,
                                      node_type maximum) override {
    perf_list_type ret;
    native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
    if (native_context != nullptr && native_context->has_object()) {
      double warn_value = 0;
      double crit_value = 0;
      double current_value = get_float_value(context);
      if (warn) warn_value = warn->get_float_value(context);
      if (crit) crit_value = crit->get_float_value(context);
      for (float_performance_generator &p : perfgen) {
        if (!p->is_configured()) p->configure(name_, context);
        p->eval(ret, context, alias, current_value, warn_value, crit_value, native_context->get_object());
      }
    }
    return ret;
  }
};

template <class TContext>
struct str_variable_node : any_node {
  std::string name_;
  typedef TContext *native_context_type;
  typedef typename TContext::bound_string_type function_type;

  function_type fun;

  str_variable_node(const std::string &name, const value_type type, function_type fun) : any_node(type), name_(name), fun(fun) {}
  // TODO: add c-tors

  std::list<node_type> get_list_value(evaluation_context context) const override { return std::list<node_type>(); }
  bool can_evaluate() const override { return true; }
  std::shared_ptr<any_node> evaluate(evaluation_context context) const override {
    try {
      native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
      if (native_context != nullptr && fun && native_context->has_object()) return factory::create_string(fun(native_context->get_object(), context));
      // Demote to warn — the no-object case is expected during the
      // modern_filter no-rows force-evaluate path. error logged here used to
      // produce one ERROR per object-bound string variable per empty-rows
      // tick in production agent logs.
      context->warn("Failed to evaluate " + name_ + " no object instance");
    } catch (const std::exception &e) {
      context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
    }
    return factory::create_false();
  }
  bool bind(object_converter context) override { return true; }
  bool static_evaluate(evaluation_context context) const override { return false; }
  bool require_object(evaluation_context context) const override { return true; }
  long long get_int_value(const evaluation_context context) const override {
    context->error("Invalid type: " + name_);
    return 0;
  }
  double get_float_value(const evaluation_context context) const override {
    context->error("Invalid type: " + name_);
    return 0;
  }
  value_container get_value(evaluation_context context, const value_type vt) const override {
    if (vt == type_string) {
      try {
        native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
        // Match int_variable_node / float_variable_node's no-object contract:
        // return a typed-but-unsure default value (here, empty string with
        // is_unsure=true) plus a warn (not error). This propagates the
        // unsure flag through every downstream operator naturally — every
        // comparison/LIKE/REGEXP/IN/NOT-IN sees a real string and produces
        // a properly-flagged result. Centralises what used to be a per-
        // operator nil-guard, and stops emitting one ERROR per empty-rows
        // tick into production logs.
        if (native_context != nullptr && fun && native_context->has_object()) {
          return value_container::create_string(fun(native_context->get_object(), context));
        }
        context->warn("Unbound function " + name_);
        return value_container::create_string("", /*is_unsure=*/true);
      } catch (const std::exception &e) {
        context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
        return value_container::create_nil();
      }
    }
    context->error("Invalid type " + name_);
    return value_container::create_nil();
  }
  std::string to_string(evaluation_context context) const override {
    native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
    if (native_context != nullptr && native_context->has_object()) {
      return fun(native_context->get_object(), context);
    }
    return "(string)var:" + name_;
  }
  std::string to_string() const override { return "{string}" + name_; }
  value_type infer_type(object_converter, value_type) override { return get_type(); }
  value_type infer_type(object_converter) override { return get_type(); }
  bool find_performance_data(evaluation_context context, performance_collector &collector) override {
    collector.set_candidate_variable(name_);
    return false;
  }
};

template <class TContext>
struct dual_variable_node : any_node {
  std::string name_;
  typedef TContext *native_context_type;
  typedef typename TContext::bound_string_type s_function_type;
  typedef typename TContext::bound_int_type i_function_type;
  typedef typename TContext::bound_float_type f_function_type;
  typedef typename TContext::object_type object_type;
  typedef std::shared_ptr<number_performance_generator_interface<object_type, long long> > int_performance_generator;
  value_type fallback_type;

  i_function_type i_fun;
  f_function_type f_fun;
  s_function_type s_fun;
  std::list<int_performance_generator> perfgen;

  dual_variable_node(const std::string &name, value_type fallback_type, i_function_type i_fun, s_function_type s_fun,
                     std::list<int_performance_generator> perfgen)
      : any_node(type_multi), name_(name), fallback_type(fallback_type), i_fun(i_fun), s_fun(s_fun), perfgen(perfgen) {}
  dual_variable_node(const std::string &name, value_type fallback_type, i_function_type i_fun, f_function_type f_fun,
                     std::list<int_performance_generator> perfgen)
      : any_node(type_multi), name_(name), fallback_type(fallback_type), i_fun(i_fun), f_fun(f_fun), perfgen(perfgen) {}
  // TODO: add c-tors

  std::list<node_type> get_list_value(evaluation_context context) const override { return std::list<node_type>(); }
  bool can_evaluate() const override { return true; }
  std::shared_ptr<any_node> evaluate(evaluation_context context) const override {
    // TODO!!!
    if (is_string()) {
      try {
        native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
        if (native_context != nullptr && s_fun && native_context->has_object()) return factory::create_string(s_fun(native_context->get_object(), context));
        context->error("Failed to evaluate " + name_ + " no object instance");
      } catch (const std::exception &e) {
        context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
      }
      return factory::create_false();
    }
    if (is_float()) {
      try {
        native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
        if (native_context != nullptr && f_fun && native_context->has_object()) return factory::create_float(f_fun(native_context->get_object(), context));
        context->error("Failed to evaluate " + name_ + " no object instance");
      } catch (const std::exception &e) {
        context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
      }
      return factory::create_false();
    }
    try {
      native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
      if (native_context != nullptr && i_fun && native_context->has_object()) return factory::create_int(i_fun(native_context->get_object(), context));
      context->error("Failed to evaluate " + name_ + " no object instance");
    } catch (const std::exception &e) {
      context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
    }
    return factory::create_false();
  }
  bool bind(object_converter context) override { return true; }
  bool static_evaluate(evaluation_context context) const override { return false; }
  bool require_object(evaluation_context context) const override { return true; }
  value_container get_value(evaluation_context context, value_type vt) const override {
    try {
      native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
      if (native_context != nullptr && native_context->has_object()) {
        if (helpers::type_is_int(vt) && i_fun) return value_container::create_int(i_fun(native_context->get_object(), context));
        if (helpers::type_is_float(vt) && f_fun) return value_container::create_float(f_fun(native_context->get_object(), context));
        if (vt == type_string && s_fun) return value_container::create_string(s_fun(native_context->get_object(), context));
        if (vt == type_string && i_fun && (is_int() || !f_fun)) return value_container::create_string(str::xtos(i_fun(native_context->get_object(), context)));
        if (vt == type_string && f_fun) return value_container::create_string(str::xtos(f_fun(native_context->get_object(), context)));
      } else {
        context->warn("Failed to get " + name_ + " no object instance");
        if (helpers::type_is_int(vt)) return value_container::create_int(0, true);
        if (helpers::type_is_float(vt)) return value_container::create_float(0, true);
        if (vt == type_string) return value_container::create_string("", true);
      }
      context->error("No context when evaluating: " + name_);
      return value_container::create_nil();
    } catch (const std::exception &e) {
      context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
    }
    context->error("Failed to evaluate " + name_);
    return value_container::create_nil();
  }
  std::string to_string(evaluation_context context) const override {
    native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
    if (native_context != nullptr && native_context->has_object()) {
      if (s_fun) return s_fun(native_context->get_object(), context);
      if (i_fun) return str::xtos(i_fun(native_context->get_object(), context));
      if (f_fun) return str::xtos(f_fun(native_context->get_object(), context));
    }
    if (is_int()) return name_ + "?";
    if (is_string()) return name_ + "?";
    if (is_float()) return name_ + "?";
    return name_ + "?";
  }

  std::string to_string() const override {
    if (is_int()) return "{int}" + name_;
    if (is_string()) return "{string}" + name_;
    return "{" + helpers::type_to_string(get_type()) + "}" + name_;
  }
  value_type infer_type(object_converter converter, value_type suggestion) override {
    if (helpers::type_is_int(suggestion)) {
      set_type(type_int);
    } else if (helpers::type_is_float(suggestion)) {
      set_type(type_float);
    } else if (suggestion == type_string) {
      set_type(type_string);
    } else if (suggestion == type_tbd) {
      set_type(fallback_type);
    }

    return get_type();
  }
  value_type infer_type(object_converter converter) override { return get_type(); }
  bool find_performance_data(evaluation_context context, performance_collector &collector) override {
    if (get_type() != type_string) collector.set_candidate_variable(name_);
    return false;
  }

  perf_list_type get_performance_data(object_factory context, std::string alias, node_type warn, node_type crit, node_type minimum,
                                      node_type maximum) override {
    perf_list_type ret;
    native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
    if (native_context != nullptr && native_context->has_object()) {
      long long warn_value = 0;
      long long crit_value = 0;
      long long current_value = get_int_value(context);
      if (warn) warn_value = warn->get_int_value(context);
      if (crit) crit_value = crit->get_int_value(context);
      for (const int_performance_generator &p : perfgen) {
        if (!p->is_configured()) p->configure(name_, context);
        p->eval(ret, context, alias, current_value, warn_value, crit_value, native_context->get_object());
      }
    }
    return ret;
  }
};

//////////////////////////////////////////////////////////////////////////

struct custom_function_node : any_node {
  typedef boost::function<node_type(value_type, evaluation_context, node_type)> bound_function_type;

  std::string name_;
  bound_function_type fun;
  node_type subject;

  custom_function_node(const std::string &name, bound_function_type fun, const node_type &subject, value_type type)
      : any_node(type), name_(name), fun(fun), subject(subject) {}

  std::list<node_type> get_list_value(evaluation_context context) const override { return std::list<node_type>(); }
  bool can_evaluate() const override { return false; }
  std::shared_ptr<any_node> evaluate(const evaluation_context context) const override {
    try {
      if (fun) return fun(get_type(), context, subject);
      context->error("Failed to evaluate " + name_ + " no function");
    } catch (const std::exception &e) {
      context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
    }
    return factory::create_false();
  }
  bool bind(object_converter context) override { return true; }
  bool static_evaluate(evaluation_context context) const override { return false; }
  bool require_object(evaluation_context context) const override { return true; }
  value_container get_value(const evaluation_context context, value_type vt) const override { return evaluate(context)->get_value(context, vt); }
  std::string to_string(const evaluation_context context) const override {
    if (fun) return fun(type_string, context, subject)->get_string_value(context);
    return "(string)fun:" + name_;
  }
  std::string to_string() const override { return "{string}" + name_ + "()"; }
  value_type infer_type(object_converter converter, value_type) override { return type_string; }
  value_type infer_type(object_converter converter) override { return type_string; }
  bool find_performance_data(evaluation_context context, performance_collector &) override {
    // collector.set_candidate_variable(name_);
    return false;
  }
};

template <class TContext>
struct summary_int_variable_node : any_node {
  std::string name_;
  typedef TContext *native_context_type;
  typedef typename TContext::object_type object_type;
  typedef typename TContext::summary_type summary_type;
  typedef std::shared_ptr<number_performance_generator_interface<object_type, long long> > int_performance_generator;
  typedef typename boost::function<long long(summary_type)> function_type;

  function_type fun;

  summary_int_variable_node(const std::string &name, function_type fun) : any_node(type_int), name_(name), fun(fun) {}

  std::list<node_type> get_list_value(evaluation_context context) const override { return std::list<node_type>(); }
  bool can_evaluate() const override { return true; }
  // Pre-dd8024ae the engine evaluated warn/crit/ok per row during iteration,
  // so summary counters such as count_match were genuinely changing under
  // the user's feet. The variable used to flag results as is_unsure when
  // the object was set, plus a "X is most likely mutating" warn, to signal
  // that a per-row warn/crit predicate was reading a non-final summary.
  //
  // Post-dd8024ae the warn/crit/ok engines run in evaluate_deferred_records()
  // *after* the iteration has populated count_match / total / etc. So when
  // an object is set during deferred per-row replay, the summary value the
  // variable returns is final — flagging it unsure or warning about
  // mutation is incorrect (and, at scale, floods production logs with two
  // warns per row per check tick).
  //
  // Drop the heuristic entirely. count_crit / count_warn / count_ok DO
  // mutate during the deferred replay loop, but engine_filter::match()
  // consumes value_container::is_true() and ignores is_unsure, so the
  // user-visible verdict is unchanged for any expression that uses them.
  std::shared_ptr<any_node> evaluate(const evaluation_context context) const override {
    try {
      auto native_context = reinterpret_cast<native_context_type>(context.get());
      if (native_context != nullptr && fun) return factory::create_int(fun(native_context->get_summary()));
      context->error("Failed to evaluate " + name_ + " no function");
    } catch (const std::exception &e) {
      context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
    }
    return factory::create_false();
  }
  bool bind(object_converter context) override { return true; }
  bool static_evaluate(evaluation_context context) const override { return false; }
  bool require_object(evaluation_context context) const override { return false; }
  value_container get_value(const evaluation_context context, value_type wanted_type) const override {
    if (wanted_type == type_int) {
      try {
        auto native_context = reinterpret_cast<native_context_type>(context.get());
        if (native_context != nullptr && fun) {
          return value_container::create_int(fun(native_context->get_summary()), /*is_unsure=*/false);
        }
        context->error("Failed to evaluate " + name_ + " no function");
      } catch (const std::exception &e) {
        context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
      }
      return value_container::create_nil();
    }
    context->error("Unknown type: " + name_);
    return value_container::create_nil();
  }
  std::string to_string(const evaluation_context context) const override {
    try {
      auto native_context = reinterpret_cast<native_context_type>(context.get());
      if (native_context != nullptr && fun) return str::xtos(fun(native_context->get_summary()));
    } catch (...) {
    }
    return name_ + "?";
  }
  std::string to_string() const override { return "{int}" + name_ + "()"; }
  value_type infer_type(object_converter converter, value_type) override { return get_type(); }
  value_type infer_type(object_converter converter) override { return get_type(); }
  bool find_performance_data(evaluation_context context, performance_collector &collector) override {
    collector.set_candidate_variable(name_);
    return false;
  }
  perf_list_type get_performance_data(const object_factory context, std::string alias, const node_type warn, const node_type crit, node_type minimum,
                                      node_type maximum) override {
    perf_list_type ret;
    native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
    if (native_context != nullptr && !native_context->has_object()) {
      long long warn_value = 0;
      long long crit_value = 0;
      const long long current_value = get_int_value(context);
      if (warn) warn_value = warn->get_int_value(context);
      if (crit) crit_value = crit->get_int_value(context);
      performance_data data;
      performance_data::perf_value int_data;
      int_data.value = static_cast<double>(current_value);
      int_data.warn = static_cast<double>(warn_value);
      int_data.crit = static_cast<double>(crit_value);
      data.set(int_data);
      data.alias = name_;
      ret.push_back(data);
    }
    return ret;
  }
};
template <class TContext>
struct summary_string_variable_node : any_node {
  std::string name_;
  typedef TContext *native_context_type;
  typedef typename TContext::object_type object_type;
  typedef typename TContext::summary_type summary_type;
  typedef std::shared_ptr<number_performance_generator_interface<object_type, long long> > int_performance_generator;
  typedef typename boost::function<std::string(summary_type)> function_type;

  function_type fun;

  summary_string_variable_node(const std::string &name, function_type fun) : any_node(type_string), name_(name), fun(fun) {}

  std::list<node_type> get_list_value(evaluation_context context) const override { return std::list<node_type>(); }
  bool can_evaluate() const override { return true; }
  std::shared_ptr<any_node> evaluate(const evaluation_context context) const override {
    try {
      native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
      if (native_context != nullptr && fun) return factory::create_string(fun(native_context->get_summary()));
      context->error("Failed to evaluate " + name_ + " no function");
    } catch (const std::exception &e) {
      context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
    }
    return factory::create_false();
  }
  bool bind(object_converter context) override { return true; }
  bool static_evaluate(evaluation_context context) const override { return false; }
  bool require_object(evaluation_context context) const override { return false; }
  value_container get_value(const evaluation_context context, value_type tpe) const override {
    if (tpe == type_int || tpe == type_float) {
      context->error("Function not numeric: " + name_);
      return value_container::create_nil();
    }
    if (tpe == type_string) {
      native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
      if (native_context != nullptr && fun) return value_container::create_string(fun(native_context->get_summary()));
      context->error("Invalid function: " + name_);
      return value_container::create_nil();
    }
    context->error("Unknown type: " + name_);
    return value_container::create_nil();
  }
  std::string to_string(evaluation_context context) const override {
    native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
    if (native_context != nullptr && fun) return fun(native_context->get_summary());
    return "(str)var:" + name_;
  }
  std::string to_string() const override { return "{str}" + name_ + "()"; }
  value_type infer_type(object_converter converter, value_type) override { return get_type(); }
  value_type infer_type(object_converter converter) override { return get_type(); }
  bool find_performance_data(evaluation_context, performance_collector &) override { return false; }
};
}  // namespace where
}  // namespace parsers