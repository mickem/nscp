#pragma once

#include <map>
#include <string>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <settings/client/settings_client.hpp>
#include <nscapi/settings_proxy.hpp>
#include <nscapi/functions.hpp>

#include <net/net.hpp>

namespace nscapi {

	namespace settings_object_helper {
		class default_object_type {
			std::wstring path;
			std::wstring alias;
			std::wstring value;
			std::wstring parent;
		};

		template<class object_type>
		class default_object_reader {
			static void read_target(boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object) {}
			static void apply_parent(object_type &object, object_type &parent) {}
			static void post_process_object(object_type &object) {}
			static void init_default(object_type &object) {}
		};


		template<class object_type = default_object_type, class object_reader = default_object_reader<default_object_type> >
		struct target_handler : boost::noncopyable {
			typedef boost::optional<object_type> optional_object;
			typedef optional_object optarget;
			typedef object_type target;
			typedef std::map<std::wstring, object_type> object_list_type;
			typedef target_handler<object_type, object_reader> my_type;

			object_list_type object_list;
			object_list_type template_list;

			void add_missing(boost::shared_ptr<nscapi::settings_proxy> proxy, std::wstring path, std::wstring alias, std::wstring value, bool is_template = false) {
				if (has_object(alias))
					return;
				add(proxy, path, alias, value, is_template);
			}

			object_type add(boost::shared_ptr<nscapi::settings_proxy> proxy, std::wstring path, std::wstring alias, std::wstring value, bool is_template = false) {
				object_type object;
				object.alias = alias;
				object.value = value;
				object.path = path + _T("/") + alias;
				object.parent = _T("default");

				object_reader::read_target(proxy, object);

				if (!object.parent.empty() && object.parent != alias & object.parent != object.alias) {
					object_type parent;
					optional_object tmp = find_object(object.parent);
					if (!tmp) {
						parent = add(proxy, path, object.parent, _T(""));
						add_template(parent);
					} else {
						parent = *tmp;
					}
					object_reader::apply_parent(object, parent);
				}
				if (is_template)
					add_template(object);
				else
					add_object(object);
				return object;
			}

			void rebuild(boost::shared_ptr<nscapi::settings_proxy> proxy) {
				std::list<object_type> tmp;
				BOOST_FOREACH(object_list_type::value_type t, object_list) {
					tmp.push_back(t.second);
				}
				object_list.clear();
				BOOST_FOREACH(const &object_item o, tmp) {
					std::wstring::size_type pos = o.path.find_last_of(_T("/"));
					if (pos == std::wstring::npos)
						continue;
					add(proxy, o.path.substr(0, pos-1), o.path.substr(pos), o.host);
				}
			}


			optional_object find_object(std::wstring alias) {
				object_list_type::const_iterator cit = object_list.find(alias);
				if (cit != object_list.end())
					return optional_object(cit->second);
				cit = template_list.find(alias);
				if (cit != template_list.end())
					return optional_object(cit->second);
				return optional_object();
			}

			bool has_object(std::wstring alias) {
				object_list_type::const_iterator cit = object_list.find(alias);
				if (cit != object_list.end())
					return true;
				cit = template_list.find(alias);
				if (cit != template_list.end())
					return true;
				return false;
			}


			std::wstring to_wstring() {
				std::wstringstream ss;
				ss << _T("Objects: ");
				BOOST_FOREACH(object_list_type::value_type t, object_list) {
					ss << _T(", ") << t.first << _T(" = {") << t.second.to_wstring() + _T("} ");
				}
				ss << _T("Templates: ");
				BOOST_FOREACH(object_list_type::value_type t, template_list) {
					ss << _T(", ") << t.first << _T(" = {") << t.second.to_wstring() + _T("} ");
				}
				return ss.str();
			}

			void add_object(object_type object) {
				object_reader::post_process_object(object);
				object_list_type::iterator cit = template_list.find(object.alias);
				if (cit != template_list.end())
					template_list.erase(cit);
				object_list[object.alias] = object;
			}
			void add_template(object_type object) {
				object_list_type::const_iterator cit = object_list.find(object.alias);
				if (cit != object_list.end())
					return;
				object_reader::post_process_object(object);
				template_list[object.alias] = object;
			}
		};
	}


	namespace targets {
		struct target_object {

			std::wstring path;
			std::wstring alias;
			std::wstring value;
			std::wstring parent;

			net::wurl address;
			typedef std::map<std::wstring,std::wstring> options_type;
			options_type options;

			std::wstring to_wstring() const {
				std::wstringstream ss;
				ss << _T("Target: ") << alias;
				ss << _T(", address: ") << get_address();
				ss << _T(", parent: ") << parent;
				BOOST_FOREACH(options_type::value_type o, options) {
					ss << _T(", option[") << o.first << _T("]: ") << o.second;
				}
				return ss.str();
			}
			std::wstring get_address() const {
				return address.to_string();
			}
			void set_address(std::wstring value) {
				net::wurl n = net::parse(value);
				address.apply(n);
			}
			void set_host(std::wstring value) {
				address.host = value;
			}
			void set_port(int value) {
				address.host = strEx::itos(value);
			}
			bool has_option(std::wstring key) const {
				return options.find(key) != options.end();
			}
			bool has_option(std::string key) const {
				return has_option(utf8::cvt<std::wstring>(key));
			}
			void set_property_int(std::wstring key, int value) {
				if (key == _T("port")) {
					set_port(value);
				} else 
					options[key] = strEx::itos(value);
			}
			void set_property_bool(std::wstring key, bool value) {
				options[key] = value?_T("true"):_T("false");
			}
			void set_property_string(std::wstring key, std::wstring value) {
				if (key == _T("host")) {
					set_host(value);
				} else 
					options[key] = value;
			}

			nscapi::functions::destination_container to_destination_container() const {
				nscapi::functions::destination_container ret;
				if (!alias.empty())
					ret.id = utf8::cvt<std::string>(alias);
				ret.address.apply(net::wide_to_url(address));
				BOOST_FOREACH(const options_type::value_type &kvp, options) {
					ret.data[utf8::cvt<std::string>(kvp.first)] = utf8::cvt<std::string>(kvp.second);
				}
				return ret;
			}

		};
		typedef boost::optional<target_object> optional_target_object;
		typedef std::map<std::wstring,std::wstring> targets_type;

		struct target_object_reader {
			typedef target_object object_type;
			static void read_target(boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object);
			static void apply_parent(object_type &object, object_type &parent);
		};

		namespace sh = nscapi::settings_helper;
		struct dummy_custom_reader {
			typedef target_object object_type;
			static void add_custom_keys(sh::settings_registry &settings, boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object) {}
		};

		template<class custom_reader>
		struct split_object_reader {
			typedef target_object object_type;

			static void post_process_object(object_type &object) {
				custom_reader::post_process_target(object);
			}

			static void read_target(boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object) {
				object.address = net::parse(object.value, 0);
				if (object.alias == _T("default"))
					custom_reader::init_default(object);

				nscapi::settings_helper::settings_registry settings(proxy);

				object_type::options_type options;
				settings.path(object.path).add_path()
					(object.alias, nscapi::settings_helper::wstring_map_path(&options), 
					_T("TARGET DEFENITION"), _T("Target definition for: ") + object.alias)

					;

				settings.path(object.path).add_key()

					(_T("address"), sh::string_fun_key<std::wstring>(boost::bind(&object_type::set_address, &object, _1)),
					_T("TARGET ADDRESS"), _T("Target host address"))

					(_T("host"), sh::string_fun_key<std::wstring>(boost::bind(&object_type::set_host, &object, _1)),
					_T("TARGET HOST"), _T("The target server to report results to."))

					(_T("port"), sh::int_fun_key<int>(boost::bind(&object_type::set_port, &object, _1)),
					_T("TARGET PORT"), _T("The target server port"))

					(_T("alias"), nscapi::settings_helper::wstring_key(&object.alias, object.alias),
					_T("TARGET ALIAS"), _T("The alias for the target"))

					(_T("parent"), nscapi::settings_helper::wstring_key(&object.parent, _T("default")),
					_T("TARGET PARENT"), _T("The parent the target inherits from"))

					;
				custom_reader::add_custom_keys(settings, proxy, object);

				settings.register_all();
				settings.notify();

				BOOST_FOREACH(const object_type::options_type::value_type &kvp, options) {
					if (!object.has_option(kvp.first))
						object.options[kvp.first] = kvp.second;
				}

			}

			static void apply_parent(object_type &object, object_type &parent) {
				object.address.import(parent.address);
				BOOST_FOREACH(object_type::options_type::value_type i, parent.options) {
					if (object.options.find(i.first) == object.options.end())
						object.options[i.first] = i.second;
				}
			}

		};
		template<class custom_reader>
		struct handler : public nscapi::settings_object_helper::target_handler<target_object, split_object_reader<custom_reader > > {};

/*
		template<class custom_reader>
		*/
		//typedef nscapi::settings_object_helper::target_handler<target_object, split_object_reader<dummy_custom_reader > > handler;

	}
}

