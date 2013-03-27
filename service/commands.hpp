#pragma once
#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>

#include "NSCPlugin.h"
#include <nsclient/logger.hpp>
#include <strEx.h>

using namespace nscp::helpers;

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

		typedef boost::shared_ptr<NSCPlugin> plugin_type;
		typedef std::map<unsigned long,plugin_type> plugin_list_type;
		typedef std::map<std::string,command_info> description_list_type;
		typedef std::map<std::string,plugin_type> command_list_type;


	private:
		plugin_list_type plugins_;
		description_list_type descriptions_;
		command_list_type commands_;
		command_list_type aliases_;
		boost::shared_mutex mutex_;

	public:

		commands() {}

		void add_plugin(plugin_type plugin) {
			if (!plugin || !plugin->hasCommandHandler()) {
				return;
			}
			plugins_[plugin->get_id()] = plugin;
			
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
				log_error(__FILE__, __LINE__, "Failed to get mutex in remove_plugin for plugin id: " + ::to_string(id));
				return;
			}
			command_list_type::iterator it = commands_.begin();
			while (it != commands_.end()) {
				if ((*it).second->get_id() == id) {
					command_list_type::iterator toerase = it;
					++it;
					commands_.erase(toerase);
					description_list_type::iterator dit = descriptions_.find((*toerase).first);
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
				throw command_exception("Failed to find plugin: " + ::to_string(plugin_id) + " {" + unsafe_get_all_plugin_ids() + "}");
			if (commands_.find(lc) != commands_.end()) {
				log_error(__FILE__,__LINE__, "Duplicate command", cmd);
			descriptions_[lc].description = desc;
			descriptions_[lc].plugin_id = plugin_id;
			descriptions_[lc].name = cmd;
			}
			commands_[lc] = plugins_[plugin_id];
		}
		void register_alias(unsigned long plugin_id, std::string cmd, std::string desc) {
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(10));
			if (!writeLock.owns_lock()) {
				log_error(__FILE__, __LINE__, "Failed to get mutex", cmd);
				return;
			}
			std::string lc = make_key(cmd);
			if (!have_plugin(plugin_id))
				throw command_exception("Failed to find plugin: " + ::to_string(plugin_id) + " {" + unsafe_get_all_plugin_ids() + "}");
			descriptions_[lc].description = desc;
			descriptions_[lc].plugin_id = plugin_id;
			descriptions_[lc].name = cmd;
			aliases_[lc] = plugins_[plugin_id];
		}

private:

		std::string unsafe_get_all_plugin_ids() {
			std::string ret;
			std::pair<unsigned long,plugin_type> cit;
			BOOST_FOREACH(cit, plugins_) {
				ret += strEx::s::xtos(cit.first) + "(" + utf8::cvt<std::string>(cit.second->getFilename()) + "), ";
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
			std::pair<unsigned long,plugin_type> cit;
			BOOST_FOREACH(cit, plugins_) {
				lst.push_back(::to_string(cit.first));
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
			if (cit == commands_.end()) {
				std::wcout << _T("NOT FOUND") << std::endl;
				return plugin_type();
			}
			return (*cit).second;
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
			nsclient::logging::logger::get_logger()->error("core", file, line, error);
		}
		void log_error(const char* file, int line, std::string error, std::string command) {
			nsclient::logging::logger::get_logger()->error("core", file, line, error + "for command: " + utf8::cvt<std::string>(command));
		}

		inline bool have_plugin(unsigned long plugin_id) {
			return !(plugins_.find(plugin_id) == plugins_.end());
		}
	};
}
