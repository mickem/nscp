#define BOOST_SPIRIT_DEBUG  ///$$$ DEFINE THIS WHEN DEBUGGING $$$///
#pragma once

#include <iostream>
#include <string>
#include <vector>

#include <boost/tuple/tuple.hpp>

namespace parsers {
	struct perfconfig {
		struct perf_option {
			std::string key;
			std::string value;
		};
		struct perf_rule {
			std::string name;
			std::vector<perf_option> options;
		};
		typedef std::vector<perf_rule> result_type;
		bool parse(const std::string &str, result_type& v);
	};
}