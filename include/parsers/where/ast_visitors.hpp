#pragma once

namespace parsers {
	namespace where {

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
				//std::wcout << "force_bin_op" << std::endl;
				expr.left.force_type(type);
				expr.right.force_type(type);
				return true;
			}
			bool operator()(unary_op & expr) {
				//std::wcout << "force_un_op" << std::endl;
				expr.subject.force_type(type);
				return true;
			}

			bool operator()(unary_fun & expr) {
				//std::wcout << "force_fun" << std::endl;
				if (expr.is_transparent(type))
					expr.subject.force_type(type);
				return true;
			}

			bool operator()(list_value & expr) {
				BOOST_FOREACH(expression_ast &e, expr.list) {
					e.force_type(type);
					//boost::apply_visitor(*this, e.expr);
				}
				return true;
			}

			bool operator()(string_value & expr) { return false; }
			bool operator()(int_value & expr) { return false; }
			bool operator()(variable & expr) { return false; }

			bool operator()(nil & expr) { return false; }
		};

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
				result << _T("fun:") << (expr.is_bound()?_T("bound:"):_T("")) << expr.name << _T("(");
				operator()(expr.subject);
				//boost::apply_visitor(*this, expr.subject.expr);
				result << L')';
			}

			void operator()(list_value const& expr) {
				result << _T(" { ");
				BOOST_FOREACH(const expression_ast e, expr.list) {
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
			void operator()(variable const& expr) {
				result << _T(":") << expr.get_name();
			}

			void operator()(nil const& expr) {
				result << _T("<NIL>") ;
			}
		};


		struct visitor_get_int {
			typedef long long result_type;

			filter_handler handler;
			value_type type;
			visitor_get_int(filter_handler handler, value_type type) : handler(handler), type(type) {}

			long long operator()(expression_ast const& ast) {
				return boost::apply_visitor(*this, ast.expr);
			}
			long long operator()(binary_op const& expr) {
				return expr.evaluate(handler, type).get_int(handler);
			}
			long long operator()(unary_op const& expr) {
				return expr.evaluate(handler).get_int(handler);
			}
			long long operator()(unary_fun const& expr) {
				return expr.evaluate(handler, type).get_int(handler);
			}
			long long operator()(list_value const& expr) {
				handler->error(_T("List not supported yet!"));
				return -1;
			}
			long long operator()(string_value const& expr) {
				return strEx::stoi64(expr.value);
			}
			long long operator()(int_value const& expr) {
				return expr.value;
			}
			long long operator()(variable const& expr) {
				return expr.get_int(handler);
			}
			long long operator()(nil const& expr) {
				handler->error(_T("NIL node should never happen"));
				return -1;
			}
		};
		
		
		struct visitor_get_string {
			typedef std::wstring result_type;

			filter_handler handler;
			value_type type;
			visitor_get_string(filter_handler handler, value_type type) : handler(handler), type(type) {}

			std::wstring operator()(expression_ast const& ast) {
				return boost::apply_visitor(*this, ast.expr);
			}
			std::wstring operator()(binary_op const& expr) {
				return expr.evaluate(handler, type).get_string(handler);
			}
			std::wstring operator()(unary_op const& expr) {
				return expr.evaluate(handler).get_string(handler);
			}
			std::wstring operator()(unary_fun const& expr) {
				return expr.evaluate(handler, type).get_string(handler);
			}
			std::wstring operator()(list_value const& expr) {
				handler->error(_T("List not supported yet!"));
				return _T("");
			}
			std::wstring operator()(string_value const& expr) {
				return expr.value;
			}
			std::wstring operator()(int_value const& expr) {
				return strEx::itos(expr.value);
			}
			std::wstring operator()(variable const& expr) {
				return expr.get_string(handler);
			}
			std::wstring operator()(nil const& expr) {
				handler->error(_T("NIL node should never happen"));
				return _T("");
			}
		};
		

		struct visitor_can_evaluate {
			typedef bool result_type;

			visitor_can_evaluate() {}

			bool operator()(expression_ast const& ast) {
				return true;
			}
			bool operator()(binary_op const& expr) {
				return true;
			}
			bool operator()(unary_op const& expr) {
				return true;
			}
			bool operator()(unary_fun const& expr) {
				return true;
			}
			bool operator()(list_value const& expr) {
				return false;
			}
			bool operator()(string_value const& expr) {
				return false;
			}
			bool operator()(int_value const& expr) {
				return false;
			}
			bool operator()(variable const& expr) {
				return false;
			}
			bool operator()(nil const& expr) {
				return false;
			}
		};
		

		struct visitor_evaluate {
			typedef expression_ast result_type;

			filter_handler handler;
			value_type type;
			visitor_evaluate(filter_handler handler, value_type type) : handler(handler), type(type) {}

			expression_ast operator()(expression_ast const& ast) {
				return ast.evaluate(handler);
			}
			expression_ast operator()(binary_op const& op) {
				return op.evaluate(handler, type);
			}
			expression_ast operator()(unary_op const& op) {
				return op.evaluate(handler);
			}
			expression_ast operator()(unary_fun const& fun) {
				return fun.evaluate(handler, type);
			}
			expression_ast operator()(list_value const& expr) {
				return expression_ast(int_value(FALSE));
			}
			expression_ast operator()(string_value const& expr) {
				return expression_ast(int_value(FALSE));
			}
			expression_ast operator()(int_value const& expr) {
				return expression_ast(int_value(FALSE));
			}
			expression_ast operator()(variable const& expr) {
				return expression_ast(int_value(FALSE));
			}
			expression_ast operator()(nil const& expr) {
				return expression_ast(int_value(FALSE));
			}
		};
	}
}