// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "storage_manager.hpp"

#include <nscapi/protobuf/io.hpp>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/thread/locks.hpp>
#include <file_helpers.hpp>
#include <fstream>
#include <str/xtos.hpp>

std::string mk_key(const std::string &plugin_name, const std::string &context, const std::string key = "") { return plugin_name + "." + context + "." + key; }

const std::string nsclient::core::storage_manager::legacy_owner = "";

template <typename T>
bool read_chunk(::google::protobuf::io::CodedInputStream &stream, T &obj) {
  uint32_t size = 0;
  stream.ReadVarint32(&size);
  std::string tmp;
  stream.ReadString(&tmp, size);
  obj.ParseFromString(tmp);
  return true;
}
void nsclient::core::storage_manager::load() {
  // load() runs at boot before any plugin exists, so it does not overlap
  // put()/get(). save() no longer has that guarantee: a plugin can ask for one
  // from its own thread while requests are being served (the put `flush`
  // flag), which is exactly what the locking here and in save() is for.
  boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!writeLock.owns_lock()) {
    LOG_ERROR_CORE("FATAL ERROR: Could not get write-mutex.");
    return;
  }

  std::string file = get_filename();

  std::ifstream in(file.c_str(), std::ios::in | std::ios::binary);

  typedef std::shared_ptr<google::protobuf::io::ZeroCopyInputStream> istr_type;
  typedef std::shared_ptr<google::protobuf::io::CodedInputStream> codedstr_type;
  istr_type raw_in = istr_type(new ::google::protobuf::io::IstreamInputStream(&in));
  codedstr_type coded_in = codedstr_type(new ::google::protobuf::io::CodedInputStream(raw_in.get()));

  ::PB::Storage::Storage::File header;
  if (!read_chunk(*coded_in, header)) {
    LOG_ERROR_CORE("Failed to read storage.");
    return;
  }
  // A file older than version 2 was written while every request resolved to
  // the plugin with id 0, so its owners are not the modules the entries
  // belong to. Hold them all under the legacy owner; each module claims its
  // own contexts on its first read (claim_legacy).
  const bool legacy = header.version() < file_version;
  for (long long i = 0; i < header.entries(); i++) {
    ::PB::Storage::Storage::Block block;
    if (!read_chunk(*coded_in, block)) {
      LOG_ERROR_CORE("Failed to read block " + str::xtos(i) + " from storage.");
      continue;
    }
    const std::string owner = legacy ? legacy_owner : block.owner();
    storage_[mk_key(owner, block.entry().context(), block.entry().key())] = storage_item(owner, block.entry());
  }
  // Loading a legacy file marks the store changed, so the next save writes
  // the new version even if no module has claimed anything yet.
  has_changed_ = legacy && header.entries() > 0;
}

void nsclient::core::storage_manager::put(std::string plugin_name, const ::PB::Storage::Storage_Entry &entry) {
  boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!writeLock.owns_lock()) {
    LOG_ERROR_CORE("FATAL ERROR: Could not get write-mutex.");
    return;
  }
  std::string key = mk_key(plugin_name, entry.context(), entry.key());
  storage_[key] = storage_item(plugin_name, entry);
  has_changed_ = true;
}

nsclient::core::storage_manager::entry_list nsclient::core::storage_manager::get(std::string plugin_name, std::string context) {
  entry_list ret;
  boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!readLock.owns_lock()) {
    LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
    return ret;
  }
  std::string key = mk_key(plugin_name, context);
  for (const storage_type::value_type &v : storage_) {
    // The prefix of "disk" also covers "disk.trends"; only the context asked
    // for belongs in the answer.
    if (boost::algorithm::starts_with(v.first, key) && v.second.entry.context() == context) {
      ret.push_back(v.second.entry);
    }
  }
  return ret;
}

nsclient::core::storage_manager::entry_list nsclient::core::storage_manager::claim_legacy(const std::string &plugin_name, const std::string &context) {
  entry_list ret;
  if (plugin_name == legacy_owner) {
    return ret;
  }
  boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!writeLock.owns_lock()) {
    LOG_ERROR_CORE("FATAL ERROR: Could not get write-mutex.");
    return ret;
  }
  // Keys are "<owner>.<context>.<key>", so the prefix of "disk" also covers
  // "disk.trends": compare the entry's own context, not just the prefix.
  const std::string own_prefix = mk_key(plugin_name, context);
  for (storage_type::const_iterator it = storage_.lower_bound(own_prefix); it != storage_.end() && boost::algorithm::starts_with(it->first, own_prefix);
       ++it) {
    if (it->second.entry.context() == context) {
      return ret;
    }
  }
  const std::string legacy_prefix = mk_key(legacy_owner, context);
  storage_type::iterator it = storage_.lower_bound(legacy_prefix);
  while (it != storage_.end() && boost::algorithm::starts_with(it->first, legacy_prefix)) {
    if (it->second.entry.context() != context) {
      ++it;
      continue;
    }
    const ::PB::Storage::Storage_Entry entry = it->second.entry;
    it = storage_.erase(it);
    storage_[mk_key(plugin_name, entry.context(), entry.key())] = storage_item(plugin_name, entry);
    ret.push_back(entry);
  }
  if (!ret.empty()) {
    has_changed_ = true;
    LOG_INFO_CORE("Storage: moved " + str::xtos(ret.size()) + " entries of " + context + " from the shared pre-upgrade store to " + plugin_name);
  }
  return ret;
}

template <typename T>
bool write_chunk(::google::protobuf::io::CodedOutputStream &stream, const T &obj) {
  std::string tmp;
  obj.SerializeToString(&tmp);
  stream.WriteVarint32(static_cast<uint32_t>(tmp.size()));
  stream.WriteString(tmp);
  return true;
}

void nsclient::core::storage_manager::save() {
  // One saver at a time: everything below writes the same nsclient.tmp and
  // then renames it over nsclient.db, and the store lock taken inside is
  // shared, so it does not keep two savers apart. A plugin can ask for a
  // save from its own thread (the put `flush` flag), so this is a real race
  // rather than a hypothetical one.
  boost::lock_guard<boost::mutex> saveLock(save_mutex_);
  try {
    {
      // Shared rather than exclusive: this only reads storage_ (and
      // has_changed_), so concurrent get()s are fine - it is a concurrent
      // put() rebalancing the map mid-iteration that has to be excluded.
      boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
      if (!readLock.owns_lock()) {
        LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex, storage not saved.");
        return;
      }
      if (!has_changed_) {
        return;
      }
      std::string file = get_tmp_name();
      std::string path = file_helpers::meta::get_path(file);
      if (!file_helpers::checks::is_file(path)) {
        boost::filesystem::create_directories(path);
      }
      std::ofstream out(file.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);

      typedef std::shared_ptr<google::protobuf::io::ZeroCopyOutputStream> istr_type;
      typedef std::shared_ptr<google::protobuf::io::CodedOutputStream> codedstr_type;
      istr_type raw_out = istr_type(new ::google::protobuf::io::OstreamOutputStream(&out));
      codedstr_type coded_out = codedstr_type(new ::google::protobuf::io::CodedOutputStream(raw_out.get()));

      ::PB::Storage::Storage::File header;
      header.set_version(file_version);
      header.set_entries(storage_.size());
      if (!write_chunk<>(*coded_out, header)) {
        LOG_ERROR_CORE("Failed to write header to storage.");
        return;
      }

      for (const storage_type::value_type &v : storage_) {
        ::PB::Storage::Storage::Block block;
        block.set_owner(v.second.owner);
        block.mutable_entry()->CopyFrom(v.second.entry);
        if (!write_chunk(*coded_out, block)) {
          LOG_ERROR_CORE("Failed to write block to storage.");
          return;
        }
      }
    }
    boost::filesystem::rename(get_tmp_name(), get_filename());
  } catch (const std::exception &e) {
    LOG_ERROR_CORE("Failed to save storage: " + utf8::utf8_from_native(e.what()));
  } catch (...) {
    LOG_ERROR_CORE("Failed to save storage: UNKNOWN EXCEPTION");
  }
}

std::string nsclient::core::storage_manager::get_filename() const { return path_->expand_path("${data-path}/nsclient.db"); }
std::string nsclient::core::storage_manager::get_tmp_name() const { return path_->expand_path("${data-path}/nsclient.tmp"); }
