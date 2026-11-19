// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <list>
#include <memory>
#include <nscapi/protobuf/storage.hpp>
#include <nsclient/logger/logger.hpp>
#include <string>

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
  // Entries read from a file written before storage was keyed by the module
  // that owns them (file version < 2) are all held under this owner until a
  // module claims them (see claim_legacy). Before that fix every request was
  // resolved to whichever plugin happened to have id 0, so the owner written
  // on disk says nothing about which module the entry really belongs to.
  static const std::string legacy_owner;
  // The layout save() writes. Version 1 (and 0, a missing file) predates
  // per-module owners.
  static const long long file_version = 2;

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
  // Serialises save() with itself. save() only reads storage_, so m_mutexRW
  // is taken shared and two savers could hold it at once - and they would
  // then both be writing nsclient.tmp before renaming it over nsclient.db.
  // A plugin can now ask for a save at any time (the put `flush` flag), so
  // that is reachable rather than theoretical.
  boost::mutex save_mutex_;

 public:
  storage_manager(const path_instance& path_, const logging::logger_instance& logger) : path_(path_), logger_(logger), has_read_(false), has_changed_(false) {}
  void load();
  void put(std::string plugin_name, const ::PB::Storage::Storage_Entry& entry);
  entry_list get(std::string plugin_name, std::string context);
  // Move the legacy entries of `context` to `plugin_name`, and return them -
  // but only when `plugin_name` holds nothing in that context yet, so a module
  // that already has its own state never has it overwritten by the old shared
  // copy. Atomic, so two modules reading the same context at once cannot both
  // claim it. Returns an empty list when there was nothing to claim.
  entry_list claim_legacy(const std::string& plugin_name, const std::string& context);
  void save();

 private:
  logging::logger_instance get_logger() { return logger_; }
  std::string get_filename() const;
  std::string get_tmp_name() const;
};
typedef std::shared_ptr<storage_manager> storage_manager_instance;

}  // namespace core
}  // namespace nsclient
