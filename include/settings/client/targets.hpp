#pragma once

#include <map>
#include <string>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <settings/client/settings_client.hpp>
#include <nscapi/settings_proxy.hpp>

namespace settings_client {
	struct target_handler {

		struct target {
			std::wstring host;
			std::wstring alias;
			std::wstring protocol;
			std::wstring parent;
			typedef std::map<std::wstring,std::wstring> options_type;
			options_type options;

			std::wstring to_wstring() {
				std::wstringstream ss;
				ss << _T("Target: ") << alias;
				ss << _T(", host: ") << host;
				ss << _T(", protocol: ") << protocol;
				ss << _T(", parent: ") << parent;
				BOOST_FOREACH(options_type::value_type o, options) {
					ss << _T(", option[") << o.first << _T("]: ") << o.second;
				}
				return ss.str();
			}

		};
		typedef boost::optional<target> optarget;
		typedef std::map<std::wstring, target> target_list_type;

		target_list_type target_list;
		target_list_type template_list;
		void add(boost::shared_ptr<nscapi::settings_proxy> proxy, std::wstring path, std::wstring key, std::wstring value) {
			add(read_target(proxy, path, key, value));
		}
		void add(target t) {
			target_list[t.alias] = t;
		}
		void add_template(target t) {
			template_list[t.alias] = t;
		}
		target read_target(boost::shared_ptr<nscapi::settings_proxy> proxy, std::wstring path, std::wstring alias, std::wstring host);

		optarget find_target(std::wstring alias);
		static void apply_parent(target &t, target &p);
		std::wstring to_wstring();

	};
}

