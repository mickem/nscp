#pragma once

namespace parsers {
	namespace where {


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
		std::wstring expression_ast<THandler>::to_string() const {
			visitor_to_string<THandler> visitor;
			visitor(*this);
			return visitor.result.str();
		}


		template<typename THandler>
		long long expression_ast<THandler>::get_int(object_type &object) const {
			visitor_get_int<THandler> visitor(type, object);
			return boost::apply_visitor(visitor, expr);
		}


		template<typename THandler>
		std::wstring expression_ast<THandler>::get_string(object_type &object) const {
			visitor_get_string<THandler> visitor(object);
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
// 		template<typename THandler>
// 		expression_ast<THandler>& expression_ast<THandler>::operator=(expression_ast<THandler> const& rhs) {
// 			expr = rhs.expr;
// 			type = rhs.type;
// 			return *this;
// 		}


		template<typename THandler>
		bool expression_ast<THandler>::can_evaluate() const {
			visitor_can_evaluate<THandler> visitor;
			return boost::apply_visitor(visitor, expr);
		}

		template<typename THandler>
		expression_ast<THandler> expression_ast<THandler>::evaluate(object_type &object) const {
			visitor_evaluate<THandler> visitor(object, get_type());
			return boost::apply_visitor(visitor, expr);
		}

		template<typename THandler>
		bool expression_ast<THandler>::bind(THandler &handler) {
			if (variable<THandler> *var = boost::get<variable<THandler> >(&expr))
				return var->bind(type, handler);
			if (unary_fun<THandler> *var = boost::get<unary_fun<THandler> >(&expr))
				return var->bind(type, handler);
			return true;
		}

		template<typename THandler>
		expression_ast<THandler> binary_op<THandler>::evaluate(value_type type, object_type &object) const {
			factory<THandler>::bin_op_type impl = factory<THandler>::get_binary_operator(op, left, right);
			value_type expected_type = get_return_type(op, type);
			if (type_is_int(type)) {
				return impl->evaluate(object, left, right);
			}
			object.error(_T("Missing operator implementation"));
			return expression_ast<THandler>(int_value(FALSE));
		}
		template<typename THandler>
		expression_ast<THandler> unary_op<THandler>::evaluate(object_type &object) const {
			factory<THandler>::un_op_type impl = factory<THandler>::get_unary_operator(op);
			value_type type = get_return_type(op, type_invalid);
			if (type_is_int(type)) {
				return impl->evaluate(object, subject);
			}
			object.error(_T("Missing operator implementation"));
			return expression_ast<THandler>(int_value(FALSE));
		}
	
	}
}

