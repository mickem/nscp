#pragma once

#include "plugin_interface.hpp"
#include "path_manager.hpp"

#include <nsclient/logger/logger.hpp>
#include <nscapi/nscapi_protobuf.hpp>

#include <boost/thread/shared_mutex.hpp>
#include <boost/optional.hpp>

#include <string>
#include <list>

namespace nsclient {
	namespace core {

		typedef boost::shared_ptr<nsclient::core::plugin_interface> plugin_type;

		struct storage_item {
			bool is_modified;
			std::string owner;

			::Plugin::Storage::Entry entry;

			storage_item() : is_modified(false) {}
			storage_item(const std::string owner, const ::Plugin::Storage::Entry entry_) 
				: is_modified(true)
				, owner(owner) 
			{
				entry.CopyFrom(entry_);
			}
			storage_item(const storage_item& other)
				: is_modified(other.is_modified)
				, owner(other.owner)
			{
				entry.CopyFrom(other.entry);
			}
			const storage_item& operator=(const storage_item& other) {
				is_modified = other.is_modified;
				owner = other.owner;
				entry.CopyFrom(other.entry);
				return *this;
			}
		};

		class storage_manager {
		public:
			typedef std::map<std::string, storage_item> storage_type;
			typedef std::list<std::string> key_list_type;
			typedef std::list<Plugin::Storage_Entry> entry_list;

		private:
			nsclient::core::path_instance path_;
			nsclient::logging::logger_instance logger_;
			storage_type storage_;
			key_list_type deleted_;
			bool has_read_;
			boost::shared_mutex m_mutexRW;
			
		public:
			storage_manager(nsclient::core::path_instance path_, nsclient::logging::logger_instance logger) : path_(path_), logger_(logger), has_read_(false) {}
			void load();
			void put(std::string plugin_name, const ::Plugin::Storage_Entry& entry);
			entry_list get(std::string plugin_name, std::string context);
			void save();

		private:
			nsclient::logging::logger_instance get_logger() {
				return logger_;
			}
			std::string get_filename();
			std::string get_tmpname();
		};
		typedef boost::shared_ptr<storage_manager> storage_manager_instance;

	}
}
