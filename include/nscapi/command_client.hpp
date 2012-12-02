#pragma once

#include <list>
#include <string>

#include <boost/shared_ptr.hpp>

#include <unicode_char.hpp>
#include <strEx.h>

#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/command_proxy.hpp>

namespace nscapi {
	namespace command_helper {

		typedef boost::shared_ptr<nscapi::command_proxy> command_proxy_ptr;

		struct command_info {
			std::wstring name;
			std::wstring description;
			std::list<std::wstring> aliases;

			command_info(std::wstring name, std::wstring description_) : name(name), description(description_) {}

			command_info(const command_info& obj) : name(obj.name), description(obj.description), aliases(obj.aliases) {}
			command_info& operator=(const command_info& obj) {
				name = obj.name;
				description = obj.description;
				aliases = obj.aliases;
				return *this;
			}

			void add_alias(std::wstring alias) {
				aliases.push_back(alias);
			}
		};

		class command_registry;
		class register_command_helper {
		public:
			register_command_helper(command_registry* owner_) : owner(owner_) {}
			virtual ~register_command_helper() {}

			register_command_helper& operator()(std::wstring command, std::wstring description) {
				add(boost::shared_ptr<command_info>(new command_info(command, description)));
				return *this;
			}

			register_command_helper& operator()(std::wstring command, std::wstring alias, std::wstring description) {
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
			add_metadata_helper(std::wstring command, command_registry* owner_) : command(command), owner(owner_) {}
			virtual ~add_metadata_helper() {}

			add_metadata_helper& operator()(std::wstring key, std::wstring value) {
				add(key, value);
				return *this;
			}

			void add(std::wstring key, std::wstring value);

		private:
			std::wstring command;
			command_registry* owner;
		};	


		class command_registry {
			typedef std::list<boost::shared_ptr<command_info> > command_list;
			command_list commands;
			command_proxy_ptr core_;
		public:
			command_registry(command_proxy_ptr core) : core_(core) {}
			virtual ~command_registry() {}
			void add(boost::shared_ptr<command_info> info) {
				commands.push_back(info);
			}
			void set(std::wstring key, std::wstring value) {
				// TODO
			}

			register_command_helper command() {
				return register_command_helper(this);
			} 
			add_metadata_helper add_metadata(std::wstring command) {
				return add_metadata_helper(command, this);
			}

			void register_all() {
				BOOST_FOREACH(command_list::value_type v, commands) {
					core_->register_command(v->name, v->description);
				}
			}
			void clear() {
				commands.clear();
			}
		};
	}
}