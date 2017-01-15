/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <list>
#include <string>

#include <boost/shared_ptr.hpp>
#include <nscapi/dll_defines.hpp>

namespace nscapi {
	class command_proxy;
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
		class NSCAPI_EXPORT register_command_helper {
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


		class NSCAPI_EXPORT command_registry {
		private:
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

			void register_all();
			void clear() {
				commands.clear();
			}
		};
	}
}