#pragma once

#include <str/xtos.hpp>

#include <boost/optional/optional.hpp>

#include <string>

namespace nscapi {
	namespace settings {




		struct settings_value {
			boost::optional<std::string> string_value;
			boost::optional<int> int_value;
			boost::optional<bool> bool_value;


			settings_value(std::string v) : string_value(v) {}
			settings_value(int v) : int_value(v) {}
			settings_value(bool v) : bool_value(v) {}
			settings_value() {}

			settings_value(const settings_value &other) : string_value(other.string_value), int_value(other.int_value), bool_value(other.bool_value) {}
			const settings_value& operator=(const settings_value &other) {
				string_value = other.string_value;
				int_value = other.int_value;
				bool_value = other.bool_value;
				return *this;
			}

		public:
			static settings_value make_string(std::string str) {
				return settings_value(str);
			}
			static settings_value make_int(int i) {
				return settings_value(i);
			}
			static settings_value make_bool(bool v) {
				return settings_value(v);
			}
			static settings_value make_empty() {
				return settings_value();
			}

			std::string get_string() const {
				if (string_value)
					return *string_value;
				if (int_value)
					return str::xtos(*int_value);
				if (bool_value)
					return (*bool_value) ? "true" : "false";
				return "UNKNOWN";
			}
			int get_int() const {
				if (string_value)
					return -1;
				if (int_value)
					return *int_value;
				if (bool_value)
					return (*bool_value) ? 1 : 0;
				return -1;
			}
			bool get_bool() const {
				if (string_value)
					return false;
				if (int_value)
					return false;
				if (bool_value)
					return *bool_value;
				return false;
			}

			bool is_empty() const {
				return !string_value && !int_value && !bool_value;
			}

		};
	}
}
