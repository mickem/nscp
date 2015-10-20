#include <nscapi/nscapi_settings_object.hpp>

namespace nscapi {

	namespace settings_objects {



		/*
		void object_instance_interface::add_oneliner_hint(boost::shared_ptr<nscapi::settings_proxy> proxy, const bool oneliner, const bool is_sample) {
				if (oneliner) {
					std::string::size_type pos = path.find_last_of("/");
					if (pos != std::string::npos) {
						std::string spath = path.substr(0, pos);
						std::string key = path.substr(pos+1);
						proxy->register_key(path, key, NSCAPI::key_string, alias, "Short-hand for " + alias + ". To configure this item add a section called: " + path, "", false, is_sample);
						proxy->set_string(path, key, value);
						return;
					}
				}

			}
			*/
	}
}

