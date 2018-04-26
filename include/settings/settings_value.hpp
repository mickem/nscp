#pragma once

#include <str/xtos.hpp>

#include <string>

namespace nscapi {
	namespace settings {
		struct settings_value {
			static std::string from_int(int i) {
				return str::xtos(i);
			}
			static int to_int(std::string v, int def = -1) {
				return str::stox(v, def);
			}
			static std::string from_bool(bool i) {
				return i ? "true" : "false";
			}
			static bool to_bool(std::string str) {
				std::transform(str.begin(), str.end(), str.begin(), ::tolower);
				return str == "true" || str == "1" || str == "yes";
			}
		};
	}
}
