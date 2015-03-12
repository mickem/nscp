#pragma once

#include <map>
#include <string>

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/nscapi_settings_object.hpp>
#include <nscapi/nscapi_protobuf_types.hpp>
#include <nscapi/nscapi_helper.hpp>

#include <nscapi/dll_defines.hpp>

namespace nscapi {
	namespace settings_filters {
		struct NSCAPI_EXPORT filter_object {

			bool debug;

			std::string syntax_top;
			std::string syntax_ok;
			std::string syntax_empty;
			std::string syntax_detail;
			std::string filter_string;
			std::string filter_ok;
			std::string filter_warn;
			std::string filter_crit;
			std::string perf_data;
			std::string perf_config;
			NSCAPI::nagiosReturn severity;
			std::string command;
			boost::optional<boost::posix_time::time_duration> max_age;
			std::string target;
			std::string target_id;
			std::string source_id;
			std::string timeout_msg;


			filter_object() : debug(false), severity(-1) {}

			std::string to_string() const {
				std::stringstream ss;
				ss << "{TODO}";
				return ss.str();
			}

			void set_severity(std::string severity_) {
				severity = nscapi::plugin_helper::translateReturn(severity_);
			}

			inline boost::posix_time::time_duration parse_time(std::string time) {
				std::string::size_type p = time.find_first_of("sSmMhHdDwW");
				if (p == std::string::npos)
					return boost::posix_time::seconds(boost::lexical_cast<long>(time));
				long value = boost::lexical_cast<long>(time.substr(0, p));
				if ( (time[p] == 's') || (time[p] == 'S') )
					return boost::posix_time::seconds(value);
				else if ( (time[p] == 'm') || (time[p] == 'M') )
					return boost::posix_time::minutes(value);
				else if ( (time[p] == 'h') || (time[p] == 'H') )
					return boost::posix_time::hours(value);
				else if ( (time[p] == 'd') || (time[p] == 'D') )
					return boost::posix_time::hours(value*24);
				else if ( (time[p] == 'w') || (time[p] == 'W') )
					return boost::posix_time::hours(value*24*7);
				return boost::posix_time::seconds(value);
			}

			void set_max_age(std::string age) {
				if (age != "none" && age != "infinite" && age != "false" && age != "off")
					max_age = parse_time(age);
			}

			void read_object(nscapi::settings_helper::path_extension &path, const bool is_default);
			void apply_parent(const filter_object &parent);
		};
	}
}

