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
	}
}

