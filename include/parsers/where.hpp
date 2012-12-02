#pragma once

#include <parsers/where/expression_ast.hpp>

namespace parsers {
	namespace where {

		struct parser {
			expression_ast resulting_tree;
			std::wstring rest;
			bool parse(std::wstring expr);
			bool derive_types(filter_handler handler);
			bool static_eval(filter_handler handler);
			bool bind(filter_handler handler);
			bool evaluate(filter_handler handler);
			bool collect_perfkeys(filter_handler handler);
			std::wstring result_as_tree() const;
		};
	}
}


