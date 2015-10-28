//#define BOOST_SPIRIT_DEBUG  ///$$$ DEFINE THIS WHEN DEBUGGING $$$///
#pragma once

#include <iostream>
#include <string>
#include <vector>

#include <boost/tuple/tuple.hpp>

namespace parsers {
	struct simple_expression {
		struct entry {
			bool is_variable;
			std::string name;
			entry() : is_variable(false) {}
			entry(bool is_variable, std::string name) : is_variable(is_variable), name(name) {}
			entry(bool is_variable, std::vector<char> name) : is_variable(is_variable), name(name.begin(), name.end()) {}
			entry(const entry &other) : is_variable(other.is_variable), name(other.name) {}
			const entry& operator= (const entry &other) {
				is_variable = other.is_variable;
				name = other.name;
				return *this;
			}
		};
		typedef std::vector<entry> result_type;
		static bool parse(const std::string &str, result_type& v);
	};
}