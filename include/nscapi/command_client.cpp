
#include <nscapi/command_client.hpp>

namespace nscapi {
	namespace command_helper {
		void register_command_helper::add(boost::shared_ptr<command_info> d) {
			owner->add(d);
		}
		void add_metadata_helper::add(std::string key, std::string value) {
			owner->set(key, value);
		}
	}
}