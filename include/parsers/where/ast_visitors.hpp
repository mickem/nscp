#pragma once

namespace parsers {
	namespace where {

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
		struct visitor_get_int {
			typedef long long result_type;
			typedef typename THandler::object_type object_type;

			object_type &object;
			value_type type;
			visitor_get_int(value_type  type, object_type &object) : type(type), object(object) {}

			long long operator()(expression_ast<THandler> const& ast) {
				return boost::apply_visitor(*this, ast.expr);
			}
			long long operator()(binary_op<THandler> const& expr) {
				return expr.evaluate(type, object).get_int(object);
			}
			long long operator()(unary_op<THandler> const& expr) {
				return expr.evaluate(object).get_int(object);
			}
			long long operator()(unary_fun<THandler> const& expr) {
				return expr.evaluate(type, object).get_int(object);
			}
			long long operator()(list_value<THandler> const& expr) {
				object.error(_T("List not supported yet!"));
				return -1;
			}
			long long operator()(string_value const& expr) {
				return strEx::stoi64(expr.value);
			}
			long long operator()(int_value const& expr) {
				return expr.value;
			}
			long long operator()(variable<THandler> const& expr) {
				return expr.get_int(object);
			}
			long long operator()(nil const& expr) {
				object.error(_T("NIL node should never happen"));
				return -1;
			}
		};
		
		
		template<typename THandler>
		struct visitor_get_string {
			typedef std::wstring result_type;
			typedef typename THandler::object_type object_type;

			object_type &object;
			visitor_get_string(object_type &object) : object(object) {}

			std::wstring operator()(expression_ast<THandler> const& ast) {
				return boost::apply_visitor(*this, ast.expr);
			}
			std::wstring operator()(binary_op<THandler> const& expr) {
				return expr.evaluate(type_string, object).get_string(object);
			}
			std::wstring operator()(unary_op<THandler> const& expr) {
				return expr.evaluate(object).get_string(object);
			}
			std::wstring operator()(unary_fun<THandler> const& expr) {
				return expr.evaluate(type_string, object).get_string(object);
			}
			std::wstring operator()(list_value<THandler> const& expr) {
				object.error(_T("List not supported yet!"));
				return _T("");
			}
			std::wstring operator()(string_value const& expr) {
				return expr.value;
			}
			std::wstring operator()(int_value const& expr) {
				return strEx::itos(expr.value);
			}
			std::wstring operator()(variable<THandler> const& expr) {
				return expr.get_string(object);
			}
			std::wstring operator()(nil const& expr) {
				object.error(_T("NIL node should never happen"));
				return _T("");
			}
		};
		

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
		struct visitor_evaluate {
			typedef expression_ast<THandler> result_type;
			typedef typename THandler::object_type object_type;

			object_type &object;
			value_type type;
			visitor_evaluate(object_type &object, value_type type) : object(object), type(type) {}

			expression_ast<THandler> operator()(expression_ast<THandler> const& ast) {
				return ast.evaluate(object);
			}
			expression_ast<THandler> operator()(binary_op<THandler> const& op) {
				return op.evaluate(type, object);
			}
			expression_ast<THandler> operator()(unary_op<THandler> const& op) {
				return op.evaluate(object);
			}
			expression_ast<THandler> operator()(unary_fun<THandler> const& fun) {
				return fun.evaluate(type, object);
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
	}
}