#pragma once

#include <boost/variant.hpp>

namespace parsers {
	namespace where {

		template<typename THandler>
		struct binary_op;
		template<typename THandler>
		struct unary_op;
		template<typename THandler>
		struct unary_fun;
		struct string_value;
		struct int_value;

		template<typename THandler>
		struct list_value;

		template<typename THandler>
		struct variable;
		struct nil {};

		enum operators {
			op_eq, op_le, op_lt, op_gt, op_ge, op_ne, op_in, op_nin, op_or, op_and, op_inv, op_not, op_like, op_not_like, op_binand, op_binor, op_regexp
		};

		enum value_type {
			type_invalid = 99, type_tbd = 66,
			type_int = 1, type_bool = 2, 
			type_string = 10, 
			type_date = 20, 
			type_size = 30, 
			type_custom_int = 1024,
			type_custom_int_1 = 1024+1,
			type_custom_int_2 = 1024+2,
			type_custom_int_3 = 1024+3,
			type_custom_int_4 = 1024+4,
			type_custom_int_end = 1024+100,
			type_custom_string = 2048,
			type_custom_string_1 = 2048+1,
			type_custom_string_2 = 2048+2,
			type_custom_string_3 = 2048+3,
			type_custom_string_4 = 2048+4,
			type_custom_string_end = 2048+100,
			type_custom = 4096,
			type_custom_1 = 4096+1,
			type_custom_2 = 4096+2,
			type_custom_3 = 4096+3,
			type_custom_4 = 4096+4
		};

		struct varible_type_handler {
			virtual bool has_variable(std::wstring) = 0;
			virtual value_type get_type(std::wstring) = 0;
			virtual void error(std::wstring) = 0;
			virtual bool can_convert(value_type from, value_type to) = 0;
		};


		inline bool type_is_int(value_type type) {
			return type == type_int || type == type_bool || type == type_date || type == type_size || (type >= type_custom_int && type < type_custom_int_end);
		}

		inline value_type get_return_type(operators op, value_type type) {
			if (op == op_inv)
				return type;
			if (op == op_binor)
				return type;
			if (op == op_binand)
				return type;
			return type_bool;
		}
		inline std::wstring to_string(value_type type) {
			if (type == type_bool)
				return _T("bool");
			if (type == type_string)
				return _T("string");
			if (type == type_int)
				return _T("int");
			if (type == type_date)
				return _T("date");
			if (type == type_size)
				return _T("size");
			if (type == type_invalid)
				return _T("invalid");
			if (type == type_tbd)
				return _T("tbd");
			if (type >= type_custom)
				return _T("u:") + strEx::itos(type-type_custom);
			if (type >= type_custom_string)
				return _T("us:") + strEx::itos(type-type_custom_string);
			if (type >= type_custom_int)
				return _T("ui:") + strEx::itos(type-type_custom_int);
			return _T("unknown:") + strEx::itos(type);
		}
		inline std::wstring type_to_string(value_type type) {
			return to_string(type);
		}

		inline std::wstring operator_to_string(operators const& identifier) {
			if (identifier == op_and)
				return _T("and");
			if (identifier == op_or)
				return _T("or");
			if (identifier == op_eq)
				return _T("=");
			if (identifier == op_gt)
				return _T(">");
			if (identifier == op_lt)
				return _T("<");
			if (identifier == op_ge)
				return _T(">=");
			if (identifier == op_le)
				return _T("<=");
			if (identifier == op_in)
				return _T("in");
			if (identifier == op_nin)
				return _T("not in");
			if (identifier == op_binand)
				return _T("&");
			if (identifier == op_binor)
				return _T("|");
			return _T("?");
		}

		typedef boost::variant<nil,unsigned int,std::wstring> list_value_type;
		typedef std::list<list_value_type> list_type;

		struct parsing_exception {};

		struct string_value {
			string_value(std::wstring value) : value(value) {}

			std::wstring value;
		};
		struct int_value {
			int_value(long long value) : value(value) {}

			long long value;

		};	
		template<typename THandler>
		struct expression_ast {
			typedef
				boost::variant<
				nil // can't happen!
				//, identifier_type
				, boost::recursive_wrapper<expression_ast<THandler> >
				, boost::recursive_wrapper<list_value<THandler> >
				, boost::recursive_wrapper<unary_fun<THandler> >
				, boost::recursive_wrapper<binary_op<THandler> >
				, boost::recursive_wrapper<unary_op<THandler> >
 				, boost::recursive_wrapper<string_value>
 				, boost::recursive_wrapper<int_value>
 				, boost::recursive_wrapper<variable<THandler> >
				>
				payload_type;
			typedef std::list<expression_ast<typename THandler> > list_type;
			typedef typename THandler::object_type object_type;

			expression_ast() : expr(nil()), type(type_tbd) {}

			template <typename Expr>
			expression_ast(Expr const& expr_) : expr(expr_), type(type_tbd) {}

			expression_ast<THandler>& operator&=(expression_ast<THandler> const& rhs);
			expression_ast<THandler>& operator|=(expression_ast<THandler> const& rhs);
			expression_ast<THandler>& operator!=(expression_ast<THandler> const& rhs);
//			expression_ast<THandler>& operator=(expression_ast<THandler> const& rhs);

			payload_type expr;
			value_type type;

			value_type get_type() const { return type; }
			void set_type( value_type newtype);

			std::wstring to_string() const;

			void force_type(value_type newtype);
			long long get_int(object_type &object) const;
			std::wstring get_string(object_type &object) const; 
			list_type get_list() const;

			bool can_evaluate() const;
			expression_ast<THandler> evaluate(object_type &handler) const;
			bool bind(THandler &handler);

		};
	}
}
