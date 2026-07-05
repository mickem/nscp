// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/thread.hpp>
#include <nscapi/plugin.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/log.hpp>
#include <string>

class CheckNSCP : public nscapi::impl::simple_plugin {
  boost::timed_mutex mutex_;
  boost::filesystem::path crashFolder;
  std::string last_error_;
  unsigned int error_count_;
  boost::posix_time::ptime start_;

  // Configuration for check_nscp_update.
  // Cache lifetime for the GitHub release lookup (in hours).
  unsigned int update_cache_hours_;
  // When true, GitHub pre-releases ("experimental" builds) are considered
  // when determining the latest available version. Defaults to false so that
  // only stable releases trigger an "update available" result.
  bool update_check_experimental_;
  // URL of the GitHub releases API endpoint (configurable so installations
  // behind a proxy or air-gap can point at a mirror).
  std::string update_url_;
  // TLS configuration for the GitHub release lookup. Defaults are chosen so
  // the update check validates the server certificate against the system CA
  // bundle and rejects anything below TLS 1.2; admins can override to point
  // at a private CA bundle when going through a TLS-intercepting proxy.
  std::string update_tls_version_;
  std::string update_verify_mode_;
  std::string update_ca_;

  // Cached latest version lookup. Guarded by update_mutex_.
  boost::timed_mutex update_mutex_;
  boost::posix_time::ptime update_cached_at_;
  bool update_cache_valid_;
  std::string update_cached_version_;
  std::string update_cached_tag_;
  std::string update_cached_url_;
  std::string update_cached_published_;
  std::string update_cached_error_;

 public:
  CheckNSCP() : simple_plugin(), error_count_(0), update_cache_hours_(24), update_check_experimental_(false), update_cache_valid_(false) {}

  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  void check_nscp(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_nscp_version(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) const;
  void check_nscp_update(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void handleLogMessage(const PB::Log::LogEntry::Entry &message);

  std::size_t get_errors(std::string &last_error);
};