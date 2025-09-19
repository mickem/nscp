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

#include "CommandClient.h"

#include <boost/algorithm/string.hpp>
#include <boost/thread.hpp>
#include <iostream>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_protobuf_command.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_protobuf_log.hpp>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN  // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#pragma warning(disable : 4100)
#else
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

void client_handler::output_message(const std::string &msg) {
  std::string msg_copy = msg;
  std::size_t p = msg_copy.find_last_not_of(" \t\n\r");
  if (p != std::string::npos) {
    msg_copy = msg_copy.substr(0, p + 1);
  }
  if (msg_copy.find("\n") == std::string::npos) {
    NSC_LOG_MESSAGE(msg_copy);
  } else {
#ifdef WIN32
    NSC_LOG_MESSAGE(boost::replace_all_copy(msg_copy, "\r", "\t... \r"));
#else
    NSC_LOG_MESSAGE(boost::replace_all_copy(msg_copy, "\n", "\t... \n"));
#endif
  }
}

void client_handler::log_debug(std::string module, std::string file, int line, std::string msg) const {
  if (get_core()->should_log(NSCAPI::log_level::debug)) {
    get_core()->log(NSCAPI::log_level::debug, file, line, msg);
  }
}

void client_handler::log_error(std::string module, std::string file, int line, std::string msg) const {
  if (get_core()->should_log(NSCAPI::log_level::debug)) {
    get_core()->log(NSCAPI::log_level::error, file, line, msg);
  }
}

bool CommandClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
  client::cli_handler_ptr handler(new client_handler(get_core(), get_id()));
  client.reset(new client::cli_client(handler));
  return true;
}

bool CommandClient::unloadModule() { return true; }

void CommandClient::handleLogMessage(const PB::Log::LogEntry::Entry &message) {
  /*
  using namespace boost::posix_time;
  using namespace boost::gregorian;

  error_handler::log_entry entry;
  entry.line = message.line();
  entry.file = message.file();
  entry.message = message.message();
  entry.date = to_simple_string(second_clock::local_time());

  switch (message.level()) {
  case PB::Log::LogEntry_Entry_Level_LOG_CRITICAL:
          entry.type = "critical";
          break;
  case PB::Log::LogEntry_Entry_Level_LOG_DEBUG:
          entry.type = "debug";
          break;
  case PB::Log::LogEntry_Entry_Level_LOG_ERROR:
          entry.type = "error";
          break;
  case PB::Log::LogEntry_Entry_Level_LOG_INFO:
          entry.type = "info";
          break;
  case PB::Log::LogEntry_Entry_Level_LOG_WARNING:
          entry.type = "warning";
          break;
  default:
          entry.type = "unknown";
  }
  log_data.add_message(message.level() == PB::Log::LogEntry_Entry_Level_LOG_CRITICAL || message.level() == PB::Log::LogEntry_Entry_Level_LOG_ERROR, entry);
  */
}
bool is_running = false;
boost::thread input_thread;

#ifdef WIN32
BOOL WINAPI consoleHandler(DWORD signal) {
  if (signal == CTRL_C_EVENT) {
    is_running = false;
    NSC_LOG_MESSAGE("Ctrl+c is not exit...");
  }
  return TRUE;
}
#endif

bool input_available() {
#ifdef WIN32
  HANDLE input_handle = GetStdHandle(STD_INPUT_HANDLE);
  DWORD num_events = 0;
  if (!GetNumberOfConsoleInputEvents(input_handle, &num_events)) {
    return false;
  }

  if (num_events > 0) {
    INPUT_RECORD buffer[128];
    DWORD events_read = 0;
    if (PeekConsoleInput(input_handle, buffer, 128, &events_read) && events_read > 0) {
      for (DWORD i = 0; i < events_read; ++i) {
        // TODO: Maybe we need to remove control keys
        if (buffer[i].EventType == KEY_EVENT && buffer[i].Event.KeyEvent.bKeyDown) {
          return true;
        }
      }
    }
  }
  return false;
#else
  struct timeval tv;
  fd_set fds;
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  FD_ZERO(&fds);
  FD_SET(STDIN_FILENO, &fds);
  select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
  return (FD_ISSET(STDIN_FILENO, &fds));
#endif
}

void CommandClient::read_input_thread() const {
  is_running = true;
  while (is_running) {
    if (input_available()) {
      std::string s;
      std::getline(std::cin, s);
      if (s == "exit") {
        is_running = false;
      } else {
        client->handle_command(utf8::utf8_from_native(s));
      }
    } else {
      boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    }
  }
}

bool CommandClient::commandLineExec(const int target_mode, const PB::Commands::ExecuteRequestMessage::Request &request,
                                    PB::Commands::ExecuteResponseMessage::Response *response, const PB::Commands::ExecuteRequestMessage &request_message) {
  if (request.command() == "exit") {
    if (is_running == false) {
      nscapi::protobuf::functions::set_response_bad(*response, "Command client is not running (this only works in test mode)!");
      return true;
    }
    is_running = false;
    NSC_LOG_MESSAGE("Command client shutdown was requested!");
    nscapi::protobuf::functions::set_response_good(*response, "Shutdown requested");
    return true;
  }
  if (is_running) {
    NSC_LOG_ERROR("Command client is already running!");
  }

#ifdef WIN32
  if (!SetConsoleCtrlHandler(consoleHandler, TRUE)) {
    NSC_LOG_MESSAGE("Could not set control handler");
  }
#endif
  // 	if (core_->get_service_control().is_started())
  // 		info(__LINE__, "Service seems to be started (Sockets and such will probably not work)...");

  input_thread = boost::thread([this]() { this->read_input_thread(); });

  NSC_DEBUG_MSG("Enter command to execute, help for help or exit to exit...");
  input_thread.join();
  nscapi::protobuf::functions::set_response_good(*response, "Done");
  return true;
}

void CommandClient::submitMetrics(const PB::Metrics::MetricsMessage &response) { client->push_metrics(response); }
