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
			virtual void eval(perf_list_type &list, evaluation_context context, std::string alias, long long current_value, long long warn, long long crit, TObject object) = 0;
		};

		template<class TContext>
		struct simple_int_performance_generator : public int_performance_generator_interface<TContext> {
			std::string unit;
			std::string prefix;
			std::string suffix;
			simple_int_performance_generator(const std::string unit, std::string prefix, std::string suffix) : unit(unit), prefix(prefix), suffix(suffix) {}
			simple_int_performance_generator(const std::string unit) : unit(unit) {}
			virtual void eval (perf_list_type &list, evaluation_context context, std::string alias, long long current_value, long long warn, long long crit, TContext object) {
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
			typedef boost::function<long long(TContext)> maxfun_type;
			maxfun_type maxfun;
			std::string prefix;
			std::string suffix;
			percentage_int_performance_generator(maxfun_type maxfun, std::string prefix, std::string suffix) : maxfun(maxfun), prefix(prefix), suffix(suffix) {}
			virtual void eval (perf_list_type &list, evaluation_context context, std::string alias, long long current_value, long long warn, long long crit, TContext object) {
				long long maximum = maxfun(object);
				performance_data data;
				performance_data::perf_value<double> double_data;
				if (maximum > 0) {
					double_data.value = static_cast<double>(current_value*100/maximum);
					double_data.warn = static_cast<double>(warn*100/maximum);
					double_data.crit = static_cast<double>(crit*100/maximum);
					double_data.maximum = 100;
					double_data.minimum = 0;
				}
				data.value_double = double_data;
				data.alias = prefix + alias + suffix;
				data.unit = "%";
				list.push_back(data);
			}
		};

		template<class TContext>
		struct scaled_byte_int_performance_generator : public int_performance_generator_interface<TContext> {
			typedef boost::function<long long(TContext)> maxfun_type;
			maxfun_type minfun;
			maxfun_type maxfun;
			std::string prefix;
			std::string suffix;
			scaled_byte_int_performance_generator(maxfun_type minfun, maxfun_type maxfun, std::string prefix, std::string suffix) : minfun(minfun), maxfun(maxfun), prefix(prefix), suffix(suffix) {}
			scaled_byte_int_performance_generator(maxfun_type maxfun, std::string prefix, std::string suffix) : maxfun(maxfun), prefix(prefix) , suffix(suffix){}
			scaled_byte_int_performance_generator(std::string prefix, std::string suffix) : prefix(prefix), suffix(suffix) {}
			virtual void eval (perf_list_type &list, evaluation_context context, std::string alias, long long current_value, long long warn, long long crit, TContext object) {
				long long m = current_value;
				if (warn > 0)
					m = (std::max)(m, warn);
				if (crit > 0)
					m = (std::max)(m, crit);
				long long max_value, min_value;
				if (maxfun) {
					max_value = maxfun(object);
					if (max_value > 0)
						m = (std::min)(m, max_value);
				}
				if (minfun) {
					min_value = minfun(object);
					if (min_value > 0)
						m = (std::min)(m, min_value);
				}
				std::string unit = format::find_proper_unit_BKMG(m);

				performance_data::perf_value<double> double_data;
				if (maxfun) {
					if (max_value > 0)
						double_data.maximum = format::convert_to_byte_units(max_value, unit);
					else
						double_data.maximum = max_value;
				}
				if (minfun) {
					if (min_value > 0)
						double_data.minimum = format::convert_to_byte_units(min_value, unit);
					else
						double_data.minimum = min_value;
				}
				double_data.warn = format::convert_to_byte_units(warn, unit);
				double_data.crit = format::convert_to_byte_units(crit, unit);
				double_data.value = format::convert_to_byte_units(current_value, unit);
				performance_data data;
				data.value_double = double_data;
				data.alias = prefix + alias + suffix;
				data.unit = unit;
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
					if (native_context != NULL && fun)
						return factory::create_int(fun(native_context->get_object(), context));
					context->error("Failed to evaluate " + name_);
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
			long long get_int_value(evaluation_context context) const {
				try {
					native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
					if (native_context != NULL && fun)
						return fun(native_context->get_object(), context);
					context->error("Failed to evaluate " + name_);
				} catch (const std::exception &e) {
					context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
				}
				return 0;
			}
			std::string get_string_value(evaluation_context context) const {
				return strEx::s::xtos(get_int_value(context));
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

			virtual perf_list_type get_performance_data(evaluation_context context, std::string alias, node_type warn, node_type crit, node_type minimum, node_type maximum)  {
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
					context->error("Failed to evaluate " + name_);
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
			long long get_int_value(evaluation_context context) const {
				context->error("Invalid type: " + name_);
				return 0;
			}
			std::string get_string_value(evaluation_context context) const {
				try {
					native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
					if (!native_context->has_object())
						return "";
					if (native_context != NULL && fun)
						return fun(native_context->get_object(), context);
					context->error("Failed to evaluate " + name_);
				} catch (const std::exception &e) {
					context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
				}
				return "";
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
						context->error("Failed to evaluate " + name_);
					} catch (const std::exception &e) {
						context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
					}
					return factory::create_false();
				} else {
					try {
						native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
						if (native_context != NULL && i_fun && native_context->has_object())
							return factory::create_int(i_fun(native_context->get_object(), context));
						context->error("Failed to evaluate " + name_);
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
			long long get_int_value(evaluation_context context) const {
				try {
					native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
					if (native_context != NULL && i_fun && native_context->has_object())
						return i_fun(native_context->get_object(), context);
					context->error("Failed to evaluate " + name_);
				} catch (const std::exception &e) {
					context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
				}
				return 0;
			}
			std::string get_string_value(evaluation_context context) const {
				try {
					native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
					if (native_context != NULL && s_fun && native_context->has_object())
						return s_fun(native_context->get_object(), context);
					context->error("Failed to evaluate " + name_);
				} catch (const std::exception &e) {
					context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
				}
				return "";
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
				}
				else if (suggestion == type_string) {
					set_type(type_string);
				}
				else if (suggestion == type_tbd) {
					set_type(fallback_type);
				}
				
				return get_type();
			}
			value_type infer_type(object_converter converter) {
				return get_type();
			}
			bool find_performance_data(evaluation_context context, performance_collector &collector) {
				collector.set_candidate_variable(name_);
				return false;
			}

			virtual perf_list_type get_performance_data(evaluation_context context, std::string alias, node_type warn, node_type crit, node_type minimum, node_type maximum)  {
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
						p->eval(ret, context, alias, current_value, warn_value, crit_value, native_context->get_object());
					}
				}
				return ret;
			}

		};


		//////////////////////////////////////////////////////////////////////////

		struct custom_function_node : public any_node {
			typedef boost::function<node_type(const value_type,evaluation_context,const node_type)> bound_function_type;

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
					context->error("Failed to evaluate " + name_);
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
			long long get_int_value(evaluation_context context) const {
				return evaluate(context)->get_int_value(context);
			}
			std::string get_string_value(evaluation_context context) const {
				return evaluate(context)->get_string_value(context);
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
			virtual boost::shared_ptr<any_node> evaluate(evaluation_context context) const {
				try {
					native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
					if (native_context != NULL && fun)
						return factory::create_int(fun(native_context->get_summary()));
					context->error("Failed to evaluate " + name_);
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
			long long get_int_value(evaluation_context context) const {
				try {
					native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
					if (native_context != NULL && fun)
						return fun(native_context->get_summary());
					context->error("Failed to evaluate " + name_);
				} catch (const std::exception &e) {
					context->error("Failed to evaluate " + name_ + ": " + utf8::utf8_from_native(e.what()));
				}
				return 0;
			}
			std::string get_string_value(evaluation_context context) const {
				return strEx::s::xtos(get_int_value(context));
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
			virtual perf_list_type get_performance_data(evaluation_context context, std::string alias, node_type warn, node_type crit, node_type minimum, node_type maximum)  {
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
					context->error("Failed to evaluate " + name_);
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
			long long get_int_value(evaluation_context context) const {
				context->error("Function not numeric: " + name_);
				return 0;
			}
			std::string get_string_value(evaluation_context context) const {
				native_context_type native_context = reinterpret_cast<native_context_type>(context.get());
				if (native_context != NULL && fun)
					return fun(native_context->get_summary());
				context->error("Invalid function: " + name_);
				return "";
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
