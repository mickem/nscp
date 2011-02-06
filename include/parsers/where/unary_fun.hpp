#pragma once

#include <parsers/where/expression_ast.hpp>
#include <parsers/where/operators_impl.hpp>

namespace parsers {
	namespace where {

		template<typename THandler>
		struct unary_fun {
			typedef typename THandler::object_type object_type;
			boost::function<expression_ast<THandler>(object_type*,value_type,expression_ast<THandler> const&)> e_fn;
			boost::shared_ptr<binary_function_impl<typename THandler> > i_fn;

			unary_fun(std::wstring name, expression_ast<THandler> const& subject): name(name), subject(subject) {}

			expression_ast<THandler> evaluate(value_type type, object_type &handler) const;

			bool bind(value_type type, THandler & handler);
			std::wstring name;
			expression_ast<THandler> subject;
			bool is_transparent(value_type type);
			bool is_bound() const;
		};
		

		template<typename THandler>
		expression_ast<THandler> unary_fun<THandler>::evaluate(value_type type, object_type &handler) const {
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


		template<typename THandler>
		bool unary_fun<THandler>::is_bound() const {
			if (!e_fn.empty())
				return true;
			if (i_fn)
				return true;
			return false;
		}		
	}
}
