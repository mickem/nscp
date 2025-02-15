#pragma once

#include <str/xtos.hpp>

#include <string>

#include <boost/algorithm/string/case_conv.hpp>

namespace nscapi {
	namespace settings {
		struct settings_value {
			static std::string from_int(const int i) {
				return str::xtos(i);
			}
			static int to_int(const std::string v, const int def = -1) {
				return str::stox(v, def);
			}
			static std::string from_bool(const bool i) {
				return i ? "true" : "false";
			}
			static bool to_bool(const std::string str) {
				std::string tmp = boost::to_lower_copy(str);
				return tmp == "true" || tmp == "1" || tmp == "yes";
			}
		};
	}
}
