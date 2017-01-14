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