#include <parsers/where/expression_ast.hpp>

#include <parsers/where/unary_fun.hpp>
#include <parsers/where/list_value.hpp>
#include <parsers/where/binary_op.hpp>
#include <parsers/where/unary_op.hpp>
#include <parsers/where/variable.hpp>


#include <parsers/where/ast_visitors.hpp>

namespace parsers {
	namespace where {


		void expression_ast::force_type(value_type newtype) {
			if (type == newtype)
				return;
			if (type != newtype && type != type_tbd) {
				expression_ast subnode = expression_ast();
				subnode.expr = expr;
				subnode.set_type(type);
				expr = unary_fun(_T("auto_convert"), subnode);
				type = newtype;
				//std::wcout << _T("Forcing type (D1): ") << type_to_string(type) << _T(" to ") << type_to_string(newtype) << _T(" for ") << to_string() << std::endl;
				return;
			}
			visitor_set_type visitor(newtype);
			if (boost::apply_visitor(visitor, expr)) {
				set_type(newtype);
			}
			//std::wcout << _T("Forcing type (D2): ") << type_to_string(type) << _T(" to ") << type_to_string(newtype) << _T(" for ") << to_string() << std::endl;
		}
		
		void expression_ast::set_type( value_type newtype) { 
			type = newtype; 
		}

		std::wstring expression_ast::to_string() const {
			visitor_to_string visitor;
			visitor(*this);
			return visitor.result.str();
		}


		long long expression_ast::get_int(filter_handler handler) const {
			visitor_get_int visitor(handler, type);
			return boost::apply_visitor(visitor, expr);
		}


		std::wstring expression_ast::get_string(filter_handler handler) const {
			visitor_get_string visitor(handler, type);
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

		bool expression_ast::can_evaluate() const {
			visitor_can_evaluate visitor;
			return boost::apply_visitor(visitor, expr);
		}

		expression_ast expression_ast::evaluate(filter_handler handler) const {
			visitor_evaluate visitor(handler, get_type());
			return boost::apply_visitor(visitor, expr);
		}

		bool expression_ast::bind(filter_handler handler) {
			if (variable *var = boost::get<variable>(&expr))
				return var->bind(type, handler);
			if (unary_fun *var = boost::get<unary_fun>(&expr))
				return var->bind(type, handler);
			return true;
		}
	}
}

