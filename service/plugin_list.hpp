#pragma once

#include <set>

#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>
#include <boost/function.hpp>

#include "NSCPlugin.h"
#include <nsclient/logger.hpp>

namespace nsclient {
	typedef boost::shared_ptr<NSCPlugin> plugin_type;
	typedef std::map<unsigned long,plugin_type> plugin_list_type;
	typedef std::set<unsigned long> plugin_id_type;

	class plugins_list_exception : public std::exception {
		std::string what_;
	public:
		plugins_list_exception(std::string error) throw() : what_(error.c_str()) {}
		virtual ~plugins_list_exception() throw() {};

		virtual const char* what() const throw() {
			return what_.c_str();
		}
	};


	struct simple_plugins_list : public boost::noncopyable {

		typedef std::list<plugin_type> simple_plugin_list_type;
		simple_plugin_list_type plugins_;
		boost::shared_mutex mutex_;

		simple_plugins_list() {}

		bool has_valid_lock_log(boost::unique_lock<boost::shared_mutex> &lock, std::string key) {
			if (!lock.owns_lock()) {
				log_error(__FILE__, __LINE__, "Failed to get mutex", key);
				return false;
			}
			return true;
		}
		bool has_valid_lock_log(boost::shared_lock<boost::shared_mutex> &lock, std::string key) {
			if (!lock.owns_lock()) {
				log_error(__FILE__, __LINE__, "Failed to get mutex", key);
				return false;
			}
			return true;
		}
		void has_valid_lock_throw(boost::unique_lock<boost::shared_mutex> &lock, std::string key) {
			if (!lock.owns_lock()) {
				log_error(__FILE__, __LINE__, "Failed to get mutex", key);
				throw plugins_list_exception("Failed to get mutex: " + utf8::cvt<std::string>(key));
			}
		}
		void has_valid_lock_throw(boost::shared_lock<boost::shared_mutex> &lock, std::string key) {
			if (!lock.owns_lock()) {
				log_error(__FILE__, __LINE__, "Failed to get mutex", key);
				throw plugins_list_exception("Failed to get mutex: " + utf8::cvt<std::string>(key));
			}
		}

		void add_plugin(plugin_type plugin) {
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(30));
			if (!has_valid_lock_log(writeLock, "plugins_list::add_plugin"))
				return;
			BOOST_FOREACH(const plugin_type &p, plugins_) {
				if (p->get_id() == plugin->get_id()) {
					log_error(__FILE__, __LINE__, "Duplicate plugin id");
					return;
				}
			}
			plugins_.push_back(plugin);
		}

		void remove_all() {
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(30));
			if (!has_valid_lock_log(writeLock, "plugins_list::remove_all"))
				return;
			plugins_.clear();
		}

		void remove_plugin(unsigned long id) {
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(10));
			if (!has_valid_lock_log(writeLock, "plugins_list::remove_plugin" + strEx::s::xtos(id)))
				return;
			for (simple_plugin_list_type::iterator it = plugins_.begin(); it != plugins_.end(); ++it) {
				if ((*it)->get_id() == id)
					it = plugins_.erase(it);
			}
		}

		void do_all(boost::function<void(plugin_type)> fun) {
			boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!has_valid_lock_log(readLock, "plugins_list::list"))
				return;
			BOOST_FOREACH(const plugin_type p, plugins_) {
				fun(p);
			}
		}

		std::string to_string() {
			std::string ret;
			boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!has_valid_lock_log(readLock, "plugins_list::list"))
				return "";
			BOOST_FOREACH(const plugin_type p, plugins_) {
				if (!ret.empty()) ret += ", ";
				ret += p->getName();
			}
			return ret;
		}

		void log_error(const char *file, int line, std::string error) {
			nsclient::logging::logger::get_logger()->error("plugin", file, line, error);
		}
		void log_error(const char *file, int line, std::string error, std::string key) {
			nsclient::logging::logger::get_logger()->error("plugin", file, line, error + " for " + utf8::cvt<std::string>(key));
		}
	};



	template<class parent>
	struct plugins_list : boost::noncopyable, public parent {

		plugin_list_type plugins_;
		boost::shared_mutex mutex_;

		plugins_list() {}

		bool has_valid_lock_log(boost::unique_lock<boost::shared_mutex> &lock, std::string key) {
			if (!lock.owns_lock()) {
				log_error(__FILE__, __LINE__, "Failed to get mutex", key);
				return false;
			}
			return true;
		}
		bool has_valid_lock_log(boost::shared_lock<boost::shared_mutex> &lock, std::string key) {
			if (!lock.owns_lock()) {
				log_error(__FILE__, __LINE__, "Failed to get mutex", key);
				return false;
			}
			return true;
		}
		void has_valid_lock_throw(boost::unique_lock<boost::shared_mutex> &lock, std::string key) {
			if (!lock.owns_lock()) {
				log_error(__FILE__, __LINE__, "Failed to get mutex", key);
				throw plugins_list_exception("Failed to get mutex: " + utf8::cvt<std::string>(key));
			}
		}
		void has_valid_lock_throw(boost::shared_lock<boost::shared_mutex> &lock, std::string key) {
			if (!lock.owns_lock()) {
				log_error(__FILE__, __LINE__, "Failed to get mutex", key);
				throw plugins_list_exception("Failed to get mutex: " + utf8::cvt<std::string>(key));
			}
		}

		void add_plugin(plugin_type plugin) {
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(30));
			if (!has_valid_lock_log(writeLock, "plugins_list::add_plugin"))
				return;
			plugins_[plugin->get_id()] = plugin;
			parent::add_plugin(plugin);
		}

		void remove_all() {
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(30));
			if (!has_valid_lock_log(writeLock, "plugins_list::remove_all"))
				return;
			plugins_.clear();
			parent::remove_all();
		}

		void remove_plugin(unsigned long id) {
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(10));
			if (!has_valid_lock_log(writeLock, "plugins_list::remove_plugin" + strEx::s::xtos(id)))
				return;
			plugin_list_type::iterator pit = plugins_.find(id);
			if (pit != plugins_.end())
				plugins_.erase(pit);
			parent::remove_plugin(id);
		}

		std::list<std::string> list() {
			std::list<std::string> lst;
			boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!has_valid_lock_log(readLock, "plugins_list::list"))
				return lst;
			parent::list(lst);
			return lst;
		}

		std::string to_string() {
			std::string ret;
			std::list<std::string> lst = list();
			BOOST_FOREACH(std::string str, lst) {
				if (!ret.empty()) ret += ", ";
				ret += str;
			}
			return ret + parent::to_string();
		}

		inline std::string make_key(std::string key) {
			return boost::algorithm::to_lower_copy(key);
		}
		void log_error(const char *file, int line, std::string error) {
			nsclient::logging::logger::get_logger()->error("plugin", file, line, error);
		}
		void log_error(const char *file, int line, std::string error, std::string key) {
			nsclient::logging::logger::get_logger()->error("plugin", file, line, error + " for " + utf8::cvt<std::string>(key));
		}

		inline bool have_plugin(unsigned long plugin_id) {
			return !(plugins_.find(plugin_id) == plugins_.end());
		}
	};


	struct plugins_list_listeners_impl {
		typedef std::map<std::string,plugin_id_type > listener_list_type;
		listener_list_type listeners_;

		void add_plugin(plugin_type plugin) {}

		void remove_all() {
			listeners_.clear();
		}

		void remove_plugin(unsigned long id) {
			listener_list_type::iterator it = listeners_.begin();
			while (it != listeners_.end()) {
				if ((*it).second.count(id) > 0) {
					listener_list_type::iterator toerase = it;
					++it;
					listeners_.erase(toerase);
				} else
					++it;
			}
		}

		void list(std::list<std::string> &lst) {
			BOOST_FOREACH(listener_list_type::value_type i, listeners_) {
				lst.push_back(i.first);
			}
		}
		std::string to_string() {
			std::string ret;
			BOOST_FOREACH(listener_list_type::value_type i, listeners_) {
				ret += ", ";
				ret += i.first;
			}
			return ret;
		}
	};

	struct plugins_list_with_listener : plugins_list<plugins_list_listeners_impl> {
		typedef plugins_list<plugins_list_listeners_impl> parent_type;

		plugins_list_with_listener() : parent_type() {}

		void register_listener(unsigned long plugin_id, std::string channel) {
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(10));
			if (!writeLock.owns_lock()) {
				log_error(__FILE__, __LINE__, "Failed to get mutex: ", channel);
				return;
			}
			std::string lc = make_key(channel);
			if (!have_plugin(plugin_id)) {
				writeLock.unlock();
				throw plugins_list_exception("Failed to find plugin: " + strEx::s::xtos(plugin_id) + ", Plugins: " + to_string());
			}
			listeners_[lc].insert(plugin_id);
		}

		std::list<plugin_type> get(std::string channel) {
			boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
			has_valid_lock_throw(readLock, "plugins_list::get:" + channel);
			std::string lc = make_key(channel);
			plugins_list_listeners_impl::listener_list_type::iterator cit = listeners_.find(lc);
			if (cit == listeners_.end()) {
				return std::list<plugin_type>(); // throw plugins_list_exception("Channel not found: '" + ::to_string(channel) + "'" + to_string());
			}
			std::list<plugin_type> ret;
			BOOST_FOREACH(unsigned long id, cit->second) {
				ret.push_back(plugins_[id]);
			}
			return ret;
		}
	};

}