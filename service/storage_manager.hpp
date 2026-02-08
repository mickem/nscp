#pragma once

#include <boost/thread/shared_mutex.hpp>
#include <list>
#include <nscapi/nscapi_protobuf_storage.hpp>
#include <nsclient/logger/logger.hpp>
#include <string>
#include <memory>

#include "path_manager.hpp"
#include "plugins/plugin_interface.hpp"

namespace nsclient {
namespace core {

typedef std::shared_ptr<plugin_interface> plugin_type;

struct storage_item {
  bool is_modified;
  std::string owner;

  PB::Storage::Storage::Entry entry;

  storage_item() : is_modified(false) {}
  storage_item(const std::string owner, const ::PB::Storage::Storage::Entry entry_) : is_modified(true), owner(owner) { entry.CopyFrom(entry_); }
  storage_item(const storage_item& other) : is_modified(other.is_modified), owner(other.owner) { entry.CopyFrom(other.entry); }
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
  typedef std::list<PB::Storage::Storage_Entry> entry_list;

 private:
  path_instance path_;
  logging::logger_instance logger_;
  storage_type storage_;
  key_list_type deleted_;
  bool has_read_;
  bool has_changed_;
  boost::shared_mutex m_mutexRW;

 public:
  storage_manager(const path_instance& path_, const logging::logger_instance& logger) : path_(path_), logger_(logger), has_read_(false), has_changed_(false) {}
  void load();
  void put(std::string plugin_name, const ::PB::Storage::Storage_Entry& entry);
  entry_list get(std::string plugin_name, std::string context);
  void save();

 private:
  logging::logger_instance get_logger() { return logger_; }
  std::string get_filename() const;
  std::string get_tmp_name() const;
};
typedef boost::shared_ptr<storage_manager> storage_manager_instance;

}  // namespace core
}  // namespace nsclient
