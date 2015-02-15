#include <strEx.h>

#include <parsers/where/unary_fun.hpp>
#include <parsers/where/helpers.hpp>

namespace parsers {
	namespace where {


		std::string unary_fun::to_string() const {
			return "{" + helpers::type_to_string(get_type()) +"}" + name + "(" + subject->to_string() + ")";
		}

		long long unary_fun::get_int_value(evaluation_context errors) const {
			return evaluate(errors)->get_int_value(errors);
		}
		std::string unary_fun::get_string_value(evaluation_context errors) const {
			return evaluate(errors)->get_string_value(errors);
		}
		std::list<node_type> unary_fun::get_list_value(evaluation_context errors) const {
			std::list<node_type> ret;
			BOOST_FOREACH(node_type n, subject->get_list_value(errors)) {
				if (function)
					ret.push_back(function->evaluate(get_type(), errors, n));
				else
					ret.push_back(factory::create_false());
			}
			return ret;
		}

		bool unary_fun::can_evaluate() const {
			return true;
		}
		boost::shared_ptr<any_node> unary_fun::evaluate(evaluation_context errors) const {
			if (function)
				return function->evaluate(get_type(), errors, subject);
			errors->error("Missing function binding: " + name + "bound: " + strEx::s::xtos(is_bound()));
			return factory::create_false();
		}
		bool unary_fun::bind(object_converter converter) {
			try {
				if (converter->can_convert(name, subject, get_type())) {
					function = converter->create_converter(name, subject, get_type());
				} else {
					function = op_factory::get_binary_function(converter, name, subject);
				}
				if (!function) {
					converter->error("Failed to create function: " + name);
					return false;
				}
				return true;
			} catch (...) {
				converter->error("Failed to bind function: " + name);
				return false;
			}
		}
		bool unary_fun::find_performance_data(evaluation_context context, performance_collector &collector) {
			if ((name == "convert") || (name == "auto_convert" || is_transparent(type_tbd) ) ) {
				performance_collector sub_collector;
				subject->find_performance_data(context, sub_collector);
				if (sub_collector.has_candidate_value()) {
					collector.set_candidate_value(shared_from_this());
				}
			}
			return false;
		}
		bool unary_fun::static_evaluate(evaluation_context context) const {
			if ((name == "convert") || (name == "auto_convert" || is_transparent(type_tbd) ) ) {
				return subject->static_evaluate(context);
			}
			subject->static_evaluate(context);
			return false;
		}
		bool unary_fun::require_object(evaluation_context context) const {
			return subject->require_object(context);
		}

		bool unary_fun::is_transparent(value_type) const {
			if (name == "neg")
				return true;
			return false;
		}

		bool unary_fun::is_bound() const {
			return static_cast<bool>(function);
		}		
	}
}
