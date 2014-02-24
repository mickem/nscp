#include <iostream>
#include <fstream>

#include <parsers/where/grammar/grammar.hpp>
#include <parsers/where/list_node.hpp>

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;
namespace phoenix = boost::phoenix;

namespace parsers {
	namespace where {

		struct build_function_convert {
			template <typename A, typename B>
			struct result { typedef node_type type; };
			node_type operator()(const long long value, const char unit) const {
				list_node_type args = factory::create_list();
				std::string unit_s(1, unit);
				args->push_back(factory::create_int(value));
				args->push_back(factory::create_string(unit_s));
				return factory::create_conversion(args);
			}
		};

		///////////////////////////////////////////////////////////////////////////
		//  Our calculator grammar
		///////////////////////////////////////////////////////////////////////////
		where_grammar::where_grammar(object_factory obj_factory) : where_grammar::base_type(expression, "where") {
			using qi::_val;
			using qi::int_;
			using qi::_1;
			using qi::_2;
			using qi::_3;

			boost::phoenix::function<build_function_convert> build_ic;

			expression	
					= and_expr											[_val = _1]
						>> *(ascii::no_case["or"] >> and_expr)			[_val = phoenix::bind(&factory::create_bin_op, op_or, _val, _1) ]
					;
			and_expr
					= not_expr 											[_val = _1]
						>> *(ascii::no_case["and"] >> not_expr)			[_val = phoenix::bind(&factory::create_bin_op, op_and, _val, _1) ]
					;
			not_expr
					= ascii::no_case["not"] >> cond_expr				[_val = phoenix::bind(&factory::create_un_op, op_not, _1) ]
					| cond_expr 										[_val = _1]
					;

			cond_expr
					= (identifier_expr >> op >> identifier_expr)		[_val = phoenix::bind(&factory::create_bin_op, _2, _1, _3) ]
					| (identifier_expr >> ascii::no_case["not in"] 
						>> '(' >> list_expr >> ')')						[_val = phoenix::bind(&factory::create_bin_op, op_nin, _1, _2) ]
					| (identifier_expr >> ascii::no_case["in"] 
						>> '(' >> list_expr >> ')')						[_val = phoenix::bind(&factory::create_bin_op, op_in, _1, _2) ]
						| ('(' >> expression >> ')')						[_val = _1 ]
					| (identifier_expr)									[_val = _1 ]
					;

			identifier_expr
 					= (identifier >> bitop >> identifier)				[_val = phoenix::bind(&factory::create_bin_op, _2, _1, _3) ]
 					| ('(' >> identifier >> bitop >> identifier >> ')')	[_val = phoenix::bind(&factory::create_bin_op, _2, _1, _3) ]
					| identifier										[_val = _1 ]
					;

			identifier 
					= "str" >> string_literal_ex						[_val = phoenix::bind(&factory::create_string, _1) ]
					| (variable_name >> '(' >> list_expr >> ')')		[_val = phoenix::bind(&factory::create_fun, obj_factory, _1, _2) ]
					| (variable_name >> '(' >> ')')						[_val = phoenix::bind(&factory::create_fun, obj_factory, _1, factory::create_list()) ]
 					| variable_name										[_val = phoenix::bind(&factory::create_variable, obj_factory, _1) ]
 					| string_literal									[_val = phoenix::bind(&factory::create_string, _1) ]
					| qi::lexeme[
						(int_ >> (ascii::alpha|ascii::char_('%')))		[_val = build_ic(_1,_2) ]
					]
					| number											[_val = phoenix::bind(&factory::create_int, _1) ]
					;

			list_expr
					= string_list										[_val = phoenix::bind(&list_helper<std::string>::make_node, _1) ]
					| number_list										[_val = phoenix::bind(&list_helper<long long>::make_node, _1) ]
					;

			string_list
					= string_literal									[_val = _1 ]
 						>> *( ',' >> string_literal )					[_val += _1 ]
 					|	variable_name									[_val = _1 ]
 						>> *( ',' >> variable_name )					[_val += _1 ]
					;

			number_list
					= number											[_val = _1 ]
 						>> *( ',' >> number ) 							[_val += _1 ]
					;

			op 		= qi::lit("<=")										[_val = op_le]
					| qi::lit("<")										[_val = op_lt]
					| qi::lit("=")										[_val = op_eq]
					| qi::lit("!=")										[_val = op_ne]
					| qi::lit(">=")										[_val = op_ge]
					| qi::lit(">")										[_val = op_gt]
					| ascii::no_case[qi::lit("le")]						[_val = op_le]
					| ascii::no_case[qi::lit("lt")]						[_val = op_lt]
					| ascii::no_case[qi::lit("eq")]						[_val = op_eq]
					| ascii::no_case[qi::lit("ne")]						[_val = op_ne]
					| ascii::no_case[qi::lit("ge")]						[_val = op_ge]
					| ascii::no_case[qi::lit("gt")]						[_val = op_gt]
					| ascii::no_case[qi::lit("like")]					[_val = op_like]
					| ascii::no_case[qi::lit("regexp")]					[_val = op_regexp]
					| ascii::no_case[qi::lit("not like")]				[_val = op_not_like]
					| ascii::no_case[qi::lit("not regexp")]				[_val = op_not_regexp]
					;

			bitop 	= qi::lit("&")										[_val = op_binand]
					| qi::lit("|")										[_val = op_binor]
					;

			number
					= int_												[_val = _1]
					;
			variable_name
					= qi::lexeme[
							(ascii::alpha)								[_val += _1]
							>> *(ascii::alnum|ascii::char_('_'))		[_val += _1]
						]
					;
			string_literal
					= qi::lexeme[ '\'' 
							>>  +( ascii::char_ - '\'' )				[_val += _1] 
							>> '\''] 
					;
			string_literal_ex
					= qi::lexeme[ '(' 
							>>  +( ascii::char_ - ')' )					[_val += _1] 
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
					qi::on_error<qi::fail>( expression , std::cout << val("Error! Expecting ") << std::endl );

			//				<< ("Error! Expecting ")
			//				<< _4                               // what failed?
			//				<< (" here: \"")
			//				<< construct<std::string>(_3, _2)   // iterators to error-pos, end
			//				<< ("\"")
// 			<< std::endl

		}

	}
}


