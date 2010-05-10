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

#include <parsers/ast.hpp>

#include <simple_timer.hpp>

namespace parsers {
	namespace where {

		template<typename THandler>
		bool variable<THandler>::bind(value_type type, THandler & handler) {
			if (type_is_int(type)) {
				i_fn = handler.bind_int(name);
				if (i_fn.empty())
					handler.error(_T("Failed to bind (int) variable: ") + name);
				return !i_fn.empty();
			} else {
				s_fn = handler.bind_string(name);
				if (s_fn.empty())
					handler.error(_T("Failed to bind (string) variable: ") + name);
				return !s_fn.empty();;
			}
			handler.error(_T("Failed to bind (unknown) variable: ") + name);
			return false;
		}

		template<typename THandler>
		long long variable<THandler>::get_int(THandler & instance) const {
			if (!i_fn.empty())
				return i_fn(&instance);
			if (!s_fn.empty())
				instance.error(_T("Int variable bound to string: ") + name);
			else
				instance.error(_T("Int variable not bound: ") + name);
			return -1;
		}
		template<typename THandler>
		std::wstring variable<THandler>::get_string(THandler & instance) const {
			if (!s_fn.empty())
				return s_fn(&instance);
			if (!i_fn.empty())
				instance.error(_T("String variable bound to int: ") + name);
			else
				instance.error(_T("String variable not bound: ") + name);
			return _T("");
		}


		template<typename THandler>
		struct visitor_set_type {
			typedef bool result_type;
			value_type type;
			visitor_set_type(value_type type) : type(type) {}

			bool operator()(expression_ast<THandler> & ast) {
				ast.set_type(type);
				std::wcout << _T("WRR this should not happen for node: ") << ast.to_string() << _T("\n");
				//boost::apply_visitor(*this, ast.expr);
				return true;
			}

			bool operator()(binary_op<THandler> & expr) {
				//std::wcout << "force_bin_op" << std::endl;
				expr.left.force_type(type);
				expr.right.force_type(type);
				return true;
			}
			bool operator()(unary_op<THandler> & expr) {
				//std::wcout << "force_un_op" << std::endl;
				expr.subject.force_type(type);
				return true;
			}

			bool operator()(unary_fun<THandler> & expr) {
				//std::wcout << "force_fun" << std::endl;
				if (expr.is_transparent(type))
					expr.subject.force_type(type);
				return true;
			}

			bool operator()(list_value<THandler> & expr) {
				BOOST_FOREACH(expression_ast<THandler> &e, expr.list) {
					e.force_type(type);
					//boost::apply_visitor(*this, e.expr);
				}
				return true;
			}

			bool operator()(string_value & expr) { return false; }
			bool operator()(int_value & expr) { return false; }
			bool operator()(variable<THandler> & expr) { return false; }

			bool operator()(nil & expr) { return false; }
		};

		template<typename THandler>
		void expression_ast<THandler>::force_type(value_type newtype) {
			//std::wcout << _T("Forcing type: ") << type_to_string(type) << _T(" to ") << type_to_string(newtype) << _T(" for ") << to_string() << std::endl;
			if (type == newtype)
				return;
			if (type != newtype && type != type_tbd) {
				expression_ast subnode = expression_ast();
				subnode.expr = expr;
				subnode.set_type(type);
				expr = unary_fun<THandler>(_T("auto_convert"), subnode);
				type = newtype;
				//std::wcout << _T("Forcing type (D1): ") << type_to_string(type) << _T(" to ") << type_to_string(newtype) << _T(" for ") << to_string() << std::endl;
				return;
			}
			visitor_set_type<THandler> visitor(newtype);
			if (boost::apply_visitor(visitor, expr)) {
				set_type(newtype);
			}
			//std::wcout << _T("Forcing type (D2): ") << type_to_string(type) << _T(" to ") << type_to_string(newtype) << _T(" for ") << to_string() << std::endl;
		}

		template<typename THandler>
		void expression_ast<THandler>::set_type( value_type newtype) { 
			type = newtype; 
		}

		template<typename THandler>
		struct visitor_to_string {
			typedef void result_type;
			std::wstringstream result;

			void operator()(expression_ast<THandler> const& ast) {
				result << _T("{") << to_string(ast.get_type()) << _T("}");
				boost::apply_visitor(*this, ast.expr);
			}

			void operator()(binary_op<THandler> const& expr) {
				result << _T("op:") << operator_to_string(expr.op) << _T("(");
				operator()(expr.left);
				//boost::apply_visitor(*this, expr.left.expr);
				result << _T(", ");
				operator()(expr.right);
				//boost::apply_visitor(*this, expr.right.expr);
				result << L')';
			}
			void operator()(unary_op<THandler> const& expr) {
				result << _T("op:") << operator_to_string(expr.op) << _T("(");
				operator()(expr.subject);
				//boost::apply_visitor(*this, expr.subject.expr);
				result << L')';
			}

			void operator()(unary_fun<THandler> const& expr) {
				result << _T("fun:") << (expr.is_bound()?_T("bound:"):_T("")) << expr.name << _T("(");
				operator()(expr.subject);
				//boost::apply_visitor(*this, expr.subject.expr);
				result << L')';
			}

			void operator()(list_value<THandler> const& expr) {
				result << _T(" { ");
				BOOST_FOREACH(const expression_ast<THandler> e, expr.list) {
					operator()(e);
					//boost::apply_visitor(*this, e.expr);
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
			void operator()(variable<THandler> const& expr) {
				result << _T(":") << expr.name;
			}

			void operator()(nil const& expr) {
				result << _T("<NIL>") ;
			}
		};
		template<typename THandler>
		std::wstring expression_ast<THandler>::to_string() const {
			visitor_to_string<THandler> visitor;
			visitor(*this);
			return visitor.result.str();
		}


		template<typename THandler>
		struct visitor_get_int {
			typedef long long result_type;

			THandler &handler;
			value_type type;
			visitor_get_int(value_type  type, THandler &handler) : type(type), handler(handler) {}

			long long operator()(expression_ast<THandler> const& ast) {
				return boost::apply_visitor(*this, ast.expr);
			}
			long long operator()(binary_op<THandler> const& expr) {
				return expr.evaluate(handler).get_int(handler);
			}
			long long operator()(unary_op<THandler> const& expr) {
				return expr.evaluate(handler).get_int(handler);
			}
			long long operator()(unary_fun<THandler> const& expr) {
				return expr.evaluate(type, handler).get_int(handler);
			}
			long long operator()(list_value<THandler> const& expr) {
				handler.error(_T("List not supported yet!"));
				return -1;
			}
			long long operator()(string_value const& expr) {
				return strEx::stoi64(expr.value);
			}
			long long operator()(int_value const& expr) {
				return expr.value;
			}
			long long operator()(variable<THandler> const& expr) {
				return expr.get_int(handler);
			}
			long long operator()(nil const& expr) {
				handler.error(_T("NIL node should never happen"));
				return -1;
			}
		};
		template<typename THandler>
		long long expression_ast<THandler>::get_int(THandler &handler) const {
			visitor_get_int<THandler> visitor(type, handler);
			return boost::apply_visitor(visitor, expr);
		}


		template<typename THandler>
		struct visitor_get_string {
			typedef std::wstring result_type;

			THandler &handler;
			visitor_get_string(THandler &handler) : handler(handler) {}

			std::wstring operator()(expression_ast<THandler> const& ast) {
				return boost::apply_visitor(*this, ast.expr);
			}
			std::wstring operator()(binary_op<THandler> const& expr) {
				return expr.evaluate(handler).get_string(handler);
			}
			std::wstring operator()(unary_op<THandler> const& expr) {
				return expr.evaluate(handler).get_string(handler);
			}
			std::wstring operator()(unary_fun<THandler> const& expr) {
				return expr.evaluate(type_string, handler).get_string(handler);
			}
			std::wstring operator()(list_value<THandler> const& expr) {
				handler.error(_T("List not supported yet!"));
				return _T("");
			}
			std::wstring operator()(string_value const& expr) {
				return expr.value;
			}
			std::wstring operator()(int_value const& expr) {
				return strEx::itos(expr.value);
			}
			std::wstring operator()(variable<THandler> const& expr) {
				return expr.get_string(handler);
			}
			std::wstring operator()(nil const& expr) {
				handler.error(_T("NIL node should never happen"));
				return _T("");
			}
		};
		template<typename THandler>
		std::wstring expression_ast<THandler>::get_string(THandler &handler) const {
			visitor_get_string<THandler> visitor(handler);
			return boost::apply_visitor(visitor, expr);
		}
		
		template<typename THandler>
		typename expression_ast<THandler>::list_type expression_ast<THandler>::get_list() const {
			list_type ret;
			if (const list_value<THandler>  *list = boost::get<list_value<THandler> >(&expr)) {
				BOOST_FOREACH(expression_ast<THandler> a, list->list) {
					ret.push_back(a);
				}
			} else {
				ret.push_back(expr);
			}
			return ret;
		}

		
		template<typename THandler>
		expression_ast<THandler>& expression_ast<THandler>::operator&=(expression_ast<THandler> const& rhs) {
			expr = binary_op<THandler>(op_and, expr, rhs);
			return *this;
		}

		template<typename THandler>
		expression_ast<THandler>& expression_ast<THandler>::operator|=(expression_ast<THandler> const& rhs) {
			expr = binary_op<THandler>(op_or, expr, rhs);
			return *this;
		}
		template<typename THandler>
		expression_ast<THandler>& expression_ast<THandler>::operator!=(expression_ast<THandler> const& rhs) {
			expr = binary_op<THandler>(op_not, expr, rhs);
			return *this;
		}


		template<typename THandler>
		struct visitor_can_evaluate {
			typedef bool result_type;

			visitor_can_evaluate() {}

			bool operator()(expression_ast<THandler> const& ast) {
				return true;
			}
			bool operator()(binary_op<THandler> const& expr) {
				return true;
			}
			bool operator()(unary_op<THandler> const& expr) {
				return true;
			}
			bool operator()(unary_fun<THandler> const& expr) {
				return true;
			}
			bool operator()(list_value<THandler> const& expr) {
				return false;
			}
			bool operator()(string_value const& expr) {
				return false;
			}
			bool operator()(int_value const& expr) {
				return false;
			}
			bool operator()(variable<THandler> const& expr) {
				return false;
			}
			bool operator()(nil const& expr) {
				return false;
			}
		};
		template<typename THandler>
		bool expression_ast<THandler>::can_evaluate() const {
			visitor_can_evaluate<THandler> visitor;
			return boost::apply_visitor(visitor, expr);
		}

		template<typename THandler>
		struct visitor_evaluate {
			typedef expression_ast<THandler> result_type;

			THandler &handler;
			value_type type;
			visitor_evaluate(THandler &handler, value_type type) : handler(handler), type(type) {}

			expression_ast<THandler> operator()(expression_ast<THandler> const& ast) {
				return ast.evaluate(handler);
			}
			expression_ast<THandler> operator()(binary_op<THandler> const& op) {
				return op.evaluate(handler);
			}
			expression_ast<THandler> operator()(unary_op<THandler> const& op) {
				return op.evaluate(handler);
			}
			expression_ast<THandler> operator()(unary_fun<THandler> const& fun) {
				return fun.evaluate(type, handler);
			}
			expression_ast<THandler> operator()(list_value<THandler> const& expr) {
				return expression_ast<THandler>(int_value(FALSE));
			}
			expression_ast<THandler> operator()(string_value const& expr) {
				return expression_ast<THandler>(int_value(FALSE));
			}
			expression_ast<THandler> operator()(int_value const& expr) {
				return expression_ast<THandler>(int_value(FALSE));
			}
			expression_ast<THandler> operator()(variable<THandler> const& expr) {
				return expression_ast<THandler>(int_value(FALSE));
			}
			expression_ast<THandler> operator()(nil const& expr) {
				return expression_ast<THandler>(int_value(FALSE));
			}
		};

		template<typename THandler>
		expression_ast<THandler> expression_ast<THandler>::evaluate(THandler &handler) const {
			visitor_evaluate<THandler> visitor(handler, get_type());
			return boost::apply_visitor(visitor, expr);
			//simple_timer timer(to_string(), true);
			/*
			if (const binary_op<THandler> *op = boost::get<binary_op<THandler> >(&expr))
				return op->evaluate(handler);
			if (const unary_op<THandler> *op = boost::get<unary_op<THandler> >(&expr))
				return op->evaluate(handler);
			if (const unary_fun<THandler> *fun = boost::get<unary_fun<THandler> >(&expr))
				return fun->evaluate(get_type(), handler);
			if (const expression_ast<THandler> *ast = boost::get<expression_ast<THandler> >(&expr))
				return ast->evaluate(handler);
			return *this;
			*/
		}

		template<typename THandler>
		bool expression_ast<THandler>::bind(THandler &handler) {
			if (variable<THandler> *var = boost::get<variable<THandler> >(&expr))
				return var->bind(type, handler);
			if (unary_fun<THandler> *var = boost::get<unary_fun<THandler> >(&expr))
				return var->bind(type, handler);
			//handler.error(_T("Failed to bind object: ") + to_string());
			//return false;
			return true;
		}



		template<typename THandler>
		expression_ast<THandler> binary_op<THandler>::evaluate(THandler &handler) const {
			factory<THandler>::bin_op_type impl = factory<THandler>::get_binary_operator(op, left, right);
			if (get_return_type(op, type_invalid) == type_bool) {
				return impl->evaluate(handler, left, right);
			}
			handler.error(_T("Missing operator implementation"));
			return expression_ast<THandler>(int_value(FALSE));
		}
		template<typename THandler>
		expression_ast<THandler> unary_op<THandler>::evaluate(THandler &handler) const {
			factory<THandler>::un_op_type impl = factory<THandler>::get_unary_operator(op);
			value_type type = get_return_type(op, type_invalid);
			if (type_is_int(type)) {
				return impl->evaluate(handler, subject);
			}
			handler.error(_T("Missing operator implementation"));
			return expression_ast<THandler>(int_value(FALSE));
		}
		template<typename THandler>
		bool unary_fun<THandler>::is_bound() const {
			if (!e_fn.empty())
				return true;
			if (i_fn)
				return true;
			return false;
		}

		template<typename THandler>
		expression_ast<THandler> unary_fun<THandler>::evaluate(value_type type, THandler &handler) const {
			if (!e_fn.empty())
				return e_fn(&handler, type, subject);
			if (i_fn)
				return i_fn->evaluate(type, handler, subject);
			handler.error(_T("Missing function binding: ") + name + _T("bound: ") + strEx::itos(is_bound()));
			return expression_ast<THandler>(int_value(FALSE));
		}

		template<typename THandler>
		bool unary_fun<THandler>::bind(value_type type, THandler & handler) {
			try {
				if (handler.has_function(type, name, subject)) {
					e_fn = handler.bind_function(type, name, subject);
					if (e_fn.empty()) {
						handler.error(_T("Failed to bind function: ") + name);
						return false;
					}
					return true;
				}
				i_fn = parsers::where::factory<THandler>::get_binary_function(name, subject);
				if (!i_fn) {
					handler.error(_T("Failed to create function: ") + name);
					return false;
				}
				return true;
			} catch (...) {
				handler.error(_T("Failed to bind function: ") + name);
				return false;
			}
		}
		template<typename THandler>
		bool unary_fun<THandler>::is_transparent(value_type type) {
			// TODO make the handler be allowed to have a say here
			if (name == _T("neg"))
				return true;
			return false;
		}


	}
}


