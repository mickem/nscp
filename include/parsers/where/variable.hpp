#pragma once

#include <parsers/where/expression_ast.hpp>

namespace parsers {
	namespace where {		
	
		struct variable {
			variable(std::wstring name) : name(name), int_ptr_id(0), string_ptr_id(0) {}

			bool bind(value_type type, filter_handler handler);
			long long get_int(filter_handler handler) const;
			std::wstring get_string(filter_handler handler) const;
			std::wstring get_name() const { return name; }

			variable( const variable& other ) : int_ptr_id(other.int_ptr_id), string_ptr_id(other.string_ptr_id), name(other.name) {}
			const variable& operator=( const variable& other ) {
				int_ptr_id = other.int_ptr_id;
				string_ptr_id = other.string_ptr_id;
				name = other.name;
				return *this;
			}
			
		private:
			unsigned int int_ptr_id;
			unsigned int string_ptr_id;
			std::wstring name;
			variable() : int_ptr_id(0), string_ptr_id(0) {}
		};
	}
}