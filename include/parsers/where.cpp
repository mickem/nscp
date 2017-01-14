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

#include <list>
#include <iostream>
#include <sstream>

// #include <boost/spirit/include/qi.hpp>
// #include <boost/spirit/include/phoenix_core.hpp>
// #include <boost/spirit/include/phoenix_operator.hpp>
// #include <boost/spirit/include/phoenix_object.hpp>
// #include <boost/fusion/include/adapt_struct.hpp>
// #include <boost/fusion/include/io.hpp>
// #include <boost/function.hpp>

#include <parsers/where.hpp>
#include <parsers/where/node.hpp>

#include <parsers/helpers.hpp>
#include <parsers/where/grammar/grammar.hpp>

namespace parsers {
	namespace where {
		bool parser::parse(object_factory factory, std::string expr) {
			constants::reset();

			where_grammar calc(factory);

			where_grammar::iterator_type iter = expr.begin();
			where_grammar::iterator_type end = expr.end();
			if (phrase_parse(iter, end, calc, charset::space, resulting_tree)) {
				rest = std::string(iter, end);
				return rest.empty();
			}
			rest = std::string(iter, end);
			return false;
		}

		bool parser::derive_types(object_converter converter) {
			try {
				resulting_tree->infer_type(converter);
				return true;
			} catch (...) {
				converter->error("Unhandled exception resolving types: " + result_as_tree());
				return false;
			}
		}

		bool parser::static_eval(evaluation_context context) {
			try {
				resulting_tree->static_evaluate(context);
				return true;
			} catch (const std::exception &e) {
				context->error(std::string("Unhandled exception static eval: ") + e.what());
				return false;
			} catch (...) {
				context->error("Unhandled exception static eval: " + result_as_tree());
				return false;
			}
		}
		bool parser::collect_perfkeys(evaluation_context context, performance_collector &boundries) {
			try {
				resulting_tree->find_performance_data(context, boundries);
				return true;
			} catch (...) {
				context->error("Unhandled exception collecting performance data eval: " + result_as_tree());
				return false;
			}
		}

		bool parser::bind(object_converter context) {
			try {
				resulting_tree->bind(context);
				return true;
			} catch (const std::exception &e) {
				context->error(std::string("Bind exception: ") + e.what());
				return false;
			} catch (...) {
				context->error("Bind exception: " + result_as_tree());
				return false;
			}
		}

		bool parser::require_object(evaluation_context context) const {
			return resulting_tree->require_object(context);
		}

		value_container parser::evaluate(evaluation_context context) {
			try {
				node_type result = resulting_tree->evaluate(context);
				return result->get_value(context, type_int);
			} catch (const std::exception &e) {
				context->error(std::string("Evaluate exception: ") + e.what());
				return value_container::create_nil();
			} catch (...) {
				context->error("Evaluate exception: " + result_as_tree());
				return value_container::create_nil();
			}
		}

		std::string parser::result_as_tree() const {
			return resulting_tree->to_string();
		}
		std::string parser::result_as_tree(evaluation_context context) const {
			return resulting_tree->to_string(context);
		}
	}
}