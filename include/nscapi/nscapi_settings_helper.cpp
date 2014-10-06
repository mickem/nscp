
#include <nscapi/nscapi_settings_helper.hpp>

namespace nscapi {
	namespace settings_helper {
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
		boost::shared_ptr<uint_key_type> uint_key(unsigned int *val) {
			boost::shared_ptr<uint_key_type> r(new uint_key_type(val, 0, false));
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

		boost::shared_ptr<typed_path_fun_value<std::string> > fun_values_path(boost::function<void (std::string,std::string)> fun) {
			boost::shared_ptr<typed_path_fun_value<std::string> > r(new typed_path_fun_value<std::string>(fun));
			return r;
		}
		boost::shared_ptr<typed_path_map<std::string> > string_map_path(std::map<std::string,std::string> *val) {
			boost::shared_ptr<typed_path_map<std::string> > r(new typed_path_map<std::string>(val));
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