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

#include <boost/json.hpp>
#include <bytes/unzip.hpp>
#include <file_helpers.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <str/nscp_string.hpp>
#include <vector>

#include "NSCAPI.h"

namespace json = boost::json;

template <class T>
void debug_log_list(const nsclient::logging::logger_instance &logger, const char *file, const int line, const T &list, const std::string &prefix) {
  if (!logger || !logger->should_debug()) {
    return;
  }
  for (const std::string &s : list) {
    logger->debug("core", file, line, prefix + s);
  }
}

/**
 * Default c-tor
 * Initializes the plug in name but does not load the actual plug in.<br>
 * To load the plug in use function load() that loads an initializes the plug in.
 *
 * @param file The file (DLL) to load as a NSC plug in.
 */
nsclient::core::zip_plugin::zip_plugin(const unsigned int id, const boost::filesystem::path &file, const std::string &alias, const path_instance &paths,
                                       const plugin_mgr_instance &plugins, const logging::logger_instance &logger)
    : plugin_interface(id, alias), file_(file), paths_(paths), plugins_(plugins), logger_(logger) {
  read_metadata();
}
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
    const std::string name = file_helpers::meta::get_filename(boost::filesystem::path(def.script));
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
  bytes::unzip::reader archive;
  if (!archive.open(file_.string())) {
    throw plugin_exception(get_alias_or_name(), "Failed to read:" + file_.string());
  }

  for (unsigned int i = 0; i < archive.size(); i++) {
    bytes::unzip::file_entry file_stat;
    if (!archive.stat(i, file_stat)) {
      throw plugin_exception(get_alias_or_name(), "Failed to read:" + file_.string());
    }

    if (file_stat.filename == "module.json") {
      std::string data;
      if (!archive.extract(file_stat.filename, data)) {
        throw plugin_exception(get_alias_or_name(), "Failed to read:" + file_.string());
      }
      read_metadata(data);
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
        on_start_.emplace_back(s.as_string().c_str());
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
  const bytes::unzip::reader archive(file_.string());

  for (const script_def &script : scripts_) {
    boost::filesystem::path target = target_path / file_helpers::meta::get_filename(boost::filesystem::path(script.script));
    if (!archive.extract_to_file(script.script, target.string())) {
      LOG_ERROR_CORE("Failed to add script " + script.script);
      continue;
    }
    std::list<std::string> ret;
    std::vector<std::string> args;
    args.emplace_back("--script");
    args.push_back(target.string());
    args.emplace_back("--alias");
    args.push_back(script.alias);
    args.emplace_back("--no-config");
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

    const std::string command = args.front();
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

NSCAPI::nagiosReturn nsclient::core::zip_plugin::handleCommand(const std::string request, std::string &) {
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
