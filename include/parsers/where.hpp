#pragma once

#include <parsers/where/expression_ast.hpp>

namespace parsers {
	namespace where {

		struct parser {
			expression_ast resulting_tree;
			std::string rest;
			bool parse(std::string expr);
			bool derive_types(filter_handler handler);
			bool static_eval(filter_handler handler);
			bool bind(filter_handler handler);
			bool evaluate(filter_handler handler);
			bool collect_perfkeys(std::map<std::string,std::string> &boundries, filter_handler handler);
			std::string result_as_tree() const;
		};
	}
}


