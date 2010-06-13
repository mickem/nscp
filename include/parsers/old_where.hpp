#pragma once

#include <list>
#include <iostream> 
#include <sstream>

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_object.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/function.hpp>

#include <strEx.h>

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

namespace parsers {
	namespace where {
		struct nary_op;
		struct binary_op;
		struct unary_op;
		struct unary_fun;
		struct string_value;
		struct int_value;
		struct list_value;
		struct variable;
		struct nil {};

		enum operators {
			op_eq, op_le, op_lt, op_gt, op_ge, op_ne, op_in, op_nin, op_or, op_and, op_inv, op_not
		};

		enum value_type {
			type_bool, type_string, type_int, type_date, type_invalid, type_tbd
		};

		value_type get_return_type(operators op, value_type type) {
			if (op == op_inv)
				return type;
			return type_bool;
		}
		std::wstring to_string(value_type type) {
			if (type == type_bool)
				return _T("bool");
			if (type == type_string)
				return _T("string");
			if (type == type_int)
				return _T("int");
			if (type == type_date)
				return _T("date");
			if (type == type_invalid)
				return _T("invalid");
			if (type == type_tbd)
				return _T("tbd");
			return _T("unknown");
		}

		struct varible_type_handler {
			virtual bool has_variable(std::wstring) = 0;
			virtual value_type get_type(std::wstring) = 0;
			virtual void error(std::wstring) = 0;
		};
		struct varible_handler : public varible_type_handler {
			typedef boost::function<std::wstring(varible_handler*)> bound_string_type;
			typedef boost::function<long long(varible_handler*)> bound_int_type;
			virtual long long get_int(std::wstring) = 0; 
			virtual std::wstring get_string(std::wstring) = 0;
			virtual bound_string_type bind_string(std::wstring key) = 0;
			virtual bound_int_type bind_int(std::wstring key) = 0;
		};

		
		std::wstring operator_to_string(operators const& identifier) {
			if (identifier == op_and)
				return _T("and");
			if (identifier == op_or)
				return _T("or");
			if (identifier == op_eq)
				return _T("=");
			return _T("?");
		}

		typedef boost::variant<nil,unsigned int,std::wstring> list_value_type;
		typedef std::list<list_value_type> list_type;

		struct parsing_exception {};

		template<class THandler>
		struct expression_ast {
			typedef
				boost::variant<
				nil // can't happen!
				//, identifier_type
				, boost::recursive_wrapper<expression_ast>
				, boost::recursive_wrapper<list_value>
				, boost::recursive_wrapper<unary_fun>
				, boost::recursive_wrapper<binary_op>
				, boost::recursive_wrapper<unary_op>
 				, boost::recursive_wrapper<string_value>
 				, boost::recursive_wrapper<int_value>
 				, boost::recursive_wrapper<variable>
				>
				payload_type;
			typedef std::list<expression_ast> list_type;

			expression_ast() : expr(nil()), type(type_tbd) {}

			template <typename Expr>
			expression_ast(Expr const& expr_) : expr(expr_), type(type_tbd) {}

			expression_ast& operator&=(expression_ast const& rhs);
			expression_ast& operator|=(expression_ast const& rhs);
			expression_ast& operator!=(expression_ast const& rhs);

			payload_type expr;
			value_type type;

			value_type get_type() const { return type; }
			void set_type( value_type newtype);

			std::wstring to_string() const;

			void force_type(value_type newtype);
			long long get_int(THandler &handler) const;
			std::wstring get_string(THandler &handler) const; 
			list_type get_list() const;

			bool can_evaluate() const;
			expression_ast evaluate(THandler &handler) const;
			bool bind(THandler &handler);

		};

		struct binary_operator_impl {
			virtual expression_ast evaluate(varible_handler &handler, const expression_ast &left, const expression_ast & right) const = 0;
		};
		struct binary_function_impl {
			virtual expression_ast evaluate(value_type type, varible_handler &handler, const expression_ast &subject) const = 0;
		};
		struct unary_operator_impl {
			virtual expression_ast evaluate(varible_handler &handler, const expression_ast &subject) const = 0;
		};

// 
		struct list_value {
			list_value() {
			}
			list_value(expression_ast const& subject) {
				list.push_back(subject);
			}
			void append(expression_ast const& subject) {
				list.push_back(subject);
			}
			list_value& operator+=(expression_ast const& subject) {
				list.push_back(subject);
				return *this;
			}

			typedef std::list<expression_ast> list_type;
			list_type list;
		};

		struct binary_op {
			binary_op(operators op, expression_ast const& left, expression_ast const& right): op(op), left(left), right(right) {}

			expression_ast evaluate(varible_handler &handler) const;

			operators op;
			expression_ast left;
			expression_ast right;
		};

		struct unary_op {
			unary_op(operators op, expression_ast const& subject): op(op), subject(subject) {}

			expression_ast evaluate(varible_handler &handler) const;

			operators op;
			expression_ast subject;
		};

		struct unary_fun {
			unary_fun(std::wstring name, expression_ast const& subject): name(name), subject(subject) {}

			expression_ast evaluate(value_type type, varible_handler &handler) const;

			std::wstring name;
			expression_ast subject;
		};

		struct string_value {
			string_value(std::wstring value) : value(value) {}

			std::wstring value;
		};
		struct int_value {
			int_value(long long value) : value(value) {}

			long long value;

		};
		struct variable {
			boost::function<long long(varible_handler*)> i_fn;
			boost::function<std::wstring(varible_handler*)> s_fn;
			variable(std::wstring name) : name(name) {}

			bool bind(value_type type, varible_handler & handler) {
				if (type == type_int || type == type_bool) {
					i_fn = handler.bind_int(name);
					return !i_fn.empty();
				} else {
					s_fn = handler.bind_string(name);
					return !s_fn.empty();;
				}
				handler.error(_T("Failed to bind varaible: ") + name);
				return false;
			}

			long long get_int(varible_handler & instance) {
				if (!i_fn.empty())
					return i_fn(&instance);
				instance.error(_T("Variable not bound (to int): ") + name);
				return -1;
			}
			std::wstring get_string(varible_handler & instance) {
				if (!s_fn.empty())
					return s_fn(&instance);
				instance.error(_T("Variable not bound (to string): ") + name);
				return _T("");
			}

			std::wstring name;
		};


		struct visitor_set_type {
			typedef bool result_type;
			value_type type;
			visitor_set_type(value_type type) : type(type) {}

			bool operator()(expression_ast & ast) {
				ast.set_type(type);
				std::wcout << _T("WRR this should not happen for node: ") << ast.to_string() << _T("\n");
				//boost::apply_visitor(*this, ast.expr);
				return true;
			}

			bool operator()(binary_op & expr) {
				expr.left.force_type(type);
				expr.right.force_type(type);
				return true;
			}
			bool operator()(unary_op & expr) {
				expr.subject.force_type(type);
				return true;
			}

			bool operator()(unary_fun & expr) {
				//expr.subject.force_type(type);
				return true;
			}

			bool operator()(list_value & expr) {
				BOOST_FOREACH(expression_ast e, expr.list) {
					e.force_type(type);
					//boost::apply_visitor(*this, e.expr);
				}
				return false;
			}

			bool operator()(string_value & expr) { return false; }
			bool operator()(int_value & expr) { return false; }
			bool operator()(variable & expr) { return false; }

			bool operator()(nil & expr) { return false; }
		};

		void expression_ast::force_type(value_type newtype) {
			if (type == newtype)
				return;
			if (type != newtype && type != type_tbd) {
				expression_ast subnode = expression_ast();
				subnode.expr = expr;
				subnode.set_type(type);
				expr = unary_fun(_T("auto_convert"), subnode);
				type = newtype;
				return;
			}
			visitor_set_type visitor(newtype);
			if (boost::apply_visitor(visitor, expr)) {
				set_type(newtype);
			}
		}

		void expression_ast::set_type( value_type newtype) { 
			type = newtype; 
		}

		struct visitor_to_string {
			typedef void result_type;
			std::wstringstream result;

			void operator()(expression_ast const& ast) {
				result << _T("{") << to_string(ast.get_type()) << _T("}");
				boost::apply_visitor(*this, ast.expr);
			}

			void operator()(binary_op const& expr) {
				result << _T("op:") << operator_to_string(expr.op) << _T("(");
				operator()(expr.left);
				//boost::apply_visitor(*this, expr.left.expr);
				result << _T(", ");
				operator()(expr.right);
				//boost::apply_visitor(*this, expr.right.expr);
				result << L')';
			}
			void operator()(unary_op const& expr) {
				result << _T("op:") << operator_to_string(expr.op) << _T("(");
				operator()(expr.subject);
				//boost::apply_visitor(*this, expr.subject.expr);
				result << L')';
			}

			void operator()(unary_fun const& expr) {
				result << _T("fun:") << expr.name << _T("(");
				operator()(expr.subject);
				//boost::apply_visitor(*this, expr.subject.expr);
				result << L')';
			}

			void operator()(list_value const& expr) {
				result << _T(" { ");
				BOOST_FOREACH(const expression_ast e, expr.list) {
					boost::apply_visitor(*this, e.expr);
					result << _T(", ");
				}
				result << _T(" } ");
			}

			void operator()(string_value const& expr) {
				result << _T("'") << expr.value << _T("'");
			}
			void operator()(int_value const& expr) {
				result << _T("#") << expr.value;
			}
			void operator()(variable const& expr) {
				result << _T(":") << expr.name;
			}

			void operator()(nil const& expr) {
				result << _T("<NIL>") ;
			}
		};
		struct visitor_get_int {
			typedef long long result_type;

			varible_handler &handler;
			visitor_get_int(varible_handler &handler) : handler(handler) {}

			long long operator()(expression_ast const& ast) {
				return boost::apply_visitor(*this, ast.expr);
			}
			long long operator()(binary_op const& expr) {
				return expr.evaluate(handler).get_int(handler);
			}
			long long operator()(unary_op const& expr) {
				return expr.evaluate(handler).get_int(handler);
			}
			long long operator()(unary_fun const& expr) {
				return expr.evaluate(type_int, handler).get_int(handler);
			}
			long long operator()(list_value const& expr) {
				handler.error(_T("List not supported yet!"));
				return -1;
			}
			long long operator()(string_value const& expr) {
				return strEx::stoi64(expr.value);
			}
			long long operator()(int_value const& expr) {
				return expr.value;
			}
			long long operator()(variable const& expr) {
				return handler.get_int(expr.name);
			}
			long long operator()(nil const& expr) {
				handler.error(_T("NIL node should never happen"));
				return -1;
			}
		};

		struct visitor_get_string {
			typedef std::wstring result_type;

			varible_handler &handler;
			visitor_get_string(varible_handler &handler) : handler(handler) {}

			std::wstring operator()(expression_ast const& ast) {
				return boost::apply_visitor(*this, ast.expr);
			}
			std::wstring operator()(binary_op const& expr) {
				return expr.evaluate(handler).get_string(handler);
			}
			std::wstring operator()(unary_op const& expr) {
				return expr.evaluate(handler).get_string(handler);
			}
			std::wstring operator()(unary_fun const& expr) {
				return expr.evaluate(type_string, handler).get_string(handler);
			}
			std::wstring operator()(list_value const& expr) {
				handler.error(_T("List not supported yet!"));
				return _T("");
			}
			std::wstring operator()(string_value const& expr) {
				return expr.value;
			}
			std::wstring operator()(int_value const& expr) {
				return strEx::itos(expr.value);
			}
			std::wstring operator()(variable const& expr) {
				return handler.get_string(expr.name);
			}
			std::wstring operator()(nil const& expr) {
				handler.error(_T("NIL node should never happen"));
				return _T("");
			}
		};
		std::wstring expression_ast::to_string() const {
			visitor_to_string visitor;
			visitor(*this);
			return visitor.result.str();
		}

		long long expression_ast::get_int(varible_handler &handler) const {
			visitor_get_int visitor(handler);
			return boost::apply_visitor(visitor, expr);
		}
		std::wstring expression_ast::get_string(varible_handler &handler) const {
			visitor_get_string visitor(handler);
			return boost::apply_visitor(visitor, expr);
		}
		expression_ast::list_type expression_ast::get_list() const {
			list_type ret;
			if (const list_value *list = boost::get<list_value>(&expr)) {
				BOOST_FOREACH(expression_ast a, list->list) {
					ret.push_back(a);
				}
			} else {
				ret.push_back(expr);
			}
			return ret;
		}

		
// 		std::wstring expression_ast::visitor_to_string::operator()(binary_op const& expr) const {
// 			return "op:" + operator_to_string(expr.op) + "(" + boost::apply_visitor(*this, expr.left.expr) + ", " + boost::apply_visitor(*this, expr.right.expr) + ")";
// 		}

		expression_ast& expression_ast::operator&=(expression_ast const& rhs) {
			expr = binary_op(op_and, expr, rhs);
			return *this;
		}

		expression_ast& expression_ast::operator|=(expression_ast const& rhs) {
			expr = binary_op(op_or, expr, rhs);
			return *this;
		}
		expression_ast& expression_ast::operator!=(expression_ast const& rhs) {
			expr = binary_op(op_not, expr, rhs);
			return *this;
		}

		struct build_expr {
			template <typename A, typename B = unused_type, typename C = unused_type>
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
			expression_ast operator()(std::wstring const & v) const {
				return expression_ast(variable(v));
			}
		};

		struct build_function {
			template <typename A, typename B>
			struct result { typedef expression_ast type; };
			expression_ast operator()(std::wstring const name, expression_ast const & var) const {
				return expression_ast(unary_fun(name, var));
			}
		};

		struct build_function_convert {
			template <typename A, typename B>
			struct result { typedef expression_ast type; };
			expression_ast operator()(wchar_t const unit, expression_ast const & vars) const {
				list_value args = list_value(vars);
				args += string_value(std::wstring(0, unit));
				return expression_ast(unary_fun(_T("convert"), args));
			}
		};


		boost::phoenix::function<build_expr> build_e;
		boost::phoenix::function<build_string> build_is;
		boost::phoenix::function<build_int> build_ii;
		boost::phoenix::function<build_variable> build_iv;
		boost::phoenix::function<build_function> build_if;
		boost::phoenix::function<build_function_convert> build_ic;


		///////////////////////////////////////////////////////////////////////////
		//  Our calculator grammar
		///////////////////////////////////////////////////////////////////////////
		template <typename Iterator>
		struct where_grammar : qi::grammar<Iterator, expression_ast(), ascii::space_type>
		{
			where_grammar() : where_grammar::base_type(expression)
			{
				using qi::_val;
				using qi::uint_;
				using qi::_1;
				using qi::_2;
				using qi::_3;

				expression	
						= and_expr											[_val = _1]
							>> *("OR" >> and_expr)							[_val |= _1]
						;
				and_expr
						= not_expr 											[_val = _1]
							>> *("AND" >> cond_expr)						[_val &= _1]
						;
				not_expr
						= cond_expr 										[_val = _1]
							>> *("NOT" >> cond_expr)						[_val != _1]
						;

				cond_expr	
						= (identifier >> op >> identifier)					[_val = build_e(_1, _2, _3) ]
						| (identifier >> "NOT IN" 
							>> '(' >> value_list >> ')')					[_val = build_e(_1, op_nin, _2) ]
						| (identifier >> "IN" >> '(' >> value_list >> ')')	[_val = build_e(_1, op_in, _2) ]
						| ('(' >> expression >> ')')						[_val = _1 ]
						;

				identifier 
						= (variable_name >> '(' >> list_expr >> ')')		[_val = build_if(_1, _2)]
						| variable_name										[_val = build_iv(_1)]
						| string_literal									[_val = build_is(_1)]
						| qi::lexeme[
							(uint_ >> ascii::alpha)							[_val = build_ic(_2, build_ii(_1))]
							]
						| '-' >> qi::lexeme[
							(uint_ >> ascii::alpha)							[_val = build_if(std::wstring(_T("neg")), build_ic(_2, build_ii(_1)))]
							]
						| number											[_val = build_ii(_1)]
						| '-' >> number										[_val = build_if(std::wstring(_T("neg")), build_ii(_1))]
						;

				list_expr
						= value_list										[_val = _1 ]
						;

				value_list
 						= string_literal									[_val = build_is(_1) ]
 							>> *( ',' >> string_literal )					[_val += build_is(_1) ]
 						|	number											[_val = build_ii(_1) ]
 							>> *( ',' >> number ) 							[_val += build_ii(_1) ]
 						;

				op 		= qi::lit("<=")										[_val = op_le]
						| qi::lit("<")										[_val = op_lt]
						| qi::lit("=")										[_val = op_eq]
						| qi::lit("!=")										[_val = op_ne]
						| qi::lit(">=")										[_val = op_ge]
						| qi::lit(">")										[_val = op_gt]
						| qi::lit("le")										[_val = op_le]
						| qi::lit("lt")										[_val = op_le]
						| qi::lit("eq")										[_val = op_eq]
						| qi::lit("ne")										[_val = op_ne]
						| qi::lit("ge")										[_val = op_ge]
						| qi::lit("gt")										[_val = op_gt]
						;

				number
						= uint_												[_val = _1]
						;
				variable_name
						= qi::lexeme[+(ascii::alpha)						[_val += _1]]
						;
				string_literal	
						= qi::lexeme[ '\'' 
								>>  +( ascii::char_ - '\'' )				[_val += _1] 
								>> '\''] 
						;
			}

			qi::rule<Iterator, expression_ast(), ascii::space_type>  expression, and_expr, not_expr, cond_expr, identifier, list_expr;
			qi::rule<Iterator, std::wstring(), ascii::space_type> string_literal, variable_name;
			qi::rule<Iterator, unsigned int(), ascii::space_type> number;
			qi::rule<Iterator, operators(), ascii::space_type> op;
			qi::rule<Iterator, list_value(), ascii::space_type> value_list;
		};



		namespace operator_impl {

			struct simple_bool_binary_operator_impl : public binary_operator_impl {
				expression_ast evaluate(varible_handler &handler, const expression_ast &left, const expression_ast & right) const {
					if (left.get_type() != right.get_type()) {
						handler.error(_T("Invalid types (not same) for binary operator"));
						return expression_ast(int_value(FALSE));
					}
					value_type type = left.get_type();
					if (type == type_int || type == type_bool)
						return eval_int(type, handler, left, right)?expression_ast(int_value(TRUE)):expression_ast(int_value(FALSE));
					if (type == type_string)
						return eval_string(type, handler, left, right)?expression_ast(int_value(TRUE)):expression_ast(int_value(FALSE));
					handler.error(_T("*** MISSING OPERATION IMPL ***"));
					return expression_ast(int_value(FALSE));
				}
				virtual bool eval_int(value_type type, varible_handler &handler, const expression_ast &left, const expression_ast & right) const = 0;
				virtual bool eval_string(value_type type, varible_handler &handler, const expression_ast &left, const expression_ast & right) const = 0;
			};

			struct operator_and : public simple_bool_binary_operator_impl {
				bool eval_int(value_type type, varible_handler &handler, const expression_ast &left, const expression_ast & right) const {
					return left.get_int(handler) && right.get_int(handler);
				}
				bool eval_string(value_type type, varible_handler &handler, const expression_ast &left, const expression_ast & right) const {
					handler.error(_T("*** MISSING OPERATION IMPL ***"));
					// TODO convert strings
					return false;
				};
			};
			struct operator_or : public simple_bool_binary_operator_impl {
				bool eval_int(value_type type, varible_handler &handler, const expression_ast &left, const expression_ast & right) const {
					return left.get_int(handler) || right.get_int(handler);
				}
				bool eval_string(value_type type, varible_handler &handler, const expression_ast &left, const expression_ast & right) const {
					handler.error(_T("*** MISSING OPERATION IMPL ***"));
					// TODO convert strings
					return false;
				};
			};
			struct operator_eq : public simple_bool_binary_operator_impl {
				bool eval_int(value_type type, varible_handler &handler, const expression_ast &left, const expression_ast & right) const {
					return left.get_int(handler) == right.get_int(handler);
				}
				bool eval_string(value_type type, varible_handler &handler, const expression_ast &left, const expression_ast & right) const { 
					return left.get_string(handler) == right.get_string(handler);
				};
			};
			struct operator_ne : public simple_bool_binary_operator_impl {
				bool eval_int(value_type type, varible_handler &handler, const expression_ast &left, const expression_ast & right) const {
					return left.get_int(handler) != right.get_int(handler);
				}
				bool eval_string(value_type type, varible_handler &handler, const expression_ast &left, const expression_ast & right) const { 
					return left.get_string(handler) != right.get_string(handler);
				};
			};
			struct operator_gt : public simple_bool_binary_operator_impl {
				bool eval_int(value_type type, varible_handler &handler, const expression_ast &left, const expression_ast & right) const {
					return left.get_int(handler) > right.get_int(handler);
				}
				bool eval_string(value_type type, varible_handler &handler, const expression_ast &left, const expression_ast & right) const { 
					return left.get_string(handler) > right.get_string(handler);
				};
			};
			struct operator_lt : public simple_bool_binary_operator_impl {
				bool eval_int(value_type type, varible_handler &handler, const expression_ast &left, const expression_ast & right) const {
					return left.get_int(handler) < right.get_int(handler);
				}
				bool eval_string(value_type type, varible_handler &handler, const expression_ast &left, const expression_ast & right) const { 
					return left.get_string(handler) < right.get_string(handler);
				};
			};
			struct operator_le : public simple_bool_binary_operator_impl {
				bool eval_int(value_type type, varible_handler &handler, const expression_ast &left, const expression_ast & right) const {
					return left.get_int(handler) <= right.get_int(handler);
				}
				bool eval_string(value_type type, varible_handler &handler, const expression_ast &left, const expression_ast & right) const { 
					return left.get_string(handler) <= right.get_string(handler);
				};
			};
			struct operator_ge : public simple_bool_binary_operator_impl {
				bool eval_int(value_type type, varible_handler &handler, const expression_ast &left, const expression_ast & right) const {
					return left.get_int(handler) >= right.get_int(handler);
				}
				bool eval_string(value_type type, varible_handler &handler, const expression_ast &left, const expression_ast & right) const { 
					return left.get_string(handler) >= right.get_string(handler);
				};
			};
			struct operator_false : public binary_operator_impl, unary_operator_impl, binary_function_impl {
				expression_ast evaluate(varible_handler &handler, const expression_ast &left, const expression_ast & right) const {
					handler.error(_T("*** MISSING OPERATION IMPL ***"));
					return expression_ast(int_value(FALSE));
				}
				expression_ast evaluate(varible_handler &handler, const expression_ast &subject) const {
					handler.error(_T("*** MISSING OPERATION IMPL ***"));
					return expression_ast(int_value(FALSE));
				}
				expression_ast evaluate(parsers::where::value_type type,varible_handler &handler, const expression_ast &subject) const {
					handler.error(_T("*** MISSING OPERATION IMPL ***"));
					return expression_ast(int_value(FALSE));
				}
			};
			struct function_convert : public binary_function_impl {
				expression_ast::list_type list;
				bool single_item;
				function_convert(const expression_ast &subject) : list(subject.get_list()), single_item(list.size()==1) {}
				expression_ast evaluate(value_type type, varible_handler &handler, const expression_ast &subject) const {
					if (single_item) {
						if (type == type_int || type == type_bool) {
							return expression_ast(int_value(list.front().get_int(handler)));
						}
						if (type == type_string) {
							return expression_ast(string_value(list.front().get_string(handler)));
						}
						handler.error(_T("Failed to handle type: ") + to_string(type));
						return expression_ast(int_value(FALSE));
					} else {
						std::wcout << _T("----------------------------------------------\n");
						std::wcout << list.size() << _T("\n");
						std::wcout << subject.to_string() << _T("\n");
						std::wcout << _T("----------------------------------------------\n");
						handler.error(_T("*** MISSING OPERATION IMPL ***"));
						return expression_ast(int_value(FALSE));
					}
				}
			};


			struct simple_bool_unary_operator_impl : public unary_operator_impl {
				expression_ast evaluate(varible_handler &handler, const expression_ast &subject) const {
					value_type type = subject.get_type();
					if (type == type_int || type == type_bool)
						return eval_int(type, handler, subject)?expression_ast(int_value(TRUE)):expression_ast(int_value(FALSE));
					if (type == type_string)
						return eval_string(type, handler, subject)?expression_ast(int_value(TRUE)):expression_ast(int_value(FALSE));
					handler.error(_T("*** MISSING OPERATION IMPL ***"));
					return expression_ast(int_value(FALSE));
				}
				virtual bool eval_int(value_type type, varible_handler &handler, const expression_ast &subject) const = 0;
				virtual bool eval_string(value_type type, varible_handler &handler, const expression_ast &subject) const = 0;
			};

			struct operator_not : public unary_operator_impl {
				expression_ast evaluate(varible_handler &handler, const expression_ast &subject) const {
					value_type type = subject.get_type();
					if (type == type_bool)
						return subject.get_int(handler)?expression_ast(int_value(TRUE)):expression_ast(int_value(FALSE));
					if (type == type_int)
						return  expression_ast(int_value(-subject.get_int(handler)));
					handler.error(_T("*** MISSING OPERATION IMPL ***"));
					return expression_ast(int_value(FALSE));
				}
			};

		}
		struct factory {
			static boost::shared_ptr<binary_operator_impl> get_binary_operator(operators op) {
				// op_in, op_nin
				if (op == op_eq)
					return boost::shared_ptr<binary_operator_impl>(new operator_impl::operator_eq());
				if (op == op_gt)
					return boost::shared_ptr<binary_operator_impl>(new operator_impl::operator_gt());
				if (op == op_lt)
					return boost::shared_ptr<binary_operator_impl>(new operator_impl::operator_lt());
				if (op == op_le)
					return boost::shared_ptr<binary_operator_impl>(new operator_impl::operator_le());
				if (op == op_ge)
					return boost::shared_ptr<binary_operator_impl>(new operator_impl::operator_ge());
				if (op == op_ne)
					return boost::shared_ptr<binary_operator_impl>(new operator_impl::operator_ne());

				if (op == op_and)
					return boost::shared_ptr<binary_operator_impl>(new operator_impl::operator_and());
				if (op == op_or)
					return boost::shared_ptr<binary_operator_impl>(new operator_impl::operator_or());
				std::cout << "======== UNHANDLED OPERATOR\n";
				return boost::shared_ptr<binary_operator_impl>(new operator_impl::operator_false());
			}

			static boost::shared_ptr<binary_function_impl> get_binary_function(std::wstring name, const expression_ast &subject) {
				if (name == _T("convert"))
					return boost::shared_ptr<binary_function_impl>(new operator_impl::function_convert(subject));
				if (name == _T("auto_convert"))
					return boost::shared_ptr<binary_function_impl>(new operator_impl::function_convert(subject));
				std::wcout << _T("======== UNDEFINED FUNCTION: ") << name << std::endl;
				return boost::shared_ptr<binary_function_impl>(new operator_impl::operator_false());
			}

			static boost::shared_ptr<unary_operator_impl> get_unary_operator(operators op) {
				// op_inv, op_not
				if (op == op_not)
					return boost::shared_ptr<unary_operator_impl>(new operator_impl::operator_not());
				std::cout << "======== UNHANDLED OPERATOR\n";
				return boost::shared_ptr<unary_operator_impl>(new operator_impl::operator_false());
			}

		};

		bool expression_ast::can_evaluate() const {
			if (boost::get<binary_op>(&expr))
				return true;
			if (boost::get<unary_op>(&expr))
				return true;
			if (boost::get<unary_fun>(&expr))
				return true;
			if (boost::get<expression_ast>(&expr))
				return true;
			return false;
		}
		expression_ast expression_ast::evaluate(varible_handler &handler) const {
			if (const binary_op *op = boost::get<binary_op>(&expr))
				return op->evaluate(handler);
			if (const unary_op *op = boost::get<unary_op>(&expr))
				return op->evaluate(handler);
			if (const unary_fun *fun = boost::get<unary_fun>(&expr))
				return fun->evaluate(get_type(), handler);
			if (const expression_ast *ast = boost::get<expression_ast>(&expr))
				return ast->evaluate(handler);
			return *this;
		}

		bool expression_ast::bind(varible_handler &handler) {
			if (variable *var = boost::get<variable>(&expr))
				return var->bind(type, handler);
			handler.error(_T("Failed to bind object: ") + to_string());
			return false;
		}



		expression_ast binary_op::evaluate(varible_handler &handler) const {
			boost::shared_ptr<binary_operator_impl> impl = factory::get_binary_operator(op);
			if (get_return_type(op, type_invalid) == type_bool) {
				return impl->evaluate(handler, right, left);
			}
			handler.error(_T("Missing operator implementation"));
			return expression_ast(int_value(FALSE));
		}
		expression_ast unary_op::evaluate(varible_handler &handler) const {
			boost::shared_ptr<unary_operator_impl> impl = factory::get_unary_operator(op);
			value_type type = get_return_type(op, type_invalid);
			if (type == type_bool || type == type_int) {
				return impl->evaluate(handler, subject);
			}
			handler.error(_T("Missing operator implementation"));
			return expression_ast(int_value(FALSE));
		}
		expression_ast unary_fun::evaluate(value_type type, varible_handler &handler) const {
			boost::shared_ptr<binary_function_impl> impl = factory::get_binary_function(name, subject);
			return impl->evaluate(type, handler, subject);
		}

		///////////////////////////////////////////////////////////////////////////
		//  Walk the tree
		///////////////////////////////////////////////////////////////////////////
		struct ast_type_inference {
			typedef value_type result_type;

			varible_type_handler & handler;
			ast_type_inference(varible_type_handler & handler) : handler(handler) {}

			value_type operator()(expression_ast & ast) {
				value_type type = ast.get_type();
				if (type != type_tbd)
					return type;
				type = boost::apply_visitor(*this, ast.expr);
				ast.set_type(type);
				return type;
			}
			bool can_convert(value_type src, value_type dst) {
				if (src == type_invalid || dst == type_invalid)
					return false;
				if (src == type_tbd)
					return false;
				if (dst == type_tbd)
					return true;
				if (src == type_string && dst == type_int)
					return true;
			}
			value_type infer_binary_type(expression_ast & left, expression_ast & right) {
				value_type rt = operator()(right);
				value_type lt = operator()(left);
				if (lt == rt)
					return lt;
				if (rt == type_invalid || lt == type_invalid)
					return type_invalid;
				if (rt == type_tbd && lt == type_tbd)
					return type_tbd;
				if (can_convert(lt, rt)) {
					right.force_type(lt);
					return lt;
				}
				if (can_convert(rt, lt)) {
					left.force_type(rt);
					return rt;
				}
				handler.error(_T("Invalid type detected for nodes: ") + left.to_string() + _T(" and " )+ right.to_string());
				return type_invalid;
			}

			value_type operator()(binary_op & expr) {
				value_type type = infer_binary_type(expr.left, expr.right);
				if (type == type_invalid)
					return type;
				return get_return_type(expr.op, type);
			}
			value_type operator()(unary_op & expr) {
				value_type type = operator()(expr.subject);
				return get_return_type(expr.op, type);
			}

			value_type operator()(unary_fun & expr) {
				return type_tbd;
			}

			value_type operator()(list_value & expr) {
				return type_tbd;
			}

			value_type operator()(string_value & expr) {
				return type_string;
			}
			value_type operator()(int_value & expr) {
				return type_int;
			}
			value_type operator()(variable & expr) {
				if (!handler.has_variable(expr.name)) {
					handler.error(_T("Variable not found: ") + expr.name);
					return type_invalid;
				}
				return handler.get_type(expr.name);
			}

			value_type operator()(nil & expr) {
				handler.error(_T("NULL node encountered"));
				return type_invalid;
			}
		};
		///////////////////////////////////////////////////////////////////////////
		//  Walk the tree
		///////////////////////////////////////////////////////////////////////////
		struct ast_static_eval {
			typedef bool result_type;
			typedef std::list<std::wstring> error_type;

			varible_handler & handler;
			ast_static_eval(varible_handler & handler) : handler(handler) {}

			bool operator()(expression_ast & ast) {
				bool result = boost::apply_visitor(*this, ast.expr);
				if (result) {
					if (ast.can_evaluate()) {
						expression_ast nexpr = ast.evaluate(handler);
						ast.expr = nexpr.expr;
					}
				}
				return result;
			}
			bool operator()(binary_op & expr) {
				bool r1 = operator()(expr.left);
				bool r2 = operator()(expr.right);
				return r1 && r2;
			}
			bool operator()(unary_op & expr) {
				return operator()(expr.subject);
			}

			bool operator()(unary_fun & expr) {
				return false;
			}

			bool operator()(list_value & expr) {
				// TODO: this is incorrect!
				return true;
			}

			bool operator()(string_value & expr) {
				return true;
			}
			bool operator()(int_value & expr) {
				return true;
			}
			bool operator()(variable & expr) {
				return false;
			}

			bool operator()(nil & expr) {
				return false;
			}
		};
		///////////////////////////////////////////////////////////////////////////
		//  Walk the tree
		///////////////////////////////////////////////////////////////////////////
		struct ast_bind {
			typedef bool result_type;
			typedef std::list<std::wstring> error_type;

			varible_handler & handler;
			ast_bind(varible_handler & handler) : handler(handler) {}

			bool operator()(expression_ast & ast) {
				return boost::apply_visitor(*this, ast.expr);
			}
			bool operator()(binary_op & expr) {
				bool r1 = operator()(expr.left);
				bool r2 = operator()(expr.right);
				return r1 && r2;
			}
			bool operator()(unary_op & expr) {
				return operator()(expr.subject);
			}

			bool operator()(unary_fun & expr) {
				return false;
			}

			bool operator()(list_value & expr) {
				// TODO: this is incorrect!
				return true;
			}

			bool operator()(string_value & expr) {
				return true;
			}
			bool operator()(int_value & expr) {
				return true;
			}
			bool operator()(variable & expr) {
				return false;
			}

			bool operator()(nil & expr) {
				return false;
			}
		};

		struct parser {
			expression_ast resulting_tree;
			std::wstring rest;
			bool parse(std::wstring expr) {
				typedef std::wstring::const_iterator iterator_type;
				typedef where_grammar<iterator_type> where_grammar;

				where_grammar calc; // Our grammar

				typedef std::wstring::const_iterator iterator_type;

				std::wstring::const_iterator iter = expr.begin();
				std::wstring::const_iterator end = expr.end();
				if (phrase_parse(iter, end, calc, ascii::space, resulting_tree))
					return true;
				rest = std::wstring(iter, end);
				return false;
			}

			bool derive_types(varible_type_handler & handler) {
				try {
					ast_type_inference resolver(handler);
					resolver(resulting_tree);
					return true;
				} catch (...) {
					handler.error(_T("Unhandled exception resolving types: ") + result_as_tree());
					return false;
				}
			}

			bool static_eval(varible_handler & handler) {
				try {
					ast_static_eval evaluator(handler);
					evaluator(resulting_tree);
					return true;
				} catch (...) {
					handler.error(_T("Unhandled exception static eval: ") + result_as_tree());
					return false;
				}
			}
			bool bind(varible_handler & handler) {
				try {
					ast_bind binder(handler);
					binder(resulting_tree);
					return true;
				} catch (...) {
					handler.error(_T("Unhandled exception static eval: ") + result_as_tree());
					return false;
				}
			}

			bool evaluate(varible_handler & handler) {
				try {
					expression_ast ast = resulting_tree.evaluate(handler);
					return ast.get_int(handler);
				} catch (...) {
					handler.error(_T("Unhandled exception static eval: ") + result_as_tree());
					return false;
				}
			}

			std::wstring result_as_tree() const {
				return resulting_tree.to_string();
			}

		};
	}
}


