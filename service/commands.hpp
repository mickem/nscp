#pragma once
#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>

#include "NSCPlugin.h"
#include "logger.hpp"
#include <strEx.h>

using namespace nscp::helpers;

namespace nsclient {
	class commands : boost::noncopyable {
	public:
		class command_exception : public std::exception {
			std::string what_;
		public:
			command_exception(std::wstring error) throw() : what_(to_string(error).c_str()) {}
			command_exception(std::string error) throw() : what_(error.c_str()) {}
			virtual ~command_exception() throw() {};

			virtual const char* what() const throw() {
				return what_.c_str();
			}

		};

		typedef boost::shared_ptr<NSCPlugin> plugin_type;
		typedef std::map<unsigned long,plugin_type> plugin_list_type;
		typedef std::map<std::wstring,std::wstring> description_list_type;
		typedef std::map<std::wstring,plugin_type> command_list_type;


	private:
		nsclient::logger *logger_;
		plugin_list_type plugins_;
		description_list_type descriptions_;
		command_list_type commands_;
		boost::shared_mutex mutex_;

	public:

		commands(nsclient::logger *logger) : logger_(logger) {}

		void add_plugin(plugin_type plugin) {
			if (!plugin || !plugin->hasCommandHandler()) {
				return;
			}
			plugins_[plugin->get_id()] = plugin;
			
		}

		void remove_all() {
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(30));
			if (!writeLock.owns_lock()) {
				log_error(__FILEW__, __LINE__, _T("Failed to get mutex: commands::remove_all"));
				return;
			}
			descriptions_.clear();
			commands_.clear();
			plugins_.clear();
		}

		void remove_plugin(unsigned long id) {
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(10));
			if (!writeLock.owns_lock()) {
				log_error(__FILEW__, __LINE__, _T("Failed to get mutex in remove_plugin for plugin id: ") + ::to_wstring(id));
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

		void register_command(unsigned long plugin_id, std::wstring cmd, std::wstring desc) {
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(10));
			if (!writeLock.owns_lock()) {
				log_error(__FILEW__, __LINE__, _T("Failed to get mutex: ") + cmd);
				return;
			}
			std::wstring lc = make_key(cmd);
			if (!have_plugin(plugin_id))
				throw command_exception("Failed to find plugin: " + ::to_string(plugin_id) + " {" + unsafe_get_all_plugin_ids() + "}");
			descriptions_[lc] = desc;
			commands_[lc] = plugins_[plugin_id];
		}
private:

		std::string unsafe_get_all_plugin_ids() {
			std::string ret;
			std::pair<unsigned long,plugin_type> cit;
			BOOST_FOREACH(cit, plugins_) {
				ret += ::to_string(cit.first) + ", ";
				//lst.push_back(::to_wstring(cit.first) + _T("=") + cit.second->getName());
				//lst.push_back(::to_wstring(cit.first));
			}
			return ret;
		}


public:
		std::wstring describe(std::wstring command) {
			boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!readLock.owns_lock()) {
				log_error(__FILEW__, __LINE__, _T("Failed to get mutex: ") + command);
				return _T("error: ") + command;
			}
			std::wstring lc = make_key(command);
			description_list_type::const_iterator cit = descriptions_.find(lc);
			if (cit == descriptions_.end())
				return _T("Command not found: ") + command;
			return (*cit).second;
		}

		std::list<std::wstring> list() {
			std::list<std::wstring> lst;
			boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!readLock.owns_lock()) {
				log_error(__FILEW__, __LINE__, _T("Failed to get mutex"));
				return lst;
			}
			std::pair<std::wstring,std::wstring> cit;
			BOOST_FOREACH(cit, descriptions_) {
				lst.push_back(cit.first);
			}
			return lst;
		}

		std::list<std::wstring> list_plugins() {
			std::list<std::wstring> lst;
			boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!readLock.owns_lock()) {
				log_error(__FILEW__, __LINE__, _T("Failed to get mutex"));
				return lst;
			}
			std::pair<unsigned long,plugin_type> cit;
			BOOST_FOREACH(cit, plugins_) {
				//lst.push_back(::to_wstring(cit.first) + _T("=") + cit.second->getName());
				lst.push_back(::to_wstring(cit.first));
			}
			return lst;
		}

		plugin_type get(std::wstring command) {
			boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!readLock.owns_lock()) {
				log_error(__FILEW__, __LINE__, _T("Failed to get mutex: ") + command);
				throw command_exception("Failed to get mutex (commands::get)");
			}
			std::wstring lc = make_key(command);
			command_list_type::iterator cit = commands_.find(lc);
			if (cit == commands_.end()) {
				std::wcout << _T("NOT FOUND") << std::endl;
				return plugin_type();
			}
			return (*cit).second;
		}

		std::wstring to_wstring() {
			std::wstring ret = _T("commands {");
			BOOST_FOREACH(std::wstring str, list()) {
				if (!ret.empty()) ret += _T(", ");
				ret += str;
			}
			ret += _T("}, plugins {");
			BOOST_FOREACH(std::wstring str, list_plugins()) {
				if (!ret.empty()) ret += _T(", ");
				ret += str;
			}
			ret += _T("}");
			return ret;
		}

		static std::wstring make_key(std::wstring key) {
			return boost::algorithm::to_lower_copy(key);
		}
		void log_error(std::wstring file, int line, std::wstring error) {
			if (logger_ != NULL)
				logger_->nsclient_log_error(file, line, error);
		}

		inline bool have_plugin(unsigned long plugin_id) {
			return !(plugins_.find(plugin_id) == plugins_.end());
		}


	};
}