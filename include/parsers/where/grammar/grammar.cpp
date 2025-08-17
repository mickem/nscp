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

#include <fstream>
#include <iostream>
#include <parsers/where/grammar/grammar.hpp>
#include <parsers/where/list_node.hpp>

#ifdef WIN32
#pragma warning(push)
#pragma warning(disable : 4459)
#endif
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/phoenix/bind.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/object.hpp>
#include <boost/phoenix/operator.hpp>
#ifdef WIN32
#pragma warning(pop)
#endif

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;
namespace phoenix = boost::phoenix;

#if BOOST_VERSION > 105500
#define BOOST_SPIRIT_USE_PHOENIX_V3 1
#endif

namespace parsers {
namespace where {
struct build_function_convert_int {
#if BOOST_VERSION > 105500
  template <typename A>
  struct result {
    typedef node_type type;
  };
#else
  template <typename A, typename B>
  struct result {
    typedef node_type type;
  };
#endif
  node_type operator()(const long long value, const char unit) const {
    list_node_type args = factory::create_list();
    std::string unit_s(1, unit);
    args->push_back(factory::create_int(value));
    args->push_back(factory::create_string(unit_s));
    return factory::create_conversion(args);
  }
};
struct build_function_convert_float {
#if BOOST_VERSION > 105500
  template <typename A>
  struct result {
    typedef node_type type;
  };
#else
  template <typename A, typename B>
  struct result {
    typedef node_type type;
  };
#endif
  node_type operator()(const double value, const char unit) const {
    list_node_type args = factory::create_list();
    std::string unit_s(1, unit);
    args->push_back(factory::create_float(value));
    args->push_back(factory::create_string(unit_s));
    return factory::create_conversion(args);
  }
};

template <typename T>
struct strict_real_policies : qi::real_policies<T> {
  static bool const expect_dot = true;
};

qi::real_parser<double, strict_real_policies<double> > real;

///////////////////////////////////////////////////////////////////////////
//  Our calculator grammar
///////////////////////////////////////////////////////////////////////////
where_grammar::where_grammar(object_factory obj_factory) : where_grammar::base_type(expression, "where") {
  using qi::double_;
  using qi::long_long;

  boost::phoenix::function<build_function_convert_int> build_ic_int;
  boost::phoenix::function<build_function_convert_float> build_ic_float;

  // clang-format off
			expression
				= and_expr[qi::_val = qi::_1]
				>> *(charset::no_case["or"] >> and_expr)[qi::_val = phoenix::bind(&factory::create_bin_op, op_or, qi::_val, qi::_1)]
				;
			and_expr
				= not_expr[qi::_val = qi::_1]
				>> *(charset::no_case["and"] >> not_expr)[qi::_val = phoenix::bind(&factory::create_bin_op, op_and, qi::_val, qi::_1)]
				;
			not_expr
				= charset::no_case["not"] >> cond_expr[qi::_val = phoenix::bind(&factory::create_un_op, op_not, qi::_1)]
				| cond_expr[qi::_val = qi::_1]
				;

			cond_expr
				= (identifier_expr >> op >> identifier_expr)[qi::_val = phoenix::bind(&factory::create_bin_op, qi::_2, qi::_1, qi::_3)]
				| (identifier_expr >> charset::no_case["not in"]
					>> '(' >> list_expr >> ')')[qi::_val = phoenix::bind(&factory::create_bin_op, op_nin, qi::_1, qi::_2)]
				| (identifier_expr >> charset::no_case["in"]
					>> '(' >> list_expr >> ')')[qi::_val = phoenix::bind(&factory::create_bin_op, op_in, qi::_1, qi::_2)]
				| ('(' >> expression >> ')')[qi::_val = qi::_1]
				| (identifier_expr)[qi::_val = qi::_1]
				;

			identifier_expr
				= (identifier >> bitop >> identifier)[qi::_val = phoenix::bind(&factory::create_bin_op, qi::_2, qi::_1, qi::_3)]
				| ('(' >> identifier >> bitop >> identifier >> ')')[qi::_val = phoenix::bind(&factory::create_bin_op, qi::_2, qi::_1, qi::_3)]
				| identifier[qi::_val = qi::_1]
				;

			identifier
				= "str" >> string_literal_ex[qi::_val = phoenix::bind(&factory::create_string, qi::_1)]
				| (variable_name >> '(' >> list_expr >> ')')[qi::_val = phoenix::bind(&factory::create_fun, obj_factory, qi::_1, qi::_2)]
				| (variable_name >> '(' >> ')')[qi::_val = phoenix::bind(&factory::create_fun, obj_factory, qi::_1, factory::create_list())]
				| variable_name[qi::_val = phoenix::bind(&factory::create_variable, obj_factory, qi::_1)]
				| string_literal[qi::_val = phoenix::bind(&factory::create_string, qi::_1)]
				| qi::lexeme[
					(real >> (charset::alpha | charset::char_('%')))[qi::_val = build_ic_float(qi::_1, qi::_2)]
				]
				| qi::lexeme[
					(long_long >> (charset::alpha | charset::char_('%')))[qi::_val = build_ic_int(qi::_1, qi::_2)]
				]
				| real[qi::_val = phoenix::bind(&factory::create_float, qi::_1)]
				| long_long[qi::_val = phoenix::bind(&factory::create_int, qi::_1)]
				;

			list_expr
				= string_list[qi::_val = phoenix::bind(&list_helper<std::string>::make_node, qi::_1)]
				| float_list[qi::_val = phoenix::bind(&list_helper<double>::make_node, qi::_1)]
				| int_list[qi::_val = phoenix::bind(&list_helper<long long>::make_node, qi::_1)]
				;

			string_list
				= string_literal[qi::_val = qi::_1]
				>> *(',' >> string_literal)[qi::_val += qi::_1]
				| variable_name[qi::_val = qi::_1]
				>> *(',' >> variable_name)[qi::_val += qi::_1]
				;

			float_list
				= real[qi::_val = qi::_1]
				>> *(',' >> double_)[qi::_val += qi::_1]
				;

			int_list
				= long_long[qi::_val = qi::_1]
				>> *(',' >> long_long)[qi::_val += qi::_1]
				;

			op = qi::lit("<=")[qi::_val = op_le]
				| qi::lit("<")[qi::_val = op_lt]
				| qi::lit("=")[qi::_val = op_eq]
				| qi::lit("!=")[qi::_val = op_ne]
				| qi::lit(">=")[qi::_val = op_ge]
				| qi::lit(">")[qi::_val = op_gt]
				| charset::no_case[qi::lit("le")][qi::_val = op_le]
				| charset::no_case[qi::lit("lt")][qi::_val = op_lt]
				| charset::no_case[qi::lit("eq")][qi::_val = op_eq]
				| charset::no_case[qi::lit("ne")][qi::_val = op_ne]
				| charset::no_case[qi::lit("ge")][qi::_val = op_ge]
				| charset::no_case[qi::lit("gt")][qi::_val = op_gt]
				| charset::no_case[qi::lit("like")][qi::_val = op_like]
				| charset::no_case[qi::lit("regexp")][qi::_val = op_regexp]
				| charset::no_case[qi::lit("not like")][qi::_val = op_not_like]
				| charset::no_case[qi::lit("not regexp")][qi::_val = op_not_regexp]
				;

			bitop = qi::lit("&")[qi::_val = op_binand]
				| qi::lit("|")[qi::_val = op_binor]
				;

			variable_name
				= qi::lexeme[
					(charset::alpha)[qi::_val += qi::_1]
						>> *(charset::alnum | charset::char_('_'))[qi::_val += qi::_1]
				]
				;
			string_literal
				= qi::lexeme['\''
				>> +(charset::char_ - '\'')[qi::_val += qi::_1]
				>> '\'']
				;
			string_literal_ex
				= qi::lexeme['('
				>> +(charset::char_ - ')')[qi::_val += qi::_1]
				>> ')']
				;
  // clang-format on

  // 					qi::on_error<qi::fail>( expression , std::wcout
  // 						<< phoenix::val(_T("Error! Expecting "))
  // 						<< _4                               // what failed?
  // 						<< phoenix::val(_T(" here: \""))
  // 						<< phoenix::construct<std::wstring>(_3, _2)   // iterators to error-pos, end
  // 						<< phoenix::val(_T("\""))
  // 						<< std::endl
  //);

  using phoenix::val;
  qi::on_error<qi::fail>(expression, std::cout << val("Error! Expecting ") << std::endl);

  //				<< ("Error! Expecting ")
  //				<< _4                               // what failed?
  //				<< (" here: \"")
  //				<< construct<std::string>(_3, _2)   // iterators to error-pos, end
  //				<< ("\"")
  // 			<< std::endl
}
}  // namespace where
}  // namespace parsers