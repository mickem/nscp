
#include <settings/client/settings_client.hpp>

namespace nscapi {
	namespace settings_helper {
/*
		template<typename T>
		typed_key_entry_in_vector<std::wstring, T, typed_string_value<std::wstring> >* wstring_vector_key(T *val, typename T::key_type key, std::wstring def) {
			typed_key_entry_in_vector<std::wstring, T, typed_string_value<std::wstring> >* r = new typed_key_entry_in_vector<std::wstring, T, typed_string_value<std::wstring> >(val, key, def);
			return r;
		}
*/
		boost::shared_ptr<wstring_key_type> wstring_key(std::wstring *val, std::wstring def) {
			boost::shared_ptr<wstring_key_type> r(new wstring_key_type(val, def, true));
			return r;
		}
		boost::shared_ptr<wstring_key_type> wstring_key(std::wstring *val) {
			boost::shared_ptr<wstring_key_type> r(new wstring_key_type(val, _T(""), false));
			return r;
		}
		boost::shared_ptr<wpath_key_type> wpath_key(std::wstring *val, std::wstring def) {
			boost::shared_ptr<wpath_key_type> r(new wpath_key_type(val, def, true));
			return r;
		}
		boost::shared_ptr<wpath_key_type> wpath_key(std::wstring *val) {
			boost::shared_ptr<wpath_key_type> r(new wpath_key_type(val, _T(""), false));
			return r;
		}
		boost::shared_ptr<path_key_type> path_key(std::string *val, std::string def) {
			boost::shared_ptr<path_key_type> r(new path_key_type(val, def, true));
			return r;
		}
		boost::shared_ptr<path_key_type> path_key(std::string *val) {
			boost::shared_ptr<path_key_type> r(new path_key_type(val, "", false));
			return r;
		}
		boost::shared_ptr<real_path_key_type> path_key(boost::filesystem::path *val, std::string def) {
			boost::shared_ptr<real_path_key_type> r(new real_path_key_type(val, def, true));
			return r;
		}
		boost::shared_ptr<real_path_key_type> path_key(boost::filesystem::path *val) {
			boost::shared_ptr<real_path_key_type> r(new real_path_key_type(val, std::string(""), false));
			return r;
		}
		boost::shared_ptr<string_key_type> string_key(std::string *val, std::string def) {
			boost::shared_ptr<string_key_type> r(new string_key_type(val, def, true));
			return r;
		}
		boost::shared_ptr<string_key_type> string_key(std::string *val) {
			boost::shared_ptr<string_key_type> r(new string_key_type(val, "", false));
			return r;
		}
		boost::shared_ptr<int_key_type> int_key(int *val, int def) {
			boost::shared_ptr<int_key_type> r(new int_key_type(val, def, true));
			return r;
		}
		boost::shared_ptr<size_key_type> size_key(std::size_t *val, std::size_t def) {
			boost::shared_ptr<size_key_type> r(new size_key_type(val, def, true));
			return r;
		}
		boost::shared_ptr<int_key_type> int_key(int *val) {
			boost::shared_ptr<int_key_type> r(new int_key_type(val, 0, false));
			return r;
		}
		boost::shared_ptr<uint_key_type> uint_key(unsigned int *val, unsigned int def) {
			boost::shared_ptr<uint_key_type> r(new uint_key_type(val, def, true));
			return r;
		}
		boost::shared_ptr<bool_key_type> bool_key(bool *val, bool def) {
			boost::shared_ptr<bool_key_type> r(new bool_key_type(val, def, true));
			return r;
		}
		boost::shared_ptr<bool_key_type> bool_key(bool *val) {
			boost::shared_ptr<bool_key_type> r(new bool_key_type(val, false, false));
			return r;
		}


		boost::shared_ptr<typed_path_fun> fun_path(boost::function<void (std::wstring)> fun) {
			boost::shared_ptr<typed_path_fun> r(new typed_path_fun(fun));
			return r;
		}
		boost::shared_ptr<typed_path_fun_value<std::string> > fun_values_path(boost::function<void (std::string,std::string)> fun) {
			boost::shared_ptr<typed_path_fun_value<std::string> > r(new typed_path_fun_value<std::string>(fun));
			return r;
		}
		boost::shared_ptr<typed_path_map<std::wstring> > wstring_map_path(std::map<std::wstring,std::wstring> *val) {
			boost::shared_ptr<typed_path_map<std::wstring> > r(new typed_path_map<std::wstring>(val));
			return r;
		}
		boost::shared_ptr<typed_path_map<std::string> > string_map_path(std::map<std::string,std::string> *val) {
			boost::shared_ptr<typed_path_map<std::string> > r(new typed_path_map<std::string>(val));
			return r;
		}
		boost::shared_ptr<typed_path_list> wstring_list_path(std::list<std::wstring> *val) {
			boost::shared_ptr<typed_path_list> r(new typed_path_list(val));
			return r;
		}


		void settings_paths_easy_init::add(boost::shared_ptr<path_info> d) {
			if (is_sample)
				d->is_sample = true;
			owner->add(d);
		}
		void settings_keys_easy_init::add(boost::shared_ptr<key_info> d) {
			if (is_sample)
				d->is_sample = true;
			owner->add(d);
		}
	}
}