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
			op_eq, op_le, op_lt, op_gt, op_ge, op_ne, op_in, op_nin, op_or, op_and, op_inv, op_not, op_like, op_not_like, op_binand, op_binor, op_regexp, op_not_regexp
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
			typedef std::size_t index_type;

			// Get information about "object"
			virtual bool has_variable(std::string) = 0;
			virtual value_type get_type(std::string) = 0;
			virtual bool can_convert(value_type from, value_type to) = 0;

			// Bind various functions
			virtual index_type bind_string(std::string key) = 0;
			virtual index_type bind_int(std::string key) = 0;
			virtual index_type bind_function(value_type to, std::string name, expression_ast *subject) = 0;

			virtual bool has_function(value_type to, std::string name, expression_ast *subject) = 0;
			//virtual filter_object get_static_object() = 0;

			// Error handling
			virtual bool has_error() = 0;
			virtual std::string get_error() = 0;
			virtual void error(std::string) = 0;

			// Execute various functions
			virtual long long execute_int(index_type id) = 0;
			virtual std::string execute_string(index_type id) = 0;
			virtual expression_ast execute_function(index_type id, value_type type, boost::shared_ptr<filter_handler_interface> handler, const expression_ast *arguments) = 0;
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
		inline std::string to_string(value_type type) {
			if (type == type_bool)
				return "bool";
			if (type == type_string)
				return "string";
			if (type == type_int)
				return "int";
			if (type == type_date)
				return "date";
			if (type == type_size)
				return "size";
			if (type == type_invalid)
				return "invalid";
			if (type == type_tbd)
				return "tbd";
			if (type >= type_custom)
				return "u:" + strEx::s::xtos(type-type_custom);
			if (type >= type_custom_string)
				return "us:" + strEx::s::xtos(type-type_custom_string);
			if (type >= type_custom_int)
				return "ui:" + strEx::s::xtos(type-type_custom_int);
			return "unknown:" + strEx::s::xtos(type);
		}
		inline std::string type_to_string(value_type type) {
			return to_string(type);
		}

		inline std::string operator_to_string(operators const& identifier) {
			if (identifier == op_and)
				return "and";
			if (identifier == op_or)
				return "or";
			if (identifier == op_eq)
				return "=";
			if (identifier == op_gt)
				return ">";
			if (identifier == op_lt)
				return "<";
			if (identifier == op_ge)
				return ">=";
			if (identifier == op_le)
				return "<=";
			if (identifier == op_in)
				return "in";
			if (identifier == op_nin)
				return "not in";
			if (identifier == op_binand)
				return "&";
			if (identifier == op_binor)
				return "|";
			return "?";
		}

		typedef boost::variant<nil,unsigned int,std::string> list_value_type;
		typedef std::list<list_value_type> list_type;

		struct parsing_exception {};

		struct string_value {
			string_value(std::string value) : value(value) {}

			std::string value;
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
			expression_ast(std::string &other) : expr(string_value(other)), type(type_tbd) {}

			expression_ast& operator&=(expression_ast const& rhs);
			expression_ast& operator|=(expression_ast const& rhs);
			expression_ast& operator!=(expression_ast const& rhs);

			payload_type expr;
			value_type type;

			value_type get_type() const { return type; }
			void set_type( value_type newtype);

			std::string to_string() const;

			void force_type(value_type newtype);
			long long get_int(filter_handler handler) const;
			std::string get_string(filter_handler handler) const; 
			list_type get_list() const;

			bool can_evaluate() const;
			expression_ast evaluate(filter_handler handler) const;
			bool bind(filter_handler handler);

		};
	}
}
