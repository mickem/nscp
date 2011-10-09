#pragma once

#include <string>
#include <boost/variant.hpp>
#include <boost/shared_ptr.hpp>

#include <unicode_char.hpp>
#include <strEx.h>

namespace parsers {
	namespace where {


		struct filter_exception : public std::exception {
			std::string what_;
			filter_exception(std::string what) : what_(what) {}
			filter_exception(std::wstring what) : what_(utf8::cvt<std::string>(what)) {}
			~filter_exception() throw () {}
			const char* what() const throw () {
				return what_.c_str();
			}
		};


		struct binary_op;
		struct unary_op;
		struct unary_fun;
		struct string_value;
		struct int_value;
		struct list_value;
		struct expression_ast;
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

		struct filter_handler_interface {

			// Get information about "object"
			virtual bool has_variable(std::wstring) = 0;
			virtual value_type get_type(std::wstring) = 0;
			virtual bool can_convert(value_type from, value_type to) = 0;

			// Bind various functions
			virtual unsigned int bind_string(std::wstring key) = 0;
			virtual unsigned int bind_int(std::wstring key) = 0;
			virtual unsigned int bind_function(value_type to, std::wstring name, expression_ast *subject) = 0;

			virtual bool has_function(value_type to, std::wstring name, expression_ast *subject) = 0;
			//virtual filter_object get_static_object() = 0;

			// Error handling
			virtual bool has_error() = 0;
			virtual std::wstring get_error() = 0;
			virtual void error(std::wstring) = 0;

			// Execute various functions
			virtual long long execute_int(unsigned int id) = 0;
			virtual std::wstring execute_string(unsigned int id) = 0;
			virtual expression_ast execute_function(unsigned int id, value_type type, boost::shared_ptr<filter_handler_interface> handler, const expression_ast *arguments) = 0;
		};


		typedef boost::shared_ptr<filter_handler_interface> filter_handler;

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

			//template <typename Expr>
			//expression_ast(expression_ast expr_) : expr(expr_), type(type_tbd) {}
			expression_ast(const expression_ast &other) : expr(other.expr), type(other.type) {}
			expression_ast(const expression_ast *other) : expr(other->expr), type(other->type) {}
			expression_ast(const payload_type &other) : expr(other), type(type_tbd) {}
			expression_ast(list_value &other) : expr(other), type(type_tbd) {}
			expression_ast(unary_fun &other) : expr(other), type(type_tbd) {}
			expression_ast(binary_op &other) : expr(other), type(type_tbd) {}
			expression_ast(unary_op &other) : expr(other), type(type_tbd) {}
			expression_ast(string_value &other) : expr(other), type(type_tbd) {}
			expression_ast(int_value &other) : expr(other), type(type_tbd) {}
			expression_ast(variable &other) : expr(other), type(type_tbd) {}
			expression_ast(std::wstring &other) : expr(string_value(other)), type(type_tbd) {}
			/*
			expression_ast(Expr expr_) : expr(expr_), type(type_tbd) {}
			expression_ast(Expr expr_) : expr(expr_), type(type_tbd) {}
			expression_ast(Expr expr_) : expr(expr_), type(type_tbd) {}
			expression_ast(Expr expr_) : expr(expr_), type(type_tbd) {}
			expression_ast(Expr expr_) : expr(expr_), type(type_tbd) {}
			expression_ast(Expr expr_) : expr(expr_), type(type_tbd) {}
			expression_ast(Expr expr_) : expr(expr_), type(type_tbd) {}
			*/

			expression_ast& operator&=(expression_ast const& rhs);
			expression_ast& operator|=(expression_ast const& rhs);
			expression_ast& operator!=(expression_ast const& rhs);

			payload_type expr;
			value_type type;

			value_type get_type() const { return type; }
			void set_type( value_type newtype);

			std::wstring to_string() const;

			void force_type(value_type newtype);
			long long get_int(filter_handler handler) const;
			std::wstring get_string(filter_handler handler) const; 
			list_type get_list() const;

			bool can_evaluate() const;
			expression_ast evaluate(filter_handler handler) const;
			bool bind(filter_handler handler);

		};
	}
}
