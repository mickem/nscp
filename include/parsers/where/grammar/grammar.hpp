/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <list>

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_object.hpp>
#include <boost/spirit/include/phoenix_bind.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/function.hpp>

#include <parsers/where/node.hpp>

namespace qi = boost::spirit::qi;

namespace charset = boost::spirit::standard;

namespace parsers {
	namespace where {
		template<class T>
		struct list_helper {
			std::list<T> list_;
			list_helper() {}
			list_helper(const T &value) { list_.push_back(value); }
			list_helper& operator+=(const T &value) {
				list_.push_back(value);
				return *this;
			}
			list_helper& operator=(const T &value) {
				list_.push_back(value);
				return *this;
			}

			node_type make_node() const {
				return factory::create_list(list_);
			}
		};

		struct where_grammar : qi::grammar<std::string::const_iterator, node_type(), charset::space_type> {
			typedef std::string::const_iterator iterator_type;
			where_grammar(object_factory obj_factory);

			qi::rule<iterator_type, node_type(), charset::space_type>  expression, and_expr, not_expr, cond_expr, identifier_expr, identifier, list_expr;
			qi::rule<iterator_type, std::string(), charset::space_type> string_literal, variable_name, string_literal_ex;
			qi::rule<iterator_type, operators(), charset::space_type> op, bitop;
			qi::rule<iterator_type, list_helper<std::string>(), charset::space_type> string_list;
			qi::rule<iterator_type, list_helper<long long>(), charset::space_type> int_list;
			qi::rule<iterator_type, list_helper<double>(), charset::space_type> float_list;
		};
	}
}