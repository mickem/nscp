
#include <settings/client/settings_client.hpp>

namespace nscapi {
	namespace settings_helper {


		wstring_key_type* wstring_key(std::wstring *val, std::wstring def) {
			wstring_key_type* r = new wstring_key_type(val, def);
			return r;
		}
		wpath_key_type* wpath_key(std::wstring *val, std::wstring def) {
			wpath_key_type* r = new wpath_key_type(val, def);
			return r;
		}
		string_key_type* string_key(std::string *val, std::string def) {
			string_key_type* r = new string_key_type(val, def);
			return r;
		}
		int_key_type* int_key(int *val, int def) {
			int_key_type* r = new int_key_type(val, def);
			return r;
		}
		uint_key_type* uint_key(unsigned int *val, unsigned int def) {
			uint_key_type* r = new uint_key_type(val, def);
			return r;
		}
		bool_key_type* bool_key(bool *val, bool def) {
			bool_key_type* r = new bool_key_type(val, def);
			return r;
		}


		typed_path_fun* fun_path(boost::function<void (std::wstring)> fun) {
			typed_path_fun* r = new typed_path_fun(fun);
			return r;
		}
		typed_path_fun_value* fun_values_path(boost::function<void (std::wstring,std::wstring)> fun) {
			typed_path_fun_value* r = new typed_path_fun_value(fun);
			return r;
		}
		typed_path_map<>* wstring_map_path(std::map<std::wstring,std::wstring> *val) {
			typed_path_map<>* r = new typed_path_map<>(val);
			return r;
		}
		typed_path_list* wstring_list_path(std::list<std::wstring> *val) {
			typed_path_list* r = new typed_path_list(val);
			return r;
		}


		void settings_paths_easy_init::add(boost::shared_ptr<path_info> d) {
			owner->add(d);
		}
		void settings_keys_easy_init::add(boost::shared_ptr<key_info> d) {
			owner->add(d);
		}
	}
}