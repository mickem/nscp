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

#include <parsers/where/engine_impl.hpp>
#include <parsers/where/variable.hpp>
#include <parsers/where/helpers.hpp>

#include <str/format.hpp>

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/unordered_map.hpp>
#include <boost/optional.hpp>

#include <map>
#include <vector>

namespace parsers {
	namespace where {
		template<class T>
		struct function_registry;

		template<class T>
		struct filter_variable {
			std::string name;
			value_type type;
			std::string description;
			typedef boost::shared_ptr<parsers::where::number_performance_generator_interface<T, long long> > int_perf_generator_type;
			typedef boost::shared_ptr<parsers::where::number_performance_generator_interface<T, double> > float_perf_generator_type;
			typedef boost::function<std::string(T, evaluation_context)> str_fun_type;
			typedef boost::function<long long(T, evaluation_context)> int_fun_type;
			typedef boost::function<double(T, evaluation_context)> float_fun_type;
			str_fun_type s_function;
			int_fun_type i_function;
			float_fun_type f_function;
			std::list<int_perf_generator_type> int_perf;
			std::list<float_perf_generator_type> float_perf;
			bool add_default_perf;
			void set_no_perf() { add_default_perf = false; }

			filter_variable(std::string name, value_type type, std::string description) : name(name), type(type), description(description), add_default_perf(true) {}
		};
		template<class T>
		struct filter_converter : public parsers::where::binary_function_impl {
			value_type type;
			typedef boost::function<node_type(T, evaluation_context, node_type)> converter_fun_type;
			converter_fun_type function;
			filter_converter(value_type type, converter_fun_type function) : type(type), function(function) {}
			filter_converter(value_type type) : type(type) {}

			virtual node_type evaluate(value_type type, evaluation_context context, const node_type subject) const;
		};

		struct filter_function {
			std::string name;
			std::string description;
			typedef boost::function<node_type(const value_type, evaluation_context, const node_type)> generic_fun_type;
			generic_fun_type function;

			value_type type;
			filter_function(std::string name) : name(name) {}
		};

		template<class T>
		struct registry_adders_variables_int {
			typedef typename filter_variable<T>::int_perf_generator_type perf_generator_type;

			registry_adders_variables_int(function_registry<T>* owner_, bool human = false) : owner(owner_), human(human) {}

			registry_adders_variables_int& operator()(std::string key, typename filter_variable<T>::int_fun_type i_fun, typename filter_variable<T>::str_fun_type s_fun, std::string description) {
				boost::shared_ptr<filter_variable<T> > var(new filter_variable<T>(key, type_int, description));
				var->s_function = s_fun;
				var->i_function = i_fun;
				add_variables(var);
				return *this;
			}
			registry_adders_variables_int& operator()(std::string key, value_type type, typename filter_variable<T>::int_fun_type i_fun, typename filter_variable<T>::str_fun_type s_fun, std::string description) {
				boost::shared_ptr<filter_variable<T> > var(new filter_variable<T>(key, type, description));
				var->i_function = i_fun;
				var->s_function = s_fun;
				add_variables(var);
				return *this;
			}
			registry_adders_variables_int& operator()(std::string key, value_type type, typename filter_variable<T>::int_fun_type i_fun, std::string description) {
				boost::shared_ptr<filter_variable<T> > var(new filter_variable<T>(key, type, description));
				var->i_function = i_fun;
				add_variables(var);
				return *this;
			}
			registry_adders_variables_int& operator()(std::string key, typename filter_variable<T>::int_fun_type i_fun, std::string description) {
				boost::shared_ptr<filter_variable<T> > var(new filter_variable<T>(key, type_int, description));
				var->i_function = i_fun;
				add_variables(var);
				return *this;
			}
			registry_adders_variables_int& add_perf(std::string unit = "", std::string prefix = "", std::string suffix = "") {
				get_last()->int_perf.push_back(perf_generator_type(new parsers::where::simple_number_performance_generator<T, long long>(unit, prefix, suffix)));
				return *this;
			}
			registry_adders_variables_int& no_perf() {
				get_last()->set_no_perf();
				return *this;
			}
			typedef boost::function<long long(T, evaluation_context)> maxfun_type;
			registry_adders_variables_int& add_percentage(maxfun_type maxfun, std::string prefix = "", std::string suffix = "") {
				get_last()->int_perf.push_back(perf_generator_type(new parsers::where::percentage_int_performance_generator<T>(maxfun, prefix, suffix)));
				return *this;
			}

			typedef boost::function<long long(T, evaluation_context)> scale_type;
			registry_adders_variables_int& add_scaled_byte(std::string prefix = "", std::string suffix = "") {
				get_last()->int_perf.push_back(perf_generator_type(new parsers::where::scaled_byte_int_performance_generator<T>(prefix, suffix)));
				return *this;
			}
			registry_adders_variables_int& add_scaled_byte(scale_type minfun, scale_type maxfun, std::string prefix = "", std::string suffix = "") {
				get_last()->int_perf.push_back(perf_generator_type(new parsers::where::scaled_byte_int_performance_generator<T>(minfun, maxfun, prefix, suffix)));
				return *this;
			}
			registry_adders_variables_int& add_scaled_byte(scale_type maxfun, std::string prefix = "", std::string suffix = "") {
				get_last()->int_perf.push_back(perf_generator_type(new parsers::where::scaled_byte_int_performance_generator<T>(maxfun, prefix, suffix)));
				return *this;
			}

		private:
			boost::shared_ptr<filter_variable<T> > get_last();
			void add_variables(boost::shared_ptr<filter_variable<T> > d);
			function_registry<T>* owner;
			bool human;
		};

		template<class T>
		struct registry_adders_variables_float {
			typedef typename filter_variable<T>::float_perf_generator_type perf_generator_type;

			registry_adders_variables_float(function_registry<T>* owner_, bool human = false) : owner(owner_), human(human) {}

			registry_adders_variables_float& operator()(std::string key, value_type type, typename filter_variable<T>::float_fun_type  f_fun, std::string description) {
				boost::shared_ptr<filter_variable<T> > var(new filter_variable<T>(key, type, description));
				var->f_function = f_fun;
				add_variables(var);
				return *this;
			}
			registry_adders_variables_float& operator()(std::string key, typename filter_variable<T>::float_fun_type  f_fun, std::string description) {
				boost::shared_ptr<filter_variable<T> > var(new filter_variable<T>(key, type_float, description));
				var->f_function = f_fun;
				add_variables(var);
				return *this;
			}
			registry_adders_variables_float& add_perf(std::string unit = "", std::string prefix = "", std::string suffix = "") {
				get_last()->float_perf.push_back(perf_generator_type(new parsers::where::simple_number_performance_generator<T, double>(unit, prefix, suffix)));
				return *this;
			}
			registry_adders_variables_float& no_perf() {
				get_last()->set_no_perf();
				return *this;
			}

		private:
			boost::shared_ptr<filter_variable<T> > get_last();
			void add_variables(boost::shared_ptr<filter_variable<T> > d);
			function_registry<T>* owner;
			bool human;
		};

		template<class T>
		struct registry_adders_variables_number {
			typedef typename filter_variable<T>::float_perf_generator_type perf_generator_type;

			registry_adders_variables_number(function_registry<T>* owner_, bool human = false) : owner(owner_), human(human) {}

			registry_adders_variables_number& operator()(std::string key, value_type type, typename filter_variable<T>::int_fun_type i_fun, typename filter_variable<T>::float_fun_type f_fun, std::string description) {
				boost::shared_ptr<filter_variable<T> > var(new filter_variable<T>(key, type, description));
				var->i_function = i_fun;
				var->f_function = f_fun;
				add_variables(var);
				return *this;
			}
			registry_adders_variables_number& operator()(std::string key, typename filter_variable<T>::int_fun_type i_fun, typename filter_variable<T>::float_fun_type f_fun, std::string description) {
				boost::shared_ptr<filter_variable<T> > var(new filter_variable<T>(key, type_float, description));
				var->i_function = i_fun;
				var->f_function = f_fun;
				add_variables(var);
				return *this;
			}
			registry_adders_variables_number& add_perf(std::string unit = "", std::string prefix = "", std::string suffix = "") {
				get_last()->float_perf.push_back(perf_generator_type(new parsers::where::simple_number_performance_generator<T, double>(unit, prefix, suffix)));
				return *this;
			}
			registry_adders_variables_number& no_perf() {
				get_last()->set_no_perf();
				return *this;
			}

		private:
			boost::shared_ptr<filter_variable<T> > get_last();
			void add_variables(boost::shared_ptr<filter_variable<T> > d);
			function_registry<T>* owner;
			bool human;
		};

		template<class T>
		struct registry_adders_variables_string {
			registry_adders_variables_string(function_registry<T>* owner_, bool human = false) : owner(owner_), human(human) {}

			registry_adders_variables_string& operator()(std::string key, typename filter_variable<T>::str_fun_type s_fun, typename filter_variable<T>::int_fun_type i_fun, std::string description) {
				boost::shared_ptr<filter_variable<T> > var(new filter_variable<T>(key, type_string, description));
				var->s_function = s_fun;
				var->i_function = i_fun;
				add_variables(var);
				return *this;
			}
			registry_adders_variables_string& operator()(std::string key, typename filter_variable<T>::str_fun_type fun, std::string description) {
				boost::shared_ptr<filter_variable<T> > var(new filter_variable<T>(key, type_string, description));
				var->s_function = fun;
				add_variables(var);
				return *this;
			}

		private:
			void add_variables(boost::shared_ptr<filter_variable<T> > d);
			function_registry<T>* owner;
			bool human;
		};

		template<class T>
		struct registry_adders_converters {
			registry_adders_converters(function_registry<T>* owner_) : owner(owner_) {}

			registry_adders_converters& operator()(value_type type, typename filter_converter<T>::converter_fun_type fun) {
				boost::shared_ptr<filter_converter<T> > var(new filter_converter<T>(type, fun));
				add_converter(var);
				return *this;
			}

		private:
			void add_converter(boost::shared_ptr<filter_converter<T> > d);
			function_registry<T>* owner;
		};

		template<class T>
		struct registry_adders_function {
			registry_adders_function(function_registry<T>* owner_, value_type type) : owner(owner_), type(type) {}

			registry_adders_function& operator()(std::string key, typename filter_function::generic_fun_type fun, std::string description) {
				boost::shared_ptr<filter_function> var(new filter_function(key));
				var->function = fun;
				var->type = type;
				var->description = description;
				add_functions(var);
				return *this;
			}
			registry_adders_function& operator()(std::string key, value_type type_, typename filter_function::generic_fun_type fun, std::string description) {
				boost::shared_ptr<filter_function> var(new filter_function(key));
				var->function = fun;
				var->type = type_;
				var->description = description;
				add_functions(var);
				return *this;
			}

		private:
			void add_functions(boost::shared_ptr<filter_function> d);
			function_registry<T>* owner;
			value_type type;
		};

		template<class T>
		struct function_registry {
			typedef std::map<std::string, boost::shared_ptr<filter_variable<T> > > variable_type;
			typedef std::map<std::string, boost::shared_ptr<filter_function> > function_type;
			typedef std::map<value_type, boost::shared_ptr<filter_converter<T> > > converter_type;
			variable_type variables;
			variable_type human_variables;
			function_type functions;
			converter_type converters;
			boost::shared_ptr<filter_variable<T> > last_variable;

			registry_adders_variables_int<T> add_int() {
				return registry_adders_variables_int<T>(this);
			}
			registry_adders_variables_number<T> add_number() {
				return registry_adders_variables_number<T>(this);
			}
			registry_adders_variables_float<T> add_float() {
				return registry_adders_variables_float<T>(this);
			}
			registry_adders_variables_string<T> add_string() {
				return registry_adders_variables_string<T>(this);
			}
			registry_adders_variables_int<T> add_human_int() {
				return registry_adders_variables_int<T>(this, true);
			}
			registry_adders_variables_string<T> add_human_string() {
				return registry_adders_variables_string<T>(this, true);
			}

			registry_adders_function<T> add_string_fun() {
				return registry_adders_function<T>(this, type_string);
			}
			registry_adders_function<T> add_int_fun() {
				return registry_adders_function<T>(this, type_int);
			}
			registry_adders_converters<T> add_converter() {
				return registry_adders_converters<T>(this);
			}

			bool has_converter(const value_type type) const {
				return converters.find(type) != converters.end();
			}
			bool has_variable(const std::string &key) const {
				return variables.find(key) != variables.end() || human_variables.find(key) != human_variables.end();
			}
			bool has_function(const std::string &key) const {
				return functions.find(key) != functions.end();
			}
			boost::shared_ptr<filter_variable<T> > get_variable(const std::string &key, bool human_readable) const {
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
				return boost::shared_ptr<filter_variable<T> >(new filter_variable<T>("dummy", type_tbd, "dummy"));
			}
			boost::shared_ptr<filter_converter<T> > get_converter(const value_type type) const {
				typename converter_type::const_iterator cit = converters.find(type);
				if (cit != converters.end()) {
					return cit->second;
				}
				return boost::shared_ptr<filter_converter<T> >(new filter_converter<T>(type_tbd));
			}
			std::list<std::string> get_variables() const {
				std::list<std::string> ret;
				BOOST_FOREACH(const typename variable_type::value_type &v, variables) {
					ret.push_back(v.first);
				}
				return ret;
			}
			boost::shared_ptr<filter_function> get_function(const std::string &key) const {
				typename function_type::const_iterator cit = functions.find(key);
				if (cit != functions.end()) {
					return cit->second;
				}
				return boost::shared_ptr<filter_function>(new filter_function("dummy"));
			}
			void add(boost::shared_ptr<filter_variable<T> > d, bool human) {
				if (human)
					human_variables[d->name] = d;
				else
					variables[d->name] = d;
				last_variable = d;
			}
			boost::shared_ptr<filter_variable<T> > get_last_variable() {
				return last_variable;
			}
			void add(boost::shared_ptr<filter_function> d) {
				functions[d->name] = d;
			}
			void add(boost::shared_ptr<filter_converter<T> > d) {
				converters[d->type] = d;
			}
		};

		template<class T>
		void registry_adders_variables_int<T>::add_variables(boost::shared_ptr<filter_variable<T> > d) {
			owner->add(d, human);
		}
		template<class T>
		void registry_adders_variables_float<T>::add_variables(boost::shared_ptr<filter_variable<T> > d) {
			owner->add(d, human);
		}
		template<class T>
		void registry_adders_variables_number<T>::add_variables(boost::shared_ptr<filter_variable<T> > d) {
			owner->add(d, human);
		}
		template<class T>
		boost::shared_ptr<filter_variable<T> > registry_adders_variables_int<T>::get_last() {
			return owner->get_last_variable();
		}
		template<class T>
		void registry_adders_variables_string<T>::add_variables(boost::shared_ptr<filter_variable<T> > d) {
			owner->add(d, human);
		}
		template<class T>
		void registry_adders_function<T>::add_functions(boost::shared_ptr<filter_function> d) {
			owner->add(d);
		}
		template<class T>
		void registry_adders_converters<T>::add_converter(boost::shared_ptr<filter_converter<T> > d) {
			owner->add(d);
		}

		template<class TObject>
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

			generic_summary() : count_match(0), count_ok(0), count_warn(0), count_crit(0), count_total(0), returnCode(NSCAPI::query_return_codes::returnOK) {}

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
				list_match = list_ok = list_warn = list_crit = "";
				returnCode = NSCAPI::query_return_codes::returnOK;
			}
			void count() {
				count_total++;
			}
			void matched(std::string &line) {
				str::format::append_list(list_match, line);
				count_match++;
			}
			void matched_unique() {
				count_match++;
			}
			bool has_matched() const {
				return count_match > 0;
			}
			void matched_ok(std::string &line) {
				str::format::append_list(list_ok, line);
				count_ok++;
			}
			void matched_warn(std::string &line) {
				str::format::append_list(list_warn, line);
				str::format::append_list(list_problem, line);
				count_warn++;
			}
			void matched_crit(std::string &line) {
				str::format::append_list(list_crit, line);
				str::format::append_list(list_problem, line);
				count_crit++;
			}
			void matched_ok_unique() {
				count_ok++;
			}
			void matched_warn_unique() {
				count_warn++;
			}
			void matched_crit_unique() {
				count_crit++;
			}

			std::string get_status() {
				return nscapi::plugin_helper::translateReturn(returnCode);
			}
			std::string get_list_match() {
				return list_match;
			}
			std::string get_list_ok() {
				return list_ok;
			}
			std::string get_list_warn() {
				return list_warn;
			}
			std::string get_list_crit() {
				return list_crit;
			}
			std::string get_list_problem() {
				return list_problem;
			}
			static void append_list(std::string &result, const std::string tag, const std::string &value) {
				if (!value.empty()) {
					if (!result.empty())
						result += ", ";
					result += tag + "(" + value + ")";
				}
			}
			std::string get_list_detail() {
				std::string ret;
				append_list(ret, "critical", list_crit);
				append_list(ret, "warning", list_warn);
				str::format::append_list(ret, list_ok);
				return ret;
			}
			long long get_count_match() {
				return count_match;
			}
			long long get_count_ok() {
				return count_ok;
			}
			long long get_count_warn() {
				return count_warn;
			}
			long long get_count_crit() {
				return count_crit;
			}
			long long get_count_problem() {
				return count_warn + count_crit;
			}
			long long get_count_total() {
				return count_total;
			}
			std::map<std::string,std::string> get_filter_syntax() const {
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
				return ret;
			}

			bool has_variable(const std::string &name) {
				return name == "count" || name == "total" || name == "ok_count" || name == "warn_count" || name == "crit_count" || name == "problem_count"
					|| name == "list" || name == "ok_list" || name == "warn_list" || name == "crit_list" || name == "problem_list" || name == "detail_list" || name == "lines"
					|| name == "status";
			}

			node_type create_variable(const std::string &name, bool human_readable = false);
		};

		template<class TObject>
		struct filter_handler_impl : public parsers::where::evaluation_context_impl<TObject> {
			typedef TObject object_type;
			typedef boost::function<std::string(object_type, evaluation_context)> bound_string_type;
			typedef boost::function<long long(object_type, evaluation_context)> bound_int_type;
			typedef boost::function<double(object_type, evaluation_context)> bound_float_type;
			typedef boost::function<node_type(const value_type, evaluation_context, const node_type)> bound_function_type;
			typedef function_registry<object_type> registry_type;

			registry_type registry_;

			std::map<std::string, std::string> get_filter_syntax() const {
				std::map<std::string, std::string> ret;
				BOOST_FOREACH(const typename registry_type::variable_type::value_type &var, registry_.variables) {
					ret[var.first] = var.second->description;
				}
				BOOST_FOREACH(const typename registry_type::function_type::value_type &var, registry_.functions) {
					ret[var.first + "()"] = var.second->description;
				}
				return ret;
			}

			virtual bool can_convert(std::string name, parsers::where::node_type subject, parsers::where::value_type to) {
				return registry_.has_converter(to);
			}

			virtual boost::shared_ptr<parsers::where::binary_function_impl> create_converter(std::string name, parsers::where::node_type subject, parsers::where::value_type to) {
				return registry_.get_converter(to);
			}

			virtual bool has_variable(const std::string &name) {
				return registry_.has_variable(name) || parsers::where::evaluation_context_impl<TObject>::get_summary()->has_variable(name);
			}
			virtual node_type create_variable(const std::string &name, bool human_readable) {
				if (registry_.has_variable(name)) {
					boost::shared_ptr<filter_variable<object_type> > var = registry_.get_variable(name, human_readable);
					if (var) {
						if (var->f_function) {
							if (var->float_perf.empty() && var->add_default_perf) {
								typename filter_variable<object_type>::float_perf_generator_type gen(new parsers::where::simple_number_performance_generator<object_type, double>("", "", "_" + var->name));
								var->float_perf.push_back(gen);
							}
							return node_type(new float_variable_node<filter_handler_impl>(name, var->type, var->f_function, var->float_perf));
						} else if (var->i_function) {
							if (var->int_perf.empty() && var->add_default_perf) {
								typename filter_variable<object_type>::int_perf_generator_type gen(new parsers::where::simple_number_performance_generator<object_type, long long>("", "", "_" + var->name));
								var->int_perf.push_back(gen);
							}
							if (var->s_function)
								return node_type(new dual_variable_node<filter_handler_impl>(name, var->type, var->i_function, var->s_function, var->int_perf));
							if (var->f_function)
								return node_type(new dual_variable_node<filter_handler_impl>(name, var->type, var->i_function, var->f_function, var->int_perf));
							return node_type(new int_variable_node<filter_handler_impl>(name, var->type, var->i_function, var->int_perf));
						} else if (var->s_function)
							return node_type(new str_variable_node<filter_handler_impl>(name, var->type, var->s_function));
					}
				} else if (parsers::where::evaluation_context_impl<TObject>::get_summary()->has_variable(name)) {
					return parsers::where::evaluation_context_impl<TObject>::get_summary()->create_variable(name);
				}
				this->error("Failed to find variable: " + name);
				return parsers::where::factory::create_false();
			}

			virtual bool has_function(const std::string &name) {
				return registry_.has_function(name);
			}
			virtual node_type create_text_function(const std::string &name) {
				return create_function(name, parsers::where::factory::create_list());
			}

			virtual variable_list_type get_variables() {
				return registry_.get_variables();
			}

			virtual node_type create_function(const std::string &name, node_type subject) {
				if (!registry_.has_function(name))
					return parsers::where::factory::create_false();
				boost::shared_ptr<filter_function> var = registry_.get_function(name);
				if (var) {
					if (helpers::type_is_int(var->type)) {
						if (var->function)
							return node_type(new custom_function_node(name, var->function, subject, var->type));
					}
					if (var->type == type_string) {
						if (var->function)
							return node_type(new custom_function_node(name, var->function, subject, var->type));
					}
				}
				return parsers::where::factory::create_false();
			}
			bool can_convert(parsers::where::value_type, parsers::where::value_type to) {
				if (registry_.has_converter(to))
					return true;
				return false;
			}

			typedef std::map<std::string, std::string> perf_object_options_type;
			typedef boost::unordered_map<std::string, perf_object_options_type> perf_options_type;
			perf_options_type perf_options;

			virtual bool has_performance_config_for_object(const std::string obj) const {
				return perf_options.find(obj) != perf_options.end();
			}
			virtual std::string get_performance_config_key(const std::string prefix, const std::string obj, const std::string suffix, const std::string key, const std::string v) const {
				std::string value = v;
				bool has_p = !prefix.empty();
				bool has_s = !suffix.empty();
				if (has_p&&has_s&&get_performance_config_value(prefix + "." + obj + "." + suffix, key, value))
					return value;
				if (has_p&&get_performance_config_value(prefix + "." + obj, key, value))
					return value;
				if (has_s&&get_performance_config_value(obj + "." + suffix, key, value))
					return value;
				if (has_p&&get_performance_config_value(prefix, key, value))
					return value;
				if (has_s&&get_performance_config_value(suffix, key, value))
					return value;
				if (get_performance_config_value(obj, key, value))
					return value;
				if (get_performance_config_value("*", key, value))
					return value;
				return value;
			}
			virtual bool get_performance_config_value(const std::string obj, const std::string key, std::string &value) const {
				perf_options_type::const_iterator cit = perf_options.find(obj);
				if (cit == perf_options.end())
					return false;
				perf_object_options_type::const_iterator cit2 = cit->second.find(key);
				if (cit2 == cit->second.end())
					return false;
				value = cit2->second;
				return true;
			}
			virtual void add_perf_config(const std::string &key, const std::map<std::string, std::string> &options) {
				perf_options[key] = options;
			}
		};

		template<class TObject>
		node_type generic_summary<TObject>::create_variable(const std::string &key, bool) {
			if (key == "count")
				return node_type(new summary_int_variable_node<parsers::where::evaluation_context_impl<TObject> >(key, boost::bind(&generic_summary<TObject>::get_count_match, _1)));
			if (key == "total")
				return node_type(new summary_int_variable_node<parsers::where::evaluation_context_impl<TObject> >(key, boost::bind(&generic_summary<TObject>::get_count_total, _1)));
			if (key == "ok_count")
				return node_type(new summary_int_variable_node<parsers::where::evaluation_context_impl<TObject> >(key, boost::bind(&generic_summary<TObject>::get_count_ok, _1)));
			if (key == "warn_count")
				return node_type(new summary_int_variable_node<parsers::where::evaluation_context_impl<TObject> >(key, boost::bind(&generic_summary<TObject>::get_count_warn, _1)));
			if (key == "crit_count")
				return node_type(new summary_int_variable_node<parsers::where::evaluation_context_impl<TObject> >(key, boost::bind(&generic_summary<TObject>::get_count_crit, _1)));
			if (key == "problem_count")
				return node_type(new summary_int_variable_node<parsers::where::evaluation_context_impl<TObject> >(key, boost::bind(&generic_summary<TObject>::get_count_problem, _1)));
			if (key == "list" || key == "match_list" || key == "lines")
				return node_type(new summary_string_variable_node<parsers::where::evaluation_context_impl<TObject> >(key, boost::bind(&generic_summary<TObject>::get_list_match, _1)));
			if (key == "ok_list")
				return node_type(new summary_string_variable_node<parsers::where::evaluation_context_impl<TObject> >(key, boost::bind(&generic_summary<TObject>::get_list_ok, _1)));
			if (key == "warn_list")
				return node_type(new summary_string_variable_node<parsers::where::evaluation_context_impl<TObject> >(key, boost::bind(&generic_summary<TObject>::get_list_warn, _1)));
			if (key == "crit_list")
				return node_type(new summary_string_variable_node<parsers::where::evaluation_context_impl<TObject> >(key, boost::bind(&generic_summary<TObject>::get_list_crit, _1)));
			if (key == "problem_list")
				return node_type(new summary_string_variable_node<parsers::where::evaluation_context_impl<TObject> >(key, boost::bind(&generic_summary<TObject>::get_list_problem, _1)));
			if (key == "detail_list")
				return node_type(new summary_string_variable_node<parsers::where::evaluation_context_impl<TObject> >(key, boost::bind(&generic_summary<TObject>::get_list_detail, _1)));
			if (key == "status")
				return node_type(new summary_string_variable_node<parsers::where::evaluation_context_impl<TObject> >(key, boost::bind(&generic_summary<TObject>::get_status, _1)));
			return parsers::where::factory::create_false();
		}

		template<class T>
		node_type filter_converter<T>::evaluate(value_type, evaluation_context context, const node_type subject) const {
			try {
				typedef filter_handler_impl<T>* native_context_type;
				native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
				if (!native_context->has_object()) {
					context->error("No object attached");
					return parsers::where::factory::create_false();
				}
				if (!function) {
					context->error("No function attached");
					return parsers::where::factory::create_false();
				}
				T obj = native_context->get_object();
				return function(obj, context, subject);
			} catch (const std::exception &e) {
				context->error("Failed to evaluate function: " + utf8::utf8_from_native(e.what()));
				return parsers::where::factory::create_false();
			}
		}
	}
}