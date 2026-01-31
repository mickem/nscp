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

#include "simple_file_logger.hpp"

#include <boost/filesystem.hpp>
#include <file_helpers.hpp>
#include <nscapi/nscapi_protobuf_log.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <nsclient/logger/logger_helper.hpp>
#include <str/format.hpp>

#include "config.h"

#ifdef WIN32
#include <Windows.h>
#endif

#include "../libs/settings_manager/settings_manager_impl.h"
#include "win/shellapi.hpp"

namespace nsclient {
namespace logging {
namespace impl {

namespace sh = nscapi::settings_helper;

simple_file_logger::simple_file_logger(std::string file) : max_size_(0), format_("%Y-%m-%d %H:%M:%S") { file_ = base_path() + file; }
std::string simple_file_logger::base_path() {
#ifdef WIN32
  return shellapi::get_module_file_name().string();
#else
  return "";
#endif
}

static bool reported_log_failure = false;
static bool reported_mkdir_failure = false;

void simple_file_logger::do_log(const std::string data) {
  if (file_.empty()) return;
  try {
    if (max_size_ != 0 && boost::filesystem::exists(file_.c_str()) && boost::filesystem::file_size(file_.c_str()) > max_size_) {
      int target_size = static_cast<int>(max_size_ * 0.7);
      char *tmpBuffer = new char[target_size + 1];
      try {
        std::ifstream ifs(file_.c_str());
        ifs.seekg(-target_size, std::ios_base::end);
        ifs.read(tmpBuffer, target_size);
        ifs.close();
        std::ofstream ofs(file_.c_str(), std::ios::trunc);
        ofs.write(tmpBuffer, target_size);
      } catch (...) {
        logger_helper::log_fatal("Failed to truncate log file: " + file_);
      }
      delete[] tmpBuffer;
    }
    if (!boost::filesystem::exists(file_.c_str())) {
      boost::filesystem::path parent = file_helpers::meta::get_path(file_);
      if (!parent.empty()) {
        if (!boost::filesystem::exists(parent.string())) {
          try {
            boost::filesystem::create_directories(parent);
          } catch (...) {
            if (!reported_mkdir_failure) {
              reported_mkdir_failure = true;
              logger_helper::log_fatal("Failed to create log directory: " + parent.string());
            }
          }
        }
      }
    }
    std::string date = logger_helper::get_formated_date(format_);

    PB::Log::LogEntry message;
    if (!message.ParseFromString(data)) {
      logger_helper::log_fatal("Failed to parse message: " + str::format::strip_ctrl_chars(data));
    } else {
      std::stringstream tmp;
      for (int i = 0; i < message.entry_size(); i++) {
        PB::Log::LogEntry::Entry msg = message.entry(i);
        tmp << date << (": ") << utf8::cvt<std::string>(logger_helper::render_log_level_long(msg.level())) << (":") << msg.file() << (":") << msg.line()
            << (": ") << msg.message() << "\n";
      }
      try {
        std::ofstream stream(file_.c_str(), std::ios::out | std::ios::app | std::ios::ate);
        if (!stream) {
          if (!reported_log_failure) {
            reported_log_failure = true;
            logger_helper::log_fatal("Failed to open log file: " + file_);
          }
          logger_helper::log_fatal(tmp.str());
        } else {
          stream << tmp.str();
        }
      } catch (std::exception &e) {
        logger_helper::log_fatal("Failed to write log: " + tmp.str() + ": " + e.what());
      }
    }
  } catch (std::exception &e) {
    logger_helper::log_fatal("Failed to parse data from: " + str::format::strip_ctrl_chars(data) + ": " + e.what());
  } catch (...) {
    logger_helper::log_fatal("Failed to parse data from: " + str::format::strip_ctrl_chars(data));
  }
}

simple_file_logger::config_data simple_file_logger::do_config(const bool log_fault) {
  config_data ret;
  try {
    sh::settings_registry settings(settings_manager::get_proxy());
    settings.set_alias("log/file");

    settings.add_path_to_settings()

        ("log/file", "Logfile", "Configure log file properties.");

    settings.add_key_to_settings("log")
        .add_file("file name", sh::string_key(&ret.file, DEFAULT_LOG_LOCATION), "Log file name",
                  "The file to write log data to. Set this to none to disable log to file.")

        .add_string("date format", sh::string_key(&ret.format, "%Y-%m-%d %H:%M:%S"), "Date format",
                    "The size of the buffer to use when getting messages this affects the speed and maximum size of messages you can receive.");

    settings.add_key_to_settings("log/file")
        .add_int("max size", sh::size_key(&ret.max_size, 0), "Maximum file size",
                 "When file size reaches this it will be truncated to 50% if set to 0 (default) truncation will be disabled");

    settings.register_all();
    settings.notify();

#ifdef WIN32
    if (ret.file == "/nsclient.log") ret.file = "${exe-path}/nsclient.log";
#endif
    ret.file = settings.expand_path(ret.file);
  } catch (const std::exception &e) {
    if (log_fault) logger_helper::log_fatal(std::string("Failed to configure logger: ") + e.what());
  } catch (...) {
    if (log_fault) logger_helper::log_fatal("Failed to configure logging.");
  }
  return ret;
}
void simple_file_logger::synch_configure() { do_config(true); }

void simple_file_logger::asynch_configure() {
  try {
    config_data config = do_config(false);

    format_ = config.format;
    max_size_ = config.max_size;
    file_ = settings_manager::get_proxy()->expand_path(config.file);
    if (file_.empty()) file_ = base_path() + "nsclient.log";
    if (file_.find('\\') == std::string::npos && file_.find('/') == std::string::npos) {
      file_ = base_path() + file_;
    }
    if (file_ == "none") {
      file_ = "";
    }
  } catch (const std::exception &_e) {
    // ignored, since this might be after shutdown...
  } catch (...) {
    // ignored, since this might be after shutdown...
  }
}
}  // namespace impl
}  // namespace logging
}  // namespace nsclient