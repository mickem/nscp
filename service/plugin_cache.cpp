#include "plugin_cache.hpp"

#include <str/xtos.hpp>

#include <boost/thread/locks.hpp>
#include <boost/foreach.hpp>


void nsclient::core::plugin_cache::add_plugins(const plugin_cache_list_type & item) {
	boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!writeLock.owns_lock()) {
		LOG_ERROR_CORE("FATAL ERROR: Could not get write-mutex.");
		return;
	}
	plugin_cache_.insert(plugin_cache_.end(), item.begin(), item.end());
	has_all_ = true;
}

nsclient::core::plugin_cache::plugin_cache_list_type nsclient::core::plugin_cache::get_list() {
	boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!readLock.owns_lock()) {
		LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
		return plugin_cache_list_type();
	}
	return plugin_cache_list_type(plugin_cache_.begin(), plugin_cache_.end());
}

bool nsclient::core::plugin_cache::has_all() {
	return has_all_;
}

bool nsclient::core::plugin_cache::has_module(const std::string module) {
	boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!readLock.owns_lock()) {
		LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
		return false;
	}
	BOOST_FOREACH(const plugin_cache_item &i, plugin_cache_) {
		if (i.dll == module || i.alias == module) {
			return true;
		}
	}
	return false;
}

boost::optional<unsigned int> nsclient::core::plugin_cache::find_plugin(const ::std::string& name) {
	boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!readLock.owns_lock()) {
		LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
		return boost::optional<unsigned int>();
	}
	BOOST_FOREACH(const plugin_cache_item &i, plugin_cache_) {
		if (i.dll == name || i.alias == name) {
			return boost::optional<unsigned int>(i.id);
		}
	}
	return boost::optional<unsigned int>();
}

boost::optional<nsclient::core::plugin_cache_item> nsclient::core::plugin_cache::find_plugin_info(unsigned int id) {
	boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!readLock.owns_lock()) {
		LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
		return boost::optional<nsclient::core::plugin_cache_item>();
	}
	BOOST_FOREACH(const plugin_cache_item &i, plugin_cache_) {
		if (i.id == id) {
			return boost::optional<nsclient::core::plugin_cache_item>(i);
		}
	}
	return boost::optional<nsclient::core::plugin_cache_item>();
}


std::string nsclient::core::plugin_cache::find_plugin_alias(unsigned int plugin_id) {
	boost::optional<plugin_cache_item> info = find_plugin_info(plugin_id);
	if (!info) {
		return "Failed to find plugin: " + str::xtos(plugin_id);
	}
	if (!info->alias.empty()) {
		return info->alias;
	}
	return info->dll;
}

void nsclient::core::plugin_cache::add_plugin(nsclient::core::plugin_type plugin) {
	plugin_cache_item item(plugin);
	{
		boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
		if (!writeLock.owns_lock()) {
			LOG_ERROR_CORE("FATAL ERROR: Could not get write-mutex.");
			return;
		}
		BOOST_FOREACH(plugin_cache_item &i, plugin_cache_) {
			if (i.dll == item.dll && i.alias == item.alias) {
				i.is_loaded = item.is_loaded;
				i.id = item.id;
				i.title = item.title;
				i.desc = item.desc;
				return;
			}
		}
		plugin_cache_.push_back(plugin_cache_item(plugin));
	}
}

void nsclient::core::plugin_cache::remove_plugin(unsigned int plugin_id) {
	boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!writeLock.owns_lock()) {
		LOG_ERROR_CORE("FATAL ERROR: Could not get write-mutex.");
		return;
	}
	BOOST_FOREACH(plugin_cache_item &i, plugin_cache_) {
		if (i.id == plugin_id) {
			i.is_loaded = false;
			i.id = -1;
			return;
		}
	}
}

nsclient::core::plugin_cache_item::plugin_cache_item(const nsclient::core::plugin_type& plugin)
	: id(plugin->get_id())
	, dll(plugin->getModule())
	, alias(plugin->get_alias())
	, title(plugin->getName())
	, desc(plugin->getDescription())
	, version(plugin->get_version())
	, is_loaded(true) {}
