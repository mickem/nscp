#pragma once

#include <parsers/where/node.hpp>

namespace parsers {
	namespace where {

		struct parser {
			node_type resulting_tree;
			std::string rest;
			bool parse(object_factory factory, std::string expr);
			bool derive_types(object_converter converter);
			bool static_eval(evaluation_context context);
			bool bind(object_converter context);
			value_container evaluate(evaluation_context context);
			bool collect_perfkeys(evaluation_context context, performance_collector &boundries);
			std::string result_as_tree() const;
			bool require_object(evaluation_context context) const;

		};
	}
}


