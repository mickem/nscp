#pragma once

namespace parsers {
	namespace where {

		struct visitor_set_type {
			typedef bool result_type;
			value_type type;
			visitor_set_type(value_type type) : type(type) {}

			bool operator()(expression_ast & ast) {
				ast.set_type(type);
				std::cout << "WRR this should not happen for node: " << ast.to_string() << "\n";
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
			std::stringstream result;

			void operator()(expression_ast const& ast) {
				result << "{" << to_string(ast.get_type()) << "}";
				boost::apply_visitor(*this, ast.expr);
			}

			void operator()(binary_op const& expr) {
				result << "op:" << operator_to_string(expr.op) << "(";
				operator()(expr.left);
				result << ", ";
				operator()(expr.right);
				result << ")";
			}
			void operator()(unary_op const& expr) {
				result << "op:" << operator_to_string(expr.op) << "(";
				operator()(expr.subject);
				result << ")";
			}

			void operator()(unary_fun const& expr) {
				result << "fun:" << (expr.is_bound()?"bound:":"") << expr.name << "(";
				operator()(expr.subject);
				result << ")";
			}

			void operator()(list_value const& expr) {
				result << " { ";
				BOOST_FOREACH(const expression_ast e, expr.list) {
					operator()(e);
					result << ", ";
				}
				result << " } ";
			}

			void operator()(string_value const& expr) {
				result << "'" << expr.value << "'";
			}
			void operator()(int_value const& expr) {
				result << "#" << expr.value;
			}
			void operator()(variable const& expr) {
				result << ":" << expr.get_name();
			}

			void operator()(nil const& expr) {
				result << "<NIL>";
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
				handler->error("List not supported yet!");
				return -1;
			}
			long long operator()(string_value const& expr) {
				return strEx::s::stox<long long>(expr.value);
			}
			long long operator()(int_value const& expr) {
				return expr.value;
			}
			long long operator()(variable const& expr) {
				return expr.get_int(handler);
			}
			long long operator()(nil const& expr) {
				handler->error("NIL node should never happen");
				return -1;
			}
		};
		
		
		struct visitor_get_string {
			typedef std::string result_type;

			filter_handler handler;
			value_type type;
			visitor_get_string(filter_handler handler, value_type type) : handler(handler), type(type) {}

			std::string operator()(expression_ast const& ast) {
				return boost::apply_visitor(*this, ast.expr);
			}
			std::string operator()(binary_op const& expr) {
				return expr.evaluate(handler, type).get_string(handler);
			}
			std::string operator()(unary_op const& expr) {
				return expr.evaluate(handler).get_string(handler);
			}
			std::string operator()(unary_fun const& expr) {
				return expr.evaluate(handler, type).get_string(handler);
			}
			std::string operator()(list_value const& expr) {
				handler->error("List not supported yet!");
				return "";
			}
			std::string operator()(string_value const& expr) {
				return expr.value;
			}
			std::string operator()(int_value const& expr) {
				return strEx::s::xtos(expr.value);
			}
			std::string operator()(variable const& expr) {
				return expr.get_string(handler);
			}
			std::string operator()(nil const& expr) {
				handler->error("NIL node should never happen");
				return "";
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