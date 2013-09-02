#pragma once

#include <parsers/where/node.hpp>

namespace parsers {
	namespace where {

		struct unary_op : public any_node {
			unary_op(operators op, node_type const subject): op(op), subject(subject) {}


			virtual std::string to_string() const;

			virtual long long get_int_value(evaluation_context context) const;
			virtual std::string get_string_value(evaluation_context context) const;
			virtual std::list<node_type> get_list_value(evaluation_context errors) const;

			virtual bool can_evaluate() const;
			virtual node_type evaluate(evaluation_context context) const;
			virtual bool bind(object_converter context);
			value_type infer_type(object_converter converter);
			value_type infer_type(object_converter converter, value_type suggestion);
			virtual bool find_performance_data(evaluation_context context, performance_collector &collector);
			virtual bool static_evaluate(evaluation_context context) const;

		private:
			unary_op() {}
			operators op;
			node_type subject;
		};


	}
}