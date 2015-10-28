#pragma once

#include <algorithm>

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

#include <parsers/where/node.hpp>

#include <utf8.hpp>

namespace parsers {
	namespace where {
		template<class TObject>
		struct int_performance_generator_interface {
			virtual bool is_configured() = 0;
			virtual void configure(const std::string key, object_factory context) = 0;
			virtual void eval(perf_list_type &list, evaluation_context context, std::string alias, long long current_value, long long warn, long long crit, TObject object) = 0;
		};

		template<class TContext>
		struct simple_int_performance_generator : public int_performance_generator_interface<TContext> {
			std::string unit;
			std::string prefix;
			std::string suffix;
			bool configured;
			bool ignored;
			simple_int_performance_generator(const std::string unit, std::string prefix, std::string suffix) : unit(unit), prefix(prefix), suffix(suffix), configured(false), ignored(false) {}
			simple_int_performance_generator(const std::string unit) : unit(unit), configured(false), ignored(false) {}
			virtual bool is_configured() { return configured; }
			virtual void configure(const std::string key, object_factory context) {
				std::string p = boost::trim_copy(prefix);
				std::string k = boost::trim_copy(key);
				std::string s = boost::trim_copy(suffix);
				unit = context->get_performance_config_key(p, k, s, "unit", unit);
				prefix = context->get_performance_config_key(p, k, s, "prefix", prefix);
				suffix = context->get_performance_config_key(p, k, s, "suffix", suffix);
				if (context->get_performance_config_key(p, k, s, "ignored", "false") == "true")
					ignored = true;
				configured = true;
			}
			virtual void eval(perf_list_type &list, evaluation_context context, std::string alias, long long current_value, long long warn, long long crit, TContext object) {
				if (ignored)
					return;
				performance_data data;
				performance_data::perf_value<long long> int_data;
				int_data.value = current_value;
				int_data.warn = warn;
				int_data.crit = crit;
				data.value_int = int_data;
				data.alias = prefix + alias + suffix;
				data.unit = unit;
				list.push_back(data);
			}
		};

		template<class TContext>
		struct percentage_int_performance_generator : public int_performance_generator_interface<TContext> {
			typedef boost::function<long long(TContext, evaluation_context)> maxfun_type;
			maxfun_type maxfun;
			std::string prefix;
			std::string suffix;
			bool configured;
			bool ignored;
			percentage_int_performance_generator(maxfun_type maxfun, std::string prefix, std::string suffix) : maxfun(maxfun), prefix(prefix), suffix(suffix), configured(false), ignored(false) {}
			virtual bool is_configured() { return configured; }
			virtual void configure(const std::string key, object_factory context) {
				std::string p = boost::trim_copy(prefix);
				std::string k = boost::trim_copy(key);
				std::string s = boost::trim_copy(suffix);
				prefix = context->get_performance_config_key(p, k, s, "prefix", prefix);
				suffix = context->get_performance_config_key(p, k, s, "suffix", suffix);
				if (context->get_performance_config_key(p, k, s, "ignored", "false") == "true")
					ignored = true;
				configured = true;
			}
			virtual void eval(perf_list_type &list, evaluation_context context, std::string alias, long long current_value, long long warn, long long crit, TContext object) {
				if (ignored)
					return;
				long long maximum = maxfun(object, context);
				performance_data data;
				performance_data::perf_value<double> double_data;
				if (maximum > 0) {
					double_data.value = static_cast<double>(current_value * 100 / maximum);
					double_data.warn = static_cast<double>(warn * 100 / maximum);
					double_data.crit = static_cast<double>(crit * 100 / maximum);
					double_data.maximum = 100;
					double_data.minimum = 0;
					data.value_double = double_data;
					data.alias = prefix + alias + suffix;
					data.unit = "%";
					list.push_back(data);
				}
			}
		};

		template<class TContext>
		struct scaled_byte_int_performance_generator : public int_performance_generator_interface<TContext> {
			typedef boost::function<long long(TContext, evaluation_context)> maxfun_type;
			maxfun_type minfun;
			maxfun_type maxfun;
			std::string prefix;
			std::string suffix;
			std::string unit;
			bool configured;
			bool ignored;
			scaled_byte_int_performance_generator(maxfun_type minfun, maxfun_type maxfun, std::string prefix, std::string suffix) : minfun(minfun), maxfun(maxfun), prefix(prefix), suffix(suffix), configured(false), ignored(false) {}
			scaled_byte_int_performance_generator(maxfun_type maxfun, std::string prefix, std::string suffix) : maxfun(maxfun), prefix(prefix), suffix(suffix), configured(false), ignored(false) {}
			scaled_byte_int_performance_generator(std::string prefix, std::string suffix) : prefix(prefix), suffix(suffix), configured(false), ignored(false) {}
			virtual bool is_configured() { return configured; }
			virtual void configure(const std::string key, object_factory context) {
				std::string p = boost::trim_copy(prefix);
				std::string k = boost::trim_copy(key);
				std::string s = boost::trim_copy(suffix);
				unit = context->get_performance_config_key(p, k, s, "unit", unit);
				prefix = context->get_performance_config_key(p, k, s, "prefix", prefix);
				suffix = context->get_performance_config_key(p, k, s, "suffix", suffix);
				if (context->get_performance_config_key(p, k, s, "ignored", "false") == "true")
					ignored = true;
				configured = true;
			}
			virtual void eval(perf_list_type &list, evaluation_context context, std::string alias, long long current_value, long long warn, long long crit, TContext object) {
				if (ignored)
					return;
				std::string active_unit = unit;
				long long max_value = 0;
				long long min_value = 0;
				if (maxfun)
					max_value = maxfun(object, context);
				if (minfun)
					min_value = minfun(object, context);
				if (active_unit.empty()) {
					long long m = current_value;
					if (warn > 0)
						m = (std::max)(m, warn);
					if (crit > 0)
						m = (std::max)(m, crit);
					if (max_value > 0)
						m = (std::min)(m, max_value);
					if (min_value > 0)
						m = (std::min)(m, min_value);
					active_unit = format::find_proper_unit_BKMG(m);
				}

				performance_data::perf_value<double> double_data;
				if (maxfun) {
					if (max_value > 0)
						double_data.maximum = format::convert_to_byte_units(max_value, active_unit);
					else
						double_data.maximum = max_value;
				}
				if (minfun) {
					if (min_value > 0)
						double_data.minimum = format::convert_to_byte_units(min_value, active_unit);
					else
						double_data.minimum = min_value;
				}
				double_data.warn = format::convert_to_byte_units(warn, active_unit);
				double_data.crit = format::convert_to_byte_units(crit, active_unit);
				double_data.value = format::convert_to_byte_units(current_value, active_unit);
				performance_data data;
				data.value_double = double_data;
				data.alias = prefix + alias + suffix;
				data.unit = active_unit;
				list.push_back(data);
			}
		};

		template<class TContext>
		struct int_variable_node : public any_node {
			std::string name_;
			typedef TContext* native_context_type;
			typedef typename TContext::bound_int_type function_type;
			typedef typename TContext::object_type object_type;
			typedef boost::shared_ptr<int_performance_generator_interface<object_type> > int_performance_generator;

			function_type fun;
			std::list<int_performance_generator> perfgen;

			int_variable_node(const std::string name, value_type type, function_type fun, std::list<int_performance_generator> perfgen) : any_node(type), name_(name), fun(fun), perfgen(perfgen) {}
			// TODO: add c-tors

			virtual std::list<node_type> get_list_value(evaluation_context errors) const {
				return std::list<node_type>();
			}
			virtual bool can_evaluate() const {
				return true;
			}
			virtual boost::shared_ptr<any_node> evaluate(evaluation_context context) const {
				try {
					native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
					if (native_context != NULL && fun && native_context->has_object())
						return factory::create_int(fun(native_context->get_object(), context));
					context->error("Failed to evaluate " + name_ + " no object instance");
				} catch (const std::exception &e) {
					context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
				}
				return factory::create_false();
			}
			virtual bool bind(object_converter context) {
				return true;
			}
			virtual bool static_evaluate(evaluation_context context) const {
				return false;
			}
			virtual bool require_object(evaluation_context context) const {
				return true;
			}
			value_container get_value(evaluation_context context, int type) const {
				if (type == type_int) {
					try {
						native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
						if (native_context != NULL && fun && native_context->has_object())
							return value_container::create_int(fun(native_context->get_object(), context));
						context->error("Failed to evaluate " + name_ + " no object instance");
						return value_container::create_nil();
					} catch (const std::exception &e) {
						context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
						return value_container::create_nil();
					}
				}
				context->error("Invalid type " + name_);
				return value_container::create_nil();
			}
			std::string to_string() const {
				return "(int)var:" + name_;
			}
			value_type infer_type(object_converter converter, value_type) {
				return get_type();
			}
			value_type infer_type(object_converter converter) {
				return get_type();
			}
			bool find_performance_data(evaluation_context context, performance_collector &collector) {
				collector.set_candidate_variable(name_);
				return false;
			}

			virtual perf_list_type get_performance_data(object_factory context, std::string alias, node_type warn, node_type crit, node_type minimum, node_type maximum) {
				perf_list_type ret;
				native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
				if (native_context != NULL && native_context->has_object()) {
					long long warn_value = 0;
					long long crit_value = 0;
					long long current_value = get_int_value(context);
					if (warn)
						warn_value = warn->get_int_value(context);
					if (crit)
						crit_value = crit->get_int_value(context);
					BOOST_FOREACH(int_performance_generator &p, perfgen) {
						if (!p->is_configured())
							p->configure(name_, context);
						p->eval(ret, context, alias, current_value, warn_value, crit_value, native_context->get_object());
					}
				}
				return ret;
			}
		};

		template<class TContext>
		struct str_variable_node : public any_node {
			std::string name_;
			typedef TContext* native_context_type;
			typedef typename TContext::bound_string_type function_type;

			function_type fun;

			str_variable_node(const std::string name, value_type type, function_type fun) : any_node(type), name_(name), fun(fun) {}
			// TODO: add c-tors

			virtual std::list<node_type> get_list_value(evaluation_context errors) const {
				return std::list<node_type>();
			}
			virtual bool can_evaluate() const {
				return true;
			}
			virtual boost::shared_ptr<any_node> evaluate(evaluation_context context) const {
				try {
					native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
					if (native_context != NULL && fun && native_context->has_object())
						return factory::create_string(fun(native_context->get_object(), context));
					context->error("Failed to evaluate " + name_ + " no object instance");
				} catch (const std::exception &e) {
					context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
				}
				return factory::create_false();
			}
			virtual bool bind(object_converter context) {
				return true;
			}
			virtual bool static_evaluate(evaluation_context context) const {
				return false;
			}
			virtual bool require_object(evaluation_context context) const {
				return true;
			}
			long long get_int_value(evaluation_context context) const {
				context->error("Invalid type: " + name_);
				return 0;
			}
			double get_float_value(evaluation_context context) const {
				context->error("Invalid type: " + name_);
				return 0;
			}
			value_container get_value(evaluation_context context, int type) const {
				if (type == type_string) {
					try {
						native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
						if (!native_context->has_object()) {
							context->error("Unbound function " + name_);
							return value_container::create_nil();
						}
						if (native_context != NULL && fun)
							return value_container::create_string(fun(native_context->get_object(), context));
						context->error("Failed to evaluate " + name_ + " no object instance");
						return value_container::create_nil();
					} catch (const std::exception &e) {
						context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
						return value_container::create_nil();
					}
				}
				context->error("Invalid type " + name_);
				return value_container::create_nil();
			}
			std::string to_string() const {
				return "(string)var:" + name_;
			}
			value_type infer_type(object_converter, value_type) {
				return get_type();
			}
			value_type infer_type(object_converter) {
				return get_type();
			}
			bool find_performance_data(evaluation_context context, performance_collector &collector) {
				collector.set_candidate_variable(name_);
				return false;
			}
		};

		template<class TContext>
		struct dual_variable_node : public any_node {
			std::string name_;
			typedef TContext* native_context_type;
			typedef typename TContext::bound_string_type s_function_type;
			typedef typename TContext::bound_int_type i_function_type;
			typedef typename TContext::object_type object_type;
			typedef boost::shared_ptr<int_performance_generator_interface<object_type> > int_performance_generator;
			value_type fallback_type;

			i_function_type i_fun;
			s_function_type s_fun;
			std::list<int_performance_generator> perfgen;

			dual_variable_node(const std::string name, value_type fallback_type, i_function_type i_fun, s_function_type s_fun, std::list<int_performance_generator> perfgen) : any_node(type_multi), name_(name), fallback_type(fallback_type), i_fun(i_fun), s_fun(s_fun), perfgen(perfgen) {}
			// TODO: add c-tors

			virtual std::list<node_type> get_list_value(evaluation_context errors) const {
				return std::list<node_type>();
			}
			virtual bool can_evaluate() const {
				return true;
			}
			virtual boost::shared_ptr<any_node> evaluate(evaluation_context context) const {
				// TODO!!!
				if (is_string()) {
					try {
						native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
						if (native_context != NULL && s_fun && native_context->has_object())
							return factory::create_string(s_fun(native_context->get_object(), context));
						context->error("Failed to evaluate " + name_ + " no object instance");
					} catch (const std::exception &e) {
						context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
					}
					return factory::create_false();
				} else {
					try {
						native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
						if (native_context != NULL && i_fun && native_context->has_object())
							return factory::create_int(i_fun(native_context->get_object(), context));
						context->error("Failed to evaluate " + name_ + " no object instance");
					} catch (const std::exception &e) {
						context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
					}
					return factory::create_false();
				}
			}
			virtual bool bind(object_converter context) {
				return true;
			}
			virtual bool static_evaluate(evaluation_context context) const {
				return false;
			}
			virtual bool require_object(evaluation_context context) const {
				return true;
			}
			value_container get_value(evaluation_context context, int type) const {
				if (type == type_int) {
					try {
						native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
						if (native_context != NULL && i_fun && native_context->has_object())
							return value_container::create_int(i_fun(native_context->get_object(), context));
						context->error("Failed to evaluate " + name_ + " no object instance");
					} catch (const std::exception &e) {
						context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
					}
				}
				if (type == type_string) {
					try {
						native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
						if (native_context != NULL && s_fun && native_context->has_object())
							return value_container::create_string(s_fun(native_context->get_object(), context));
						context->error("Failed to evaluate " + name_ + " no object instance");
					} catch (const std::exception &e) {
						context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
					}
				}
				context->error("Invalid type " + name_);
				return value_container::create_nil();
			}
			std::string to_string() const {
				if (is_int())
					return "(int)var:" + name_;
				else if (is_string())
					return "(string)var:" + name_;
				return "(?)var:" + name_;
			}
			value_type infer_type(object_converter converter, value_type suggestion) {
				if (suggestion == type_int) {
					set_type(type_int);
				} else if (suggestion == type_string) {
					set_type(type_string);
				} else if (suggestion == type_tbd) {
					set_type(fallback_type);
				}

				return get_type();
			}
			value_type infer_type(object_converter converter) {
				return get_type();
			}
			bool find_performance_data(evaluation_context context, performance_collector &collector) {
				if (get_type() != type_string)
					collector.set_candidate_variable(name_);
				return false;
			}

			virtual perf_list_type get_performance_data(object_factory context, std::string alias, node_type warn, node_type crit, node_type minimum, node_type maximum) {
				perf_list_type ret;
				native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
				if (native_context != NULL && native_context->has_object()) {
					long long warn_value = 0;
					long long crit_value = 0;
					long long current_value = get_int_value(context);
					if (warn)
						warn_value = warn->get_int_value(context);
					if (crit)
						crit_value = crit->get_int_value(context);
					BOOST_FOREACH(const int_performance_generator &p, perfgen) {
						if (!p->is_configured())
							p->configure(name_, context);
						p->eval(ret, context, alias, current_value, warn_value, crit_value, native_context->get_object());
					}
				}
				return ret;
			}
		};

		//////////////////////////////////////////////////////////////////////////

		struct custom_function_node : public any_node {
			typedef boost::function<node_type(const value_type, evaluation_context, const node_type)> bound_function_type;

			std::string name_;
			bound_function_type fun;
			node_type subject;

			custom_function_node(const std::string name, bound_function_type fun, node_type subject, value_type type) : any_node(type), name_(name), fun(fun), subject(subject) {}

			virtual std::list<node_type> get_list_value(evaluation_context errors) const {
				return std::list<node_type>();
			}
			virtual bool can_evaluate() const {
				return false;
			}
			virtual boost::shared_ptr<any_node> evaluate(evaluation_context context) const {
				try {
					if (fun)
						return fun(get_type(), context, subject);
					context->error("Failed to evaluate " + name_ + " no function");
				} catch (const std::exception &e) {
					context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
				}
				return factory::create_false();
			}
			virtual bool bind(object_converter context) {
				return true;
			}
			virtual bool static_evaluate(evaluation_context context) const {
				return false;
			}
			virtual bool require_object(evaluation_context context) const {
				return true;
			}
			value_container get_value(evaluation_context context, int type) const {
				return evaluate(context)->get_value(context, type);
			}
			std::string to_string() const {
				return "(string)fun:" + name_;
			}
			value_type infer_type(object_converter converter, value_type) {
				return type_string;
			}
			value_type infer_type(object_converter converter) {
				return type_string;
			}
			bool find_performance_data(evaluation_context context, performance_collector &) {
				//collector.set_candidate_variable(name_);
				return false;
			}
		};

		template<class TContext>
		struct summary_int_variable_node : public any_node {
			std::string name_;
			typedef TContext* native_context_type;
			typedef typename TContext::object_type object_type;
			typedef typename TContext::summary_type summary_type;
			typedef boost::shared_ptr<int_performance_generator_interface<object_type> > int_performance_generator;
			typedef typename boost::function<long long(summary_type)> function_type;

			function_type fun;

			summary_int_variable_node(const std::string name, function_type fun) : any_node(type_int), name_(name), fun(fun) {}

			virtual std::list<node_type> get_list_value(evaluation_context errors) const {
				return std::list<node_type>();
			}
			virtual bool can_evaluate() const {
				return true;
			}
			bool int_get_value(evaluation_context context, bool &summary, long long &value) const {
				try {
					native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
					if (native_context != NULL && fun) {
						summary = !native_context->has_object();
						value = fun(native_context->get_summary());
						return true;
					}
					context->error("Failed to evaluate " + name_ + " no function");
				} catch (const std::exception &e) {
					context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
				}
				return false;
			}
			virtual boost::shared_ptr<any_node> evaluate(evaluation_context context) const {
				long long value = 0;
				bool summary = false;
				if (!int_get_value(context, summary, value)) {
					return factory::create_false();
				}
				return factory::create_int(value);
			}
			virtual bool bind(object_converter context) {
				return true;
			}
			virtual bool static_evaluate(evaluation_context context) const {
				return false;
			}
			virtual bool require_object(evaluation_context context) const {
				return false;
			}
			value_container get_value(evaluation_context context, int type) const {
				if (type == type_int) {
					long long value = 0;
					bool summary = false;
					if (!int_get_value(context, summary, value)) {
						return value_container::create_nil();
					}
					return value_container::create_int(value, !summary);
				}
				context->error("Unknown type: " + name_);
				return value_container::create_nil();
			}
			std::string to_string() const {
				return "(int)var:" + name_;
			}
			value_type infer_type(object_converter converter, value_type) {
				return get_type();
			}
			value_type infer_type(object_converter converter) {
				return get_type();
			}
			bool find_performance_data(evaluation_context context, performance_collector &collector) {
				collector.set_candidate_variable(name_);
				return false;
			}
			virtual perf_list_type get_performance_data(object_factory context, std::string alias, node_type warn, node_type crit, node_type minimum, node_type maximum) {
				perf_list_type ret;
				native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
				if (native_context != NULL && !native_context->has_object()) {
					long long warn_value = 0;
					long long crit_value = 0;
					long long current_value = get_int_value(context);
					if (warn)
						warn_value = warn->get_int_value(context);
					if (crit)
						crit_value = crit->get_int_value(context);
					performance_data data;
					performance_data::perf_value<long long> int_data;
					int_data.value = current_value;
					int_data.warn = warn_value;
					int_data.crit = crit_value;
					data.value_int = int_data;
					data.alias = name_;
					ret.push_back(data);
				}
				return ret;
			}
		};
		template<class TContext>
		struct summary_string_variable_node : public any_node {
			std::string name_;
			typedef TContext* native_context_type;
			typedef typename TContext::object_type object_type;
			typedef typename TContext::summary_type summary_type;
			typedef boost::shared_ptr<int_performance_generator_interface<object_type> > int_performance_generator;
			typedef typename boost::function<std::string(summary_type)> function_type;

			function_type fun;

			summary_string_variable_node(const std::string name, function_type fun) : any_node(type_string), name_(name), fun(fun) {}

			virtual std::list<node_type> get_list_value(evaluation_context errors) const {
				return std::list<node_type>();
			}
			virtual bool can_evaluate() const {
				return true;
			}
			virtual boost::shared_ptr<any_node> evaluate(evaluation_context context) const {
				try {
					native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
					if (native_context != NULL && fun)
						return factory::create_string(fun(native_context->get_summary()));
					context->error("Failed to evaluate " + name_ + " no function");
				} catch (const std::exception &e) {
					context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
				}
				return factory::create_false();
			}
			virtual bool bind(object_converter context) {
				return true;
			}
			virtual bool static_evaluate(evaluation_context context) const {
				return false;
			}
			virtual bool require_object(evaluation_context context) const {
				return false;
			}
			value_container get_value(evaluation_context context, int type) const {
				if (type == type_int || type == type_float) {
					context->error("Function not numeric: " + name_);
					return value_container::create_nil();
				}
				if (type == type_string) {
					native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
					if (native_context != NULL && fun)
						return value_container::create_string(fun(native_context->get_summary()));
					context->error("Invalid function: " + name_);
					return value_container::create_nil();
				}
				context->error("Unknown type: " + name_);
				return value_container::create_nil();
			}
			std::string to_string() const {
				return "(str)var:" + name_;
			}
			value_type infer_type(object_converter converter, value_type) {
				return get_type();
			}
			value_type infer_type(object_converter converter) {
				return get_type();
			}
			bool find_performance_data(evaluation_context, performance_collector&) {
				return false;
			}
		};
	}
}