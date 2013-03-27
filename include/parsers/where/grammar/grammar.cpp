#include <parsers/where/grammar/grammar.hpp>
#include <iostream>
#include <fstream>


#include <parsers/where/unary_fun.hpp>
#include <parsers/where/list_value.hpp>
#include <parsers/where/binary_op.hpp>
#include <parsers/where/unary_op.hpp>
#include <parsers/where/variable.hpp>

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;
namespace phoenix = boost::phoenix;

namespace parsers {
	namespace where {


		struct build_expr {
			template <typename A, typename B = boost::spirit::unused_type, typename C = boost::spirit::unused_type>
			struct result { typedef expression_ast type; };
			
			expression_ast operator()(expression_ast const & expr1, operators const & op, expression_ast const & expr2) const {
				return expression_ast(binary_op(op, expr1, expr2));
			}
		};

		struct build_string {
			template <typename A>
			struct result { typedef expression_ast type; };

			template <typename A>
			expression_ast operator()(A const & v) const {
				return expression_ast(string_value(v));
			}
		};

		struct build_copy {
			template <typename A>
			struct result { typedef expression_ast type; };

			template <typename A>
			expression_ast operator()(A const v) const {
				return v;
			}
		};

		struct build_int {
			template <typename A>
			struct result { typedef expression_ast type; };

			//template <typename A>
			expression_ast operator()(unsigned int const & v) const {
				return expression_ast(int_value(v));
			}
		};

		struct build_variable {
			template <typename A>
			struct result { typedef expression_ast type; };

			//template <typename A>
			expression_ast operator()(std::string const & v) const {
				return expression_ast(variable(v));
			}
		};

		struct build_function {
			template <typename A, typename B>
			struct result { typedef expression_ast type; };
			expression_ast operator()(std::string const name, expression_ast const & var) const {
				return expression_ast(unary_fun(name, var));
			}
		};

		struct build_function_convert {
			template <typename A, typename B>
			struct result { typedef expression_ast type; };
			expression_ast operator()(wchar_t const unit, expression_ast const & vars) const {
				list_value args = list_value(vars);
				args += expression_ast(string_value(std::string(1, unit)));
				return expression_ast(unary_fun("convert", args));
			}
		};


		struct build {

		};


		///////////////////////////////////////////////////////////////////////////
		//  Our calculator grammar
		///////////////////////////////////////////////////////////////////////////
		where_grammar::where_grammar() : where_grammar::base_type(expression, "where") {
			using qi::_val;
			using qi::uint_;
			using qi::_1;
			using qi::_2;
			using qi::_3;

			boost::phoenix::function<build_expr> build_e;
			boost::phoenix::function<build_string> build_is;
			boost::phoenix::function<build_int> build_ii;
			boost::phoenix::function<build_variable> build_iv;
			boost::phoenix::function<build_function> build_if;
			boost::phoenix::function<build_function_convert> build_ic;
			boost::phoenix::function<build_copy> build_c;

			expression	
					= and_expr											[_val = _1]
						>> *(ascii::no_case["or"] >> and_expr)			[_val |= _1]
					;
			and_expr
					= not_expr 											[_val = _1]
						>> *(ascii::no_case["and"] >> not_expr)			[_val &= _1]
					;
			not_expr
					= cond_expr 										[_val = _1]
						>> *(ascii::no_case["not"] >> cond_expr)		[_val != _1]
					;

			cond_expr
					= (identifier_expr >> op >> identifier_expr)		[_val = build_e(_1, _2, _3) ]
					| (identifier_expr >> ascii::no_case["not in"] 
						>> '(' >> value_list >> ')')					[_val = build_e(_1, op_nin, _2) ]
					| (identifier_expr >> ascii::no_case["in"] 
						>> '(' >> value_list >> ')')					[_val = build_e(_1, op_in, _2) ]
						| ('(' >> expression >> ')')					[_val = _1 ]
					;

			identifier_expr
 					= (identifier >> bitop >> identifier)				[_val = build_e(_1, _2, _3) ]
 					| ('(' >> identifier >> bitop >> identifier >> ')')	[_val = build_e(_1, _2, _3) ]
					| identifier										[_val = _1 ]
					;

			identifier 
					= "str" >> string_literal_ex						[_val = build_is(_1)]
					| (variable_name >> '(' >> list_expr >> ')')		[_val = build_if(_1, _2)]
					| variable_name										[_val = build_iv(_1)]
					| string_literal									[_val = build_is(_1)]
					| qi::lexeme[
						(uint_ >> ascii::alpha)							[_val = build_ic(_2, build_ii(_1))]
						]
					| '-' >> qi::lexeme[
						(uint_ >> ascii::alpha)							[_val = build_if(std::string("neg"), build_ic(_2, build_ii(_1)))]
						]
					| number											[_val = build_ii(_1)]
					| '-' >> number										[_val = build_if(std::string("neg"), build_ii(_1))]
					;

			list_expr
					= value_list										[_val = _1 ]
					;

			value_list
					= string_literal									[_val = build_is(_1) ]
						>> *( ',' >> string_literal )					[_val += build_is(_1) ]
					|	number											[_val = build_ii(_1) ]
						>> *( ',' >> number ) 							[_val += build_ii(_1) ]
					|	variable_name									[_val = build_is(_1) ]
						>> *( ',' >> variable_name )					[_val += build_is(_1) ]
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
					= uint_												[_val = _1]
					;
			variable_name
					= qi::lexeme[
							(ascii::alpha)							[_val += _1]
							>> *(ascii::alnum|ascii::char_('_'))	[_val += _1]
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


