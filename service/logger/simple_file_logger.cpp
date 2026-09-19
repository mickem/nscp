// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "simple_file_logger.hpp"

#include <boost/filesystem.hpp>
#include <file_helpers.hpp>
#include <nscapi/protobuf/log.hpp>
#include <nscapi/settings/helper.hpp>
#include <nsclient/logger/logger_helper.hpp>
#include <nscp/path_defaults.hpp>
#include <str/format.hpp>
#include <vector>

#include "config.h"

#ifdef WIN32
#include <win/windows.hpp>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "../libs/settings_manager/settings_manager_impl.h"
#include "win/shellapi.hpp"

namespace nsclient {
namespace logging {
namespace impl {

namespace sh = nscapi::settings_helper;

simple_file_logger::simple_file_logger(std::string file) : max_size_(0), format_("%Y-%m-%d %H:%M:%S") {
  // operator/, not string concatenation: base_path() is the installation
  // directory with no trailing separator, so "+" produced
  // "<install dir>nsclient.log" on Windows. On unix base_path() is empty and
  // the join is a no-op, which is the behaviour this bootstrap logger has
  // always had there.
  file_ = (boost::filesystem::path(base_path()) / file).string();
}
std::string simple_file_logger::base_path() {
#ifdef WIN32
  return shellapi::get_module_file_name().string();
#else
  return "";
#endif
}

static bool reported_log_failure = false;
static bool reported_mkdir_failure = false;

// Create the log file ourselves, with an explicit mode, before the append
// stream below gets to it. An ofstream creates 0666 & ~umask, i.e. 0644 under
// the default umask - and a debug log echoes check arguments and settings
// paths, which is the same disclosure as the configuration file the packages
// now ship 0640. O_EXCL means this only ever applies to a file we create: a
// mode an operator set on an existing log is theirs to keep. Failures are
// ignored on purpose - the stream below reports them, once, with a message
// that names the file.
static void create_log_file_with_mode(const std::string &file) {
#ifndef WIN32
  const int fd = ::open(file.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0640);
  if (fd >= 0) ::close(fd);
#else
  // Windows inherits the directory's ACL, which the installer sets.
  (void)file;
#endif
}

void simple_file_logger::do_log(const std::string data) {
  if (file_.empty()) return;
  try {
    if (max_size_ != 0 && boost::filesystem::exists(file_.c_str()) && boost::filesystem::file_size(file_.c_str()) > max_size_) {
      std::streamsize target_size = static_cast<int>(max_size_ * 0.7);
      std::vector<char> tmpBuffer(static_cast<std::size_t>(target_size) + 1);
      try {
        std::ifstream ifs(file_.c_str());
        ifs.seekg(-target_size, std::ios_base::end);
        ifs.read(tmpBuffer.data(), target_size);
        ifs.close();
        std::ofstream ofs(file_.c_str(), std::ios::trunc);
        ofs.write(tmpBuffer.data(), target_size);
      } catch (...) {
        logger_helper::log_fatal("Failed to truncate log file: " + file_);
      }
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
        const auto &msg = message.entry(i);
        // Strip control characters from every field that originated outside
        // this process (file, message). Without this, a plugin / remote
        // command whose output contains "\n<forged log line>\n" would inject
        // arbitrary lines into the agent's log file - confusing audit and
        // breaking log shippers that key on line position.
        const std::string safe_file = str::format::strip_ctrl_chars(msg.file());
        const std::string safe_message = str::format::strip_ctrl_chars(msg.message());
        tmp << date << (": ") << utf8::cvt<std::string>(logger_helper::render_log_level_long(msg.level())) << (":") << safe_file << (":") << msg.line()
            << (": ") << safe_message << "\n";
      }
      try {
        if (!boost::filesystem::exists(file_.c_str())) create_log_file_with_mode(file_);
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
    const std::string configured = settings_manager::get_proxy()->expand_path(config.file);
    // `none` switches file logging off. Tested before anything joins a
    // directory onto it: the bare-name branch below prepends base_path(),
    // which is non-empty on Windows, so the sentinel used to be mangled into a
    // real file called "<install dir>none" and logging stayed on there.
    if (nscp::paths::is_no_path(configured)) {
      file_ = "";
      return;
    }
    file_ = configured;
    if (file_.empty()) file_ = "nsclient.log";
    if (file_.find('\\') == std::string::npos && file_.find('/') == std::string::npos) {
      // A bare file name is taken relative to the installation directory, not
      // to whatever the working directory happens to be (System32 for a
      // Windows service). operator/ supplies the separator - the string
      // concatenation this replaces produced "<install dir>nsclient.log".
      file_ = (boost::filesystem::path(base_path()) / file_).string();
    }
  } catch (const std::exception &) {
    // ignored, since this might be after shutdown...
  } catch (...) {
    // ignored, since this might be after shutdown...
  }
}
}  // namespace impl
}  // namespace logging
}  // namespace nsclient