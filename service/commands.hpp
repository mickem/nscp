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

#include "plugin_interface.hpp"

#include <nsclient/logger/logger.hpp>

#include <str/xtos.hpp>
#include <utf8.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>
#include <boost/thread.hpp>
#include <boost/algorithm/string/case_conv.hpp>

namespace nsclient {
	class commands : boost::noncopyable {
	public:
		class command_exception : public std::exception {
			std::string what_;
		public:
			command_exception(std::wstring error) throw() : what_(utf8::cvt<std::string>(error)) {}
			command_exception(std::string error) throw() : what_(error.c_str()) {}
			virtual ~command_exception() throw() {};

			virtual const char* what() const throw() {
				return what_.c_str();
			}
		};
		struct command_info {
			std::string description;
			unsigned int plugin_id;
			std::string name;
		};

		typedef boost::shared_ptr<nsclient::core::plugin_interface> plugin_type;
		typedef std::map<unsigned long, plugin_type> plugin_list_type;
		typedef std::map<std::string, command_info> description_list_type;
		typedef std::map<std::string, plugin_type> command_list_type;

	private:
		plugin_list_type plugins_;
		description_list_type descriptions_;
		command_list_type commands_;
		command_list_type aliases_;
		boost::shared_mutex mutex_;
		nsclient::logging::logger_instance logger_;

	public:

		commands(nsclient::logging::logger_instance logger): logger_(logger) {}

		void add_plugin(plugin_type plugin) {
			if (!plugin || !plugin->hasCommandHandler()) {
				return;
			}
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(30));
			if (!writeLock.owns_lock()) {
				log_error(__FILE__, __LINE__, "Failed to get mutex: commands::remove_all");
				return;
			}
			plugins_[plugin->get_id()] = plugin;
		}

		void add_plugin(int plugin_id, plugin_type plugin) {
			if (!plugin || !plugin->hasCommandHandler()) {
				return;
			}
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(30));
			if (!writeLock.owns_lock()) {
				log_error(__FILE__, __LINE__, "Failed to get mutex: commands::remove_all");
				return;
			}
			plugins_[plugin_id] = plugin;
		}

		void remove_all() {
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(30));
			if (!writeLock.owns_lock()) {
				log_error(__FILE__, __LINE__, "Failed to get mutex: commands::remove_all");
				return;
			}
			descriptions_.clear();
			commands_.clear();
			plugins_.clear();
		}

		void remove_plugin(unsigned long id) {
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(10));
			if (!writeLock.owns_lock()) {
				log_error(__FILE__, __LINE__, "Failed to get mutex in remove_plugin for plugin id: " + str::xtos(id));
				return;
			}
			command_list_type::iterator it = commands_.begin();
			while (it != commands_.end()) {
				if ((*it).second->get_id() == id) {
					std::string key = (*it).first;
					command_list_type::iterator to_erase = it;
					++it;
					commands_.erase(to_erase);
					description_list_type::iterator dit = descriptions_.find(key);
					if (dit != descriptions_.end())
						descriptions_.erase(dit);
				} else
					++it;
			}
			plugin_list_type::iterator pit = plugins_.find(id);
			if (pit != plugins_.end())
				plugins_.erase(pit);
		}

		void register_command(unsigned long plugin_id, std::string cmd, std::string desc) {
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(10));
			if (!writeLock.owns_lock()) {
				log_error(__FILE__, __LINE__, "Failed to get mutex", cmd);
				return;
			}
			std::string lc = make_key(cmd);
			if (!have_plugin(plugin_id))
				throw command_exception("Failed to find plugin: " + str::xtos(plugin_id) + " {" + unsafe_get_all_plugin_ids() + "}");
			if (commands_.find(lc) != commands_.end()) {
				log_info(__FILE__, __LINE__, "Duplicate command", cmd);
			}
			descriptions_[lc].description = desc;
			descriptions_[lc].plugin_id = plugin_id;
			descriptions_[lc].name = cmd;
			commands_[lc] = plugins_[plugin_id];
		}
		void unregister_command(unsigned long plugin_id, std::string cmd) {
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(10));
			if (!writeLock.owns_lock()) {
				log_error(__FILE__, __LINE__, "Failed to get mutex: ", cmd);
				return;
			}
			std::string lc = make_key(cmd);
			if (!have_plugin(plugin_id))
				throw command_exception("Failed to find plugin: " + str::xtos(plugin_id) + " {" + unsafe_get_all_plugin_ids() + "}");
			command_list_type::iterator it = commands_.find(lc);
			if (it == commands_.end()) {
				log_info(__FILE__, __LINE__, "Command not found: ", cmd);
			}
			commands_.erase(it);
			description_list_type::iterator dit = descriptions_.find(lc);
			if (dit != descriptions_.end())
				descriptions_.erase(dit);
		}
		void register_alias(unsigned long plugin_id, std::string cmd, std::string desc) {
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(10));
			if (!writeLock.owns_lock()) {
				log_error(__FILE__, __LINE__, "Failed to get mutex", cmd);
				return;
			}
			std::string lc = make_key(cmd);
			if (!have_plugin(plugin_id))
				throw command_exception("Failed to find plugin: " + str::xtos(plugin_id) + " {" + unsafe_get_all_plugin_ids() + "}");
			descriptions_[lc].description = desc;
			descriptions_[lc].plugin_id = plugin_id;
			descriptions_[lc].name = cmd;
			aliases_[lc] = plugins_[plugin_id];
		}

	private:

		std::string unsafe_get_all_plugin_ids() {
			std::string ret;
			std::pair<unsigned long, plugin_type> cit;
			BOOST_FOREACH(cit, plugins_) {
				ret += str::xtos(cit.first) + "(" + utf8::cvt<std::string>(cit.second->getModule()) + "), ";
			}
			return ret;
		}

	public:
		command_info describe(std::string command) {
			boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!readLock.owns_lock()) {
				log_error(__FILE__, __LINE__, "Failed to get mutex", command);
				return command_info();
			}
			std::string lc = make_key(command);
			description_list_type::const_iterator cit = descriptions_.find(lc);
			if (cit == descriptions_.end()) {
				command_info info;
				info.description = "Command not found: " + command;
				return info;
			}
			return (*cit).second;
		}

		std::list<std::string> list_all() {
			std::list<std::string> lst;
			boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!readLock.owns_lock()) {
				log_error(__FILE__, __LINE__, "Failed to get mutex");
				return lst;
			}
			BOOST_FOREACH(description_list_type::value_type cit, descriptions_) {
				lst.push_back(cit.first);
			}
			return lst;
		}
		std::list<std::string> list_commands() {
			std::list<std::string> lst;
			boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!readLock.owns_lock()) {
				log_error(__FILE__, __LINE__, "Failed to get mutex");
				return lst;
			}
			BOOST_FOREACH(description_list_type::value_type cit, descriptions_) {
				if (commands_.find(cit.first) != commands_.end())
					lst.push_back(cit.first);
			}
			return lst;
		}
		std::list<std::string> list_aliases() {
			std::list<std::string> lst;
			boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!readLock.owns_lock()) {
				log_error(__FILE__, __LINE__, "Failed to get mutex");
				return lst;
			}
			BOOST_FOREACH(description_list_type::value_type cit, descriptions_) {
				if (aliases_.find(cit.first) != aliases_.end())
					lst.push_back(cit.first);
			}
			return lst;
		}

		std::list<std::string> list_plugins() {
			std::list<std::string> lst;
			boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!readLock.owns_lock()) {
				log_error(__FILE__, __LINE__, "Failed to get mutex");
				return lst;
			}
			std::pair<unsigned long, plugin_type> cit;
			BOOST_FOREACH(cit, plugins_) {
				lst.push_back(str::xtos(cit.first));
			}
			return lst;
		}

		plugin_type get(std::string command) {
			boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!readLock.owns_lock()) {
				log_error(__FILE__, __LINE__, "Failed to get mutex", command);
				throw command_exception("Failed to get mutex (commands::get)");
			}
			std::string lc = make_key(command);
			command_list_type::iterator cit = commands_.find(lc);
			if (cit != commands_.end())
				return (*cit).second;
			cit = aliases_.find(lc);
			if (cit != aliases_.end())
				return (*cit).second;
			return plugin_type();
		}

		std::string to_string() {
			std::string ret = "commands {";
			BOOST_FOREACH(std::string str, list_all()) {
				if (!ret.empty()) ret += ", ";
				ret += str;
			}
			ret += "}, plugins {";
			BOOST_FOREACH(std::string str, list_plugins()) {
				if (!ret.empty()) ret += ", ";
				ret += str;
			}
			ret += "}";
			return ret;
		}

		static std::string make_key(std::string key) {
			return boost::algorithm::to_lower_copy(key);
		}
		void log_error(const char* file, int line, std::string error) {
			logger_->error("core", file, line, error);
		}
		void log_error(const char* file, int line, std::string error, std::string command) {
			logger_->error("core", file, line, error + "for command: " + utf8::cvt<std::string>(command));
		}
		void log_info(const char* file, int line, std::string error, std::string command) {
			logger_->info("core", file, line, error + "for command: " + utf8::cvt<std::string>(command));
		}

		inline bool have_plugin(unsigned long plugin_id) {
			return !(plugins_.find(plugin_id) == plugins_.end());
		}
	};
}