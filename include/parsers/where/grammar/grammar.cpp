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

#include <iostream>
#include <fstream>

#include <parsers/where/grammar/grammar.hpp>
#include <parsers/where/list_node.hpp>

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
			struct result { typedef node_type type; };
#else
			template <typename A, typename B>
			struct result { typedef node_type type; };
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
			struct result { typedef node_type type; };
#else
			template <typename A, typename B>
			struct result { typedef node_type type; };
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
			using qi::_val;
			using qi::long_long;
			using qi::double_;
			using qi::_1;
			using qi::_2;
			using qi::_3;

			boost::phoenix::function<build_function_convert_int> build_ic_int;
			boost::phoenix::function<build_function_convert_float> build_ic_float;

			expression
				= and_expr[_val = _1]
				>> *(charset::no_case["or"] >> and_expr)[_val = phoenix::bind(&factory::create_bin_op, op_or, _val, _1)]
				;
			and_expr
				= not_expr[_val = _1]
				>> *(charset::no_case["and"] >> not_expr)[_val = phoenix::bind(&factory::create_bin_op, op_and, _val, _1)]
				;
			not_expr
				= charset::no_case["not"] >> cond_expr[_val = phoenix::bind(&factory::create_un_op, op_not, _1)]
				| cond_expr[_val = _1]
				;

			cond_expr
				= (identifier_expr >> op >> identifier_expr)[_val = phoenix::bind(&factory::create_bin_op, _2, _1, _3)]
				| (identifier_expr >> charset::no_case["not in"]
					>> '(' >> list_expr >> ')')[_val = phoenix::bind(&factory::create_bin_op, op_nin, _1, _2)]
				| (identifier_expr >> charset::no_case["in"]
					>> '(' >> list_expr >> ')')[_val = phoenix::bind(&factory::create_bin_op, op_in, _1, _2)]
				| ('(' >> expression >> ')')[_val = _1]
				| (identifier_expr)[_val = _1]
				;

			identifier_expr
				= (identifier >> bitop >> identifier)[_val = phoenix::bind(&factory::create_bin_op, _2, _1, _3)]
				| ('(' >> identifier >> bitop >> identifier >> ')')[_val = phoenix::bind(&factory::create_bin_op, _2, _1, _3)]
				| identifier[_val = _1]
				;

			identifier
				= "str" >> string_literal_ex[_val = phoenix::bind(&factory::create_string, _1)]
				| (variable_name >> '(' >> list_expr >> ')')[_val = phoenix::bind(&factory::create_fun, obj_factory, _1, _2)]
				| (variable_name >> '(' >> ')')[_val = phoenix::bind(&factory::create_fun, obj_factory, _1, factory::create_list())]
				| variable_name[_val = phoenix::bind(&factory::create_variable, obj_factory, _1)]
				| string_literal[_val = phoenix::bind(&factory::create_string, _1)]
				| qi::lexeme[
					(real >> (charset::alpha | charset::char_('%')))[_val = build_ic_float(_1, _2)]
				]
				| qi::lexeme[
					(long_long >> (charset::alpha | charset::char_('%')))[_val = build_ic_int(_1, _2)]
				]
				| real[_val = phoenix::bind(&factory::create_float, _1)]
				| long_long[_val = phoenix::bind(&factory::create_int, _1)]
				;

			list_expr
				= string_list[_val = phoenix::bind(&list_helper<std::string>::make_node, _1)]
				| float_list[_val = phoenix::bind(&list_helper<double>::make_node, _1)]
				| int_list[_val = phoenix::bind(&list_helper<long long>::make_node, _1)]
				;

			string_list
				= string_literal[_val = _1]
				>> *(',' >> string_literal)[_val += _1]
				| variable_name[_val = _1]
				>> *(',' >> variable_name)[_val += _1]
				;

			float_list
				= real[_val = _1]
				>> *(',' >> double_)[_val += _1]
				;

			int_list
				= long_long[_val = _1]
				>> *(',' >> long_long)[_val += _1]
				;

			op = qi::lit("<=")[_val = op_le]
				| qi::lit("<")[_val = op_lt]
				| qi::lit("=")[_val = op_eq]
				| qi::lit("!=")[_val = op_ne]
				| qi::lit(">=")[_val = op_ge]
				| qi::lit(">")[_val = op_gt]
				| charset::no_case[qi::lit("le")][_val = op_le]
				| charset::no_case[qi::lit("lt")][_val = op_lt]
				| charset::no_case[qi::lit("eq")][_val = op_eq]
				| charset::no_case[qi::lit("ne")][_val = op_ne]
				| charset::no_case[qi::lit("ge")][_val = op_ge]
				| charset::no_case[qi::lit("gt")][_val = op_gt]
				| charset::no_case[qi::lit("like")][_val = op_like]
				| charset::no_case[qi::lit("regexp")][_val = op_regexp]
				| charset::no_case[qi::lit("not like")][_val = op_not_like]
				| charset::no_case[qi::lit("not regexp")][_val = op_not_regexp]
				;

			bitop = qi::lit("&")[_val = op_binand]
				| qi::lit("|")[_val = op_binor]
				;

			variable_name
				= qi::lexeme[
					(charset::alpha)[_val += _1]
						>> *(charset::alnum | charset::char_('_'))[_val += _1]
				]
				;
			string_literal
				= qi::lexeme['\''
				>> +(charset::char_ - '\'')[_val += _1]
				>> '\'']
				;
			string_literal_ex
				= qi::lexeme['('
				>> +(charset::char_ - ')')[_val += _1]
				>> ')']
				;

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
	}
}