#pragma once

#include <string>
#include <list>

#include <boost/shared_ptr.hpp>

#include <parsers/where/node.hpp>

namespace parsers {
	namespace where {

		struct list_node : public list_node_interface {
			list_node() {}
			std::list<node_type> value_;

			void push_back(node_type value) {
				value_.push_back(value);
			}

			std::string to_string() const;
			value_container get_value(evaluation_context context, int type) const;
			std::list<node_type> get_list_value(evaluation_context errors) const {
				return value_;
			}

			bool can_evaluate() const {
				return false;
			}
			node_type evaluate(evaluation_context context) const;
			bool bind(object_converter context);

			value_type infer_type(object_converter converter);
			value_type infer_type(object_converter converter, value_type suggestion);
			bool find_performance_data(evaluation_context context, performance_collector &collector);
			bool static_evaluate(evaluation_context context) const;
			bool require_object(evaluation_context context) const;

		};
	}
}
