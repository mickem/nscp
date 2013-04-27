#pragma once

#include <list>
#include <string>

#include <boost/shared_ptr.hpp>

#include <unicode_char.hpp>
#include <strEx.h>

#include <protobuf/plugin.pb.h>

#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/command_proxy.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>

namespace nscapi {
	namespace command_helper {

		typedef boost::shared_ptr<nscapi::command_proxy> command_proxy_ptr;

		struct command_info {
			std::string name;
			std::string description;
			std::list<std::string> aliases;

			command_info(std::string name, std::string description_) : name(name), description(description_) {}

			command_info(const command_info& obj) : name(obj.name), description(obj.description), aliases(obj.aliases) {}
			command_info& operator=(const command_info& obj) {
				name = obj.name;
				description = obj.description;
				aliases = obj.aliases;
				return *this;
			}

			void add_alias(std::string alias) {
				aliases.push_back(alias);
			}
		};

		class command_registry;
		class register_command_helper {
		public:
			register_command_helper(command_registry* owner_) : owner(owner_) {}
			virtual ~register_command_helper() {}

			register_command_helper& operator()(std::string command, std::string description) {
				add(boost::shared_ptr<command_info>(new command_info(command, description)));
				return *this;
			}

			register_command_helper& operator()(std::string command, std::string alias, std::string description) {
				boost::shared_ptr<command_info> d = boost::shared_ptr<command_info>(new command_info(command, description));
				d->add_alias(alias);
				add(d);
				return *this;
			}

			void add(boost::shared_ptr<command_info> d);

		private:
			command_registry* owner;
		};	

		class add_metadata_helper {
		public:
			add_metadata_helper(std::string command, command_registry* owner_) : command(command), owner(owner_) {}
			virtual ~add_metadata_helper() {}

			add_metadata_helper& operator()(std::string key, std::string value) {
				add(key, value);
				return *this;
			}

			void add(std::string key, std::string value);

		private:
			std::string command;
			command_registry* owner;
		};	


		class command_registry {
			typedef std::list<boost::shared_ptr<command_info> > command_list;
			command_list commands;
			command_proxy_ptr core_;
			std::list<std::string> errors;
		public:
			command_registry(command_proxy_ptr core) : core_(core) {}
			virtual ~command_registry() {}
			void add(boost::shared_ptr<command_info> info) {
				commands.push_back(info);
			}
			void set(std::string key, std::string value) {
				// TODO
			}

			register_command_helper command() {
				return register_command_helper(this);
			} 
			add_metadata_helper add_metadata(std::string command) {
				return add_metadata_helper(command, this);
			}

			void register_all() {
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
				for (int i=0;i<response.payload_size();i++) {
					if (response.payload(i).result().status() != Plugin::Common_Status_StatusType_STATUS_OK) {
						errors.push_back(response.payload(i).result().message());
					}
				}
			}
			void clear() {
				commands.clear();
			}
		};
	}
}