// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <memory>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>

#include "bookmarks.hpp"
#include "filter.hpp"

struct real_time_thread;
class CheckEventLog : public nscapi::impl::simple_plugin {
 private:
  std::shared_ptr<real_time_thread> thread_;
  bool debug_;
  std::string syntax_;
  int buffer_length_;
  bool lookup_names_;
  bookmarks bookmarks_;

 public:
  CheckEventLog() {}
  virtual ~CheckEventLog() {}
  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();
  void parse(std::wstring expr);

  void check_eventlog(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void CheckEventLog_(PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

  bool commandLineExec(const int target_mode, const PB::Commands::ExecuteRequestMessage::Request &request,
                       PB::Commands::ExecuteResponseMessage::Response *response, const PB::Commands::ExecuteRequestMessage &request_message);
  void insert_eventlog(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response);
  void list_providers(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response);
  void add_filter(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response);

 private:
  // Render an already-positioned bookmark handle to XML and persist it under the
  // given key. The caller advances the bookmark (via EvtUpdateBookmark) to the
  // last event it read before calling this.
  void store_bookmark(const std::string &bookmark, eventlog::api::EVT_HANDLE hBookmark);
  void check_modern(const std::string &logfile, const std::string &scan_range, const int truncate_message, eventlog_filter::filter &filter,
                    std::string bookmark);
};
