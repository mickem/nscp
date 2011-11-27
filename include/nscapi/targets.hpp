#pragma once

#include <map>
#include <string>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <settings/client/settings_client.hpp>
#include <nscapi/settings_proxy.hpp>

#include <net/net.hpp>

namespace nscapi {
	struct target_handler {

		struct target {
			std::wstring host;
			std::wstring alias;
			//std::wstring protocol;
			std::wstring address;
			std::wstring parent;
			typedef std::map<std::wstring,std::wstring> options_type;
			options_type options;

			std::wstring to_wstring() {
				std::wstringstream ss;
				ss << _T("Target: ") << alias;
				ss << _T(", host: ") << host;
//				ss << _T(", protocol: ") << protocol;
				ss << _T(", address: ") << address;
				ss << _T(", parent: ") << parent;
				BOOST_FOREACH(options_type::value_type o, options) {
					ss << _T(", option[") << o.first << _T("]: ") << o.second;
				}
				return ss.str();
			}

			bool has_protocol() {
				return !address.empty();
			}
			std::wstring get_protocol() {
				net::wurl url = net::parse(address);
				return url.protocol;
			}
			bool has_option(std::wstring key) {
				return options.find(key) != options.end();
			}
			bool has_option(std::string key) {
				return has_option(utf8::cvt<std::wstring>(key));
			}

		};
		typedef boost::optional<target> optarget;
		typedef std::map<std::wstring, target> target_list_type;

		target_list_type target_list;
		target_list_type template_list;
		target add(boost::shared_ptr<nscapi::settings_proxy> proxy, std::wstring path, std::wstring key, std::wstring value) {
			target t = read_target(proxy, path, key, value);
			add(t);
			return t;
		}
		void add(target t) {
			target_list[t.alias] = t;
		}
		void add_template(target t) {
			template_list[t.alias] = t;
		}
		target read_target(boost::shared_ptr<nscapi::settings_proxy> proxy, std::wstring path, std::wstring alias, std::wstring host);
		void rebuild();

		optarget find_target(std::wstring alias);
		bool has_target(std::wstring alias);
		static void apply_parent(target &t, target &p);
		std::wstring to_wstring();

	};
}

