/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "zip_plugin.h"

#ifdef MINIZ
#include <miniz.h>
#else
#include <zip.h>

#include <cstdio>
#include <vector>
#endif

#include <boost/json.hpp>
#include <file_helpers.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <str/nscp_string.hpp>

#include "NSCAPI.h"

namespace json = boost::json;

template <class T>
void debug_log_list(nsclient::logging::logger_instance logger, const char *file, const int line, const T &list, const std::string prefix) {
  if (!logger || !logger->should_debug()) {
    return;
  }
  for (const std::string &s : list) {
    logger->debug("core", file, line, prefix + s);
  }
}

// Backend-neutral file metadata returned by zip_archive::file_stat. Keeps the
// caller free of miniz/libzip-specific types.
struct archive_file_stat {
  std::string filename;
  std::size_t uncompressed_size = 0;
};

#ifdef _WIN32
struct zip_archive {
  mz_zip_archive handle_;
  zip_archive() { memset(&handle_, 0, sizeof(handle_)); }
  zip_archive(const std::string &file) {
    memset(&handle_, 0, sizeof(handle_));
    read(file);
  }
  ~zip_archive() { mz_zip_reader_end(&handle_); }

  bool read(const std::string &file) { return mz_zip_reader_init_file(&handle_, file.c_str(), 0); }
  unsigned int get_numfiles() { return mz_zip_reader_get_num_files(&handle_); }
  bool file_stat(unsigned int id, archive_file_stat &out) {
    mz_zip_archive_file_stat st;
    if (!mz_zip_reader_file_stat(&handle_, id, &st)) return false;
    out.filename = st.m_filename;
    out.uncompressed_size = static_cast<std::size_t>(st.m_uncomp_size);
    return true;
  }
  // Caller owns the returned buffer and must mz_free() it. Currently this
  // matches the pre-existing miniz behaviour (the metadata reader leaks the
  // pointer; out of scope for the libzip port).
  const char *extract_file_to_heap(const char *filename, std::size_t &size) {
    return static_cast<char *>(mz_zip_reader_extract_file_to_heap(&handle_, filename, &size, 0));
  }
  bool extract_file_to_file(const char *filename, const char *dst_file) { return mz_zip_reader_extract_file_to_file(&handle_, filename, dst_file, 0); }
};
#else
// libzip-backed implementation. Used on Linux where miniz is not packaged but
// libzip-dev is a first-class Debian/Ubuntu/Rocky package.
struct zip_archive {
  zip_t *handle_ = nullptr;
  zip_archive() = default;
  zip_archive(const std::string &file) { read(file); }
  ~zip_archive() {
    if (handle_) zip_close(handle_);
  }

  bool read(const std::string &file) {
    if (handle_) {
      zip_close(handle_);
      handle_ = nullptr;
    }
    int err = 0;
    handle_ = zip_open(file.c_str(), ZIP_RDONLY, &err);
    return handle_ != nullptr;
  }
  unsigned int get_numfiles() {
    if (!handle_) return 0;
    const zip_int64_t n = zip_get_num_entries(handle_, 0);
    return n < 0 ? 0u : static_cast<unsigned int>(n);
  }
  bool file_stat(unsigned int id, archive_file_stat &out) {
    if (!handle_) return false;
    zip_stat_t st;
    zip_stat_init(&st);
    if (zip_stat_index(handle_, id, 0, &st) != 0) return false;
    out.filename = st.name ? st.name : "";
    out.uncompressed_size = static_cast<std::size_t>(st.size);
    return true;
  }
  // Caller owns the returned buffer (malloc-allocated) and must free() it.
  // Mirrors the miniz contract so call sites stay portable.
  const char *extract_file_to_heap(const char *filename, std::size_t &size) {
    size = 0;
    if (!handle_) return nullptr;
    zip_stat_t st;
    zip_stat_init(&st);
    if (zip_stat(handle_, filename, 0, &st) != 0) return nullptr;
    zip_file_t *zf = zip_fopen(handle_, filename, 0);
    if (!zf) return nullptr;
    const std::size_t need = static_cast<std::size_t>(st.size);
    char *buf = static_cast<char *>(std::malloc(need));
    if (!buf) {
      zip_fclose(zf);
      return nullptr;
    }
    const zip_int64_t got = zip_fread(zf, buf, need);
    zip_fclose(zf);
    if (got < 0 || static_cast<std::size_t>(got) != need) {
      std::free(buf);
      return nullptr;
    }
    size = need;
    return buf;
  }
  bool extract_file_to_file(const char *filename, const char *dst_file) {
    if (!handle_) return false;
    zip_file_t *zf = zip_fopen(handle_, filename, 0);
    if (!zf) return false;
    FILE *out = std::fopen(dst_file, "wb");
    if (!out) {
      zip_fclose(zf);
      return false;
    }
    std::vector<char> buf(64 * 1024);
    zip_int64_t got = 0;
    bool ok = true;
    while ((got = zip_fread(zf, buf.data(), buf.size())) > 0) {
      if (std::fwrite(buf.data(), 1, static_cast<std::size_t>(got), out) != static_cast<std::size_t>(got)) {
        ok = false;
        break;
      }
    }
    if (got < 0) ok = false;
    std::fclose(out);
    zip_fclose(zf);
    return ok;
  }
};
#endif

/**
 * Default c-tor
 * Initializes the plug in name but does not load the actual plug in.<br>
 * To load the plug in use function load() that loads an initializes the plug in.
 *
 * @param file The file (DLL) to load as a NSC plug in.
 */
nsclient::core::zip_plugin::zip_plugin(const unsigned int id, const boost::filesystem::path &file, const std::string &alias,
                                       const nsclient::core::path_instance &paths, const nsclient::core::plugin_mgr_instance &plugins,
                                       const nsclient::logging::logger_instance &logger)
    : plugin_interface(id, alias), file_(file), paths_(paths), plugins_(plugins), logger_(logger) {
  read_metadata();
}
/**
 * Default d-tor
 */
nsclient::core::zip_plugin::~zip_plugin() {}
/**
 * Returns the name of the plug in.
 *
 * @return Name of the plug in.
 *
 * @throws NSPluginException if the module is not loaded.
 */
std::string nsclient::core::zip_plugin::getName() { return name_; }
std::string nsclient::core::zip_plugin::getDescription() { return description_; }

nsclient::core::script_def read_script_def(const json::value &s) {
  nsclient::core::script_def def;
  if (s.is_string()) {
    def.script = s.as_string().c_str();
    std::string name = file_helpers::meta::get_filename(boost::filesystem::path(def.script));
    if (boost::algorithm::ends_with(name, ".py")) {
      def.provider = "PythonScript";
      def.alias = name.substr(0, name.length() - 3);
      def.command = name;
    } else {
      def.provider = "CheckExternalScripts";
      def.alias = def.script;
      def.command = def.script;
    }
  } else {
    auto o = s.as_object();
    def.provider = o["provider"].as_string().c_str();
    def.script = o["script"].as_string().c_str();
    def.alias = o["alias"].as_string().c_str();
    def.command = o["command"].as_string().c_str();
  }
  return def;
}

void nsclient::core::zip_plugin::read_metadata() {
  zip_archive archive;
  if (!archive.read(file_.string())) {
    throw plugin_exception(get_alias_or_name(), "Failed to read:" + file_.string());
  }

  for (unsigned int i = 0; i < archive.get_numfiles(); i++) {
    archive_file_stat file_stat;
    if (!archive.file_stat(i, file_stat)) {
      throw plugin_exception(get_alias_or_name(), "Failed to read:" + file_.string());
    }

    if (file_stat.filename == "module.json") {
      std::size_t uncomp_size;
      const char *p = archive.extract_file_to_heap(file_stat.filename.c_str(), uncomp_size);
      if (!p) {
        throw plugin_exception(get_alias_or_name(), "Failed to read:" + file_.string());
      }
      read_metadata(std::string(p));
      return;
    }
  }
  throw plugin_exception(get_alias_or_name(), "Failed to find module.json in " + file_.string());
}
void nsclient::core::zip_plugin::read_metadata(const std::string &data) {
  try {
    auto root = json::parse(data).as_object();
    name_ = root["name"].as_string().c_str();
    description_ = root["description"].as_string().c_str();

    if (root.contains("scripts")) {
      for (const auto &s : root["scripts"].as_array()) {
        script_def def = read_script_def(s);
        if (modules_.find(def.provider) == modules_.end()) {
          modules_.insert(def.provider);
        }
        scripts_.push_back(def);
      }
    }
    if (root.contains("modules")) {
      for (const auto &s : root["modules"].as_array()) {
        std::string module = s.as_string().c_str();
        if (modules_.find(module) == modules_.end()) {
          modules_.insert(module);
        }
      }
    }
    if (root.contains("on_start")) {
      for (const auto &s : root["on_start"].as_array()) {
        on_start_.push_back(s.as_string().c_str());
      }
    }
  } catch (const std::exception &e) {
    throw plugin_exception(get_alias_or_name(), "Failed to parse module.json " + static_cast<std::string>(e.what()));
  }
}

bool nsclient::core::zip_plugin::load_plugin(NSCAPI::moduleLoadMode) {
  const boost::filesystem::path scripts_folder = boost::filesystem::path(paths_->expand_path("${scripts}")) / "tmp";
  const boost::filesystem::path target_path = scripts_folder / getModule();
  boost::filesystem::create_directory(scripts_folder);
  boost::filesystem::create_directory(target_path);
  for (const std::string &plugin : modules_) {
    plugins_->load_single_plugin(plugin, "", true);
  }
  zip_archive archive(file_.string());

  for (const script_def &script : scripts_) {
    boost::filesystem::path target = target_path / file_helpers::meta::get_filename(boost::filesystem::path(script.script));
    if (!archive.extract_file_to_file(script.script.c_str(), target.string().c_str())) {
      LOG_ERROR_CORE("Failed to add script " + script.script);
      continue;
    }
    std::list<std::string> ret;
    std::vector<std::string> args;
    args.push_back("--script");
    args.push_back(target.string());
    args.push_back("--alias");
    args.push_back(script.alias);
    args.push_back("--no-config");
    plugins_->simple_exec(script.provider + ".add", args, ret);
    debug_log_list(get_logger(), __FILE__, __LINE__, ret, " : ");
  }
  for (const std::string &cmd : on_start_) {
    std::list<std::string> ret;
    std::vector<std::string> args;
    try {
      strEx::s::parse_command(cmd, args);
    } catch (const std::exception &e) {
      LOG_ERROR_CORE("Failed to parse \"" + cmd + "\": " + utf8::utf8_from_native(e.what()));
      continue;
    }

    std::string command = args.front();
    args.erase(args.begin());
    plugins_->simple_exec(command, args, ret);
    debug_log_list(get_logger(), __FILE__, __LINE__, ret, " : ");
  }

  return true;
}

/**
 * Unload the plug in
 * @throws NSPluginException if the module is not loaded and/or cannot be unloaded (plug in remains loaded if so).
 */
void nsclient::core::zip_plugin::unload_plugin() {
  const boost::filesystem::path scripts_folder = boost::filesystem::path(paths_->expand_path("${scripts}")) / "tmp";
  const boost::filesystem::path target_path = scripts_folder / getModule();
  for (const script_def &script : scripts_) {
    boost::filesystem::path target = target_path / file_helpers::meta::get_filename(boost::filesystem::path(script.script));
    boost::filesystem::remove(target);
  }
  boost::filesystem::remove(target_path);
  boost::filesystem::remove(scripts_folder);
}

NSCAPI::nagiosReturn nsclient::core::zip_plugin::handleCommand(const std::string reuest, std::string &) {
  throw plugin_exception(get_alias_or_name(), "cannot handle commands");
}

NSCAPI::nagiosReturn nsclient::core::zip_plugin::handle_schedule(const std::string &) { throw plugin_exception(get_alias_or_name(), "cannot handle schedule"); }

NSCAPI::nagiosReturn nsclient::core::zip_plugin::handleNotification(const char *, std::string &, std::string &) {
  throw plugin_exception(get_alias_or_name(), "cannot handle commands");
}

NSCAPI::nagiosReturn nsclient::core::zip_plugin::on_event(const std::string &) { throw plugin_exception(get_alias_or_name(), "cannot handle commands"); }

NSCAPI::nagiosReturn nsclient::core::zip_plugin::fetchMetrics(std::string &) { throw plugin_exception(get_alias_or_name(), "cannot handle commands"); }

NSCAPI::nagiosReturn nsclient::core::zip_plugin::submitMetrics(const std::string &) { throw plugin_exception(get_alias_or_name(), "cannot handle commands"); }

void nsclient::core::zip_plugin::handleMessage(const char *, unsigned int) { throw plugin_exception(get_alias_or_name(), "cannot handle commands"); }

int nsclient::core::zip_plugin::commandLineExec(bool, std::string &, std::string &) { throw plugin_exception(get_alias_or_name(), "cannot handle commands"); }
bool nsclient::core::zip_plugin::is_duplicate(boost::filesystem::path, std::string) { return false; }

std::string nsclient::core::zip_plugin::get_version() { return "1.0.0"; }

bool nsclient::core::zip_plugin::route_message(const char *, const char *, unsigned int, char **, char **, unsigned int *) {
  throw plugin_exception(get_alias_or_name(), "cannot handle commands");
}

std::string nsclient::core::zip_plugin::getModule() {
  std::string tmp = file_helpers::meta::get_filename(file_);
  if (boost::algorithm::ends_with(tmp, ".zip")) {
    return tmp.substr(0, tmp.length() - 4);
  }
  return tmp;
}
