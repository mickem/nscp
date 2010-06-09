#pragma once

#include <set>

#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>

#include "NSCPlugin.h"
#include "logger.hpp"

using namespace nscp::helpers;

namespace nsclient {
	class channels : boost::noncopyable {
	public:
		class channel_exception : public std::exception {
			std::string what_;
		public:
			channel_exception(std::wstring error) throw() : what_(to_string(error).c_str()) {}
			channel_exception(std::string error) throw() : what_(error.c_str()) {}
			virtual ~channel_exception() throw() {};

			virtual const char* what() const throw() {
				return what_.c_str();
			}

		};

		typedef boost::shared_ptr<NSCPlugin> plugin_type;
		typedef std::map<unsigned long,plugin_type> plugin_list_type;
		typedef std::set<unsigned long> plugin_id_type;
		typedef std::map<std::wstring,plugin_id_type > channel_list_type;


	private:
		nsclient::logger *logger_;
		plugin_list_type plugins_;
		channel_list_type channels_;
		boost::shared_mutex mutex_;

	public:

		channels(nsclient::logger *logger) : logger_(logger) {}

		void add_plugin(plugin_type plugin) {
			if (!plugin || !plugin->hasNotificationHandler())
				return;
			plugins_[plugin->get_id()] = plugin;
			
		}

		void remove_all() {
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(30));
			if (!writeLock.owns_lock()) {
				log_error(__FILEW__, __LINE__, _T("Failed to get mutex: channels::remove_all"));
				return;
			}
			channels_.clear();
			plugins_.clear();
		}

		void remove_plugin(unsigned long id) {
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(10));
			if (!writeLock.owns_lock()) {
				log_error(__FILEW__, __LINE__, _T("Failed to get mutex in remove_plugin for plugin id: ") + ::to_wstring(id));
				return;
			}
			channel_list_type::iterator it = channels_.begin();
			while (it != channels_.end()) {
				if ((*it).second.count(id) > 0) {
					channel_list_type::iterator toerase = it;
					++it;
					channels_.erase(toerase);
				} else
					++it;
			}
			plugin_list_type::iterator pit = plugins_.find(id);
			if (pit != plugins_.end())
				plugins_.erase(pit);
		}

		void register_listener(unsigned long plugin_id, std::wstring channel) {
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(10));
			if (!writeLock.owns_lock()) {
				log_error(__FILEW__, __LINE__, _T("Failed to get mutex: ") + channel);
				return;
			}
			std::wstring lc = make_key(channel);
			if (!have_plugin(plugin_id))
				throw channel_exception("Failed to find plugin: " + ::to_string(plugin_id));
			channels_[lc].insert(plugin_id);
		}

		std::list<std::wstring> list() {
			std::list<std::wstring> lst;
			boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!readLock.owns_lock()) {
				log_error(__FILEW__, __LINE__, _T("Failed to get mutex"));
				return lst;
			}
			
			BOOST_FOREACH(channel_list_type::value_type i, channels_) {
				lst.push_back(i.first);
			}
			return lst;
		}

		std::list<plugin_type> get(std::wstring channel) {
			boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!readLock.owns_lock()) {
				log_error(__FILEW__, __LINE__, _T("Failed to get mutex: ") + channel);
				throw channel_exception("Failed to get mutex (channel::get)");
			}
			std::wstring lc = make_key(channel);
			channel_list_type::iterator cit = channels_.find(lc);
			if (cit == channels_.end()) {
				throw channel_exception("Channel not found: " + to_string(channel));
			}
			std::list<plugin_type> ret;
			BOOST_FOREACH(unsigned long id, cit->second) {
				ret.push_back(plugins_[id]);
			}
			return ret;
		}

		std::wstring to_wstring() {
			std::wstring ret;
			BOOST_FOREACH(std::wstring str, list()) {
				if (!ret.empty()) ret += _T(", ");
				ret += str;
			}
			return ret;
		}

		inline std::wstring make_key(std::wstring key) {
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