#pragma once

#include "plugin_interface.hpp"

#include <nsclient/logger/logger.hpp>

#include <boost/thread/shared_mutex.hpp>
#include <boost/optional.hpp>

#include <string>
#include <list>

namespace nsclient {
	namespace core {

		typedef boost::shared_ptr<nsclient::core::plugin_interface> plugin_type;

		struct plugin_cache_item {
			unsigned int id;
			std::string dll;
			std::string alias;
			std::string title;
			std::string desc;
			std::string version;
			bool is_loaded;

			plugin_cache_item() : id(0), is_loaded(false) {}
			plugin_cache_item(const plugin_type& other);
			plugin_cache_item(const plugin_cache_item& other)
				: id(other.id)
				, dll(other.dll)
				, alias(other.alias)
				, title(other.title)
				, desc(other.desc)
				, version(other.version)
				, is_loaded(other.is_loaded) {}
			const plugin_cache_item& operator=(const plugin_cache_item& other) {
				id = other.id;
				dll = other.dll;
				alias = other.alias;
				id = other.id;
				title = other.title;
				desc = other.desc;
				version = other.version;
				is_loaded = other.is_loaded;
				return *this;
			}
		};

		class plugin_cache {
		public:
			typedef std::list<plugin_cache_item> plugin_cache_list_type;

		private:
			nsclient::logging::logger_instance logger_;
			plugin_cache_list_type plugin_cache_;
			boost::shared_mutex m_mutexRW;
			bool has_all_;
			
		public:
			plugin_cache(nsclient::logging::logger_instance logger) : logger_(logger), has_all_(false){}
			void add_plugins(const plugin_cache_list_type &item);
			plugin_cache_list_type get_list();
			bool has_all();
			bool has_module(const std::string module);
			boost::optional<plugin_cache_item> find_plugin_info(unsigned int id);
			boost::optional<unsigned int> find_plugin(const ::std::string& name);

			std::string find_plugin_alias(unsigned int plugin_id);
			void add_plugin(plugin_type plugin);
			void remove_plugin(unsigned int plugin_id);

		private:
			nsclient::logging::logger_instance get_logger() {
				return logger_;
			}
		};
	}
}
