#include <nscapi/nscapi_settings_object.hpp>

namespace nscapi {

	namespace settings_objects {

			void template_object::read_object(nscapi::settings_helper::path_extension &root_path) {
				root_path.add_key()
					("parent", nscapi::settings_helper::string_key(&parent, "default"),
					"PARENT", "The parent the target inherits from", true)

					("is template", nscapi::settings_helper::bool_key(&is_template, false),
					"IS TEMPLATE", "Declare this object as a template (this means it will not be available as a separate object)", true)

					("alias", nscapi::settings_helper::string_key(&alias),
					"ALIAS", "The alias (service name) to report to server", true)
					;
			}

			std::string template_object::to_string() const {
				std::stringstream ss;
				ss <<  "{alias: " << alias << ", path: " << path << ", value: "  << value << ", parent: "  << parent << ", is_tpl: "  << is_template << "}";
				return ss.str();
			}

			void template_object::add_oneliner_hint(boost::shared_ptr<nscapi::settings_proxy> proxy, const bool oneliner, const bool is_sample) {
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
	}
}

