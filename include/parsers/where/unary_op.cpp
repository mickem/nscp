#include <parsers/where/unary_op.hpp>
#include <parsers/operators.hpp>
#include <parsers/where/helpers.hpp>

namespace parsers {
	namespace where {
		std::string unary_op::to_string() const {
			return helpers::operator_to_string(op) + " ( " + subject->to_string() + " ) ";
		}

		value_container unary_op::get_value(evaluation_context errors, value_type type) const {
			return evaluate(errors)->get_value(errors, type);
		}
		std::list<node_type> unary_op::get_list_value(evaluation_context errors) const {
			return std::list<node_type>();
		}

		bool unary_op::can_evaluate() const {
			return true;
		}
		node_type unary_op::evaluate(evaluation_context errors) const {
			op_factory::un_op_type impl = op_factory::get_unary_operator(op);
			value_type type = helpers::get_return_type(op, type_invalid);
			if (helpers::type_is_int(type)) {
				return impl->evaluate(errors, subject);
			}
			errors->error("Missing operator implementation");
			return factory::create_false();
		}
		bool unary_op::bind(object_converter errors) {
			return subject->bind(errors);
		}
		value_type unary_op::infer_type(object_converter converter, value_type suggestion) {
			value_type type = subject->infer_type(converter, suggestion);
			return helpers::get_return_type(op, type);
		}
		value_type unary_op::infer_type(object_converter converter) {
			value_type type = subject->infer_type(converter);
			return helpers::get_return_type(op, type);
		}
		bool unary_op::find_performance_data(evaluation_context context, performance_collector &collector) {
			return subject->find_performance_data(context, collector);
		}
		bool unary_op::static_evaluate(evaluation_context errors) const {
			return subject->static_evaluate(errors);
		}
		bool unary_op::require_object(evaluation_context errors) const {
			return subject->require_object(errors);
		}
	}
}