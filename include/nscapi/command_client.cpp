#include <nscapi/command_client.hpp>
#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/command_proxy.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>

namespace nscapi {
	namespace command_helper {
		void register_command_helper::add(boost::shared_ptr<command_info> d) {
			owner->add(d);
		}
		void add_metadata_helper::add(std::string key, std::string value) {
			owner->set(key, value);
		}

		void command_registry::register_all() {
			if (commands.size() == 0)
				return;
			Plugin::RegistryRequestMessage request;
			nscapi::protobuf::functions::create_simple_header(request.mutable_header());
			BOOST_FOREACH(command_list::value_type v, commands) {
				Plugin::RegistryRequestMessage::Request *payload = request.add_payload();
				Plugin::RegistryRequestMessage::Request::Registration *regitem = payload->mutable_registration();
				regitem->set_plugin_id(core_->get_plugin_id());
				regitem->set_type(Plugin::Registry_ItemType_QUERY);
				regitem->set_name(v->name);
				regitem->mutable_info()->set_title(v->name);
				regitem->mutable_info()->set_description(v->description);
				BOOST_FOREACH(const std::string &alias, v->aliases) {
					regitem->add_alias(alias);
				}
			}
			std::string response_string;
			core_->registry_query(request.SerializeAsString(), response_string);
			Plugin::RegistryResponseMessage response;
			response.ParseFromString(response_string);
			for (int i = 0; i < response.payload_size(); i++) {
				if (response.payload(i).result().code() != Plugin::Common_Result_StatusCodeType_STATUS_OK) {
					errors.push_back(response.payload(i).result().message());
				}
			}
		}
	}
}