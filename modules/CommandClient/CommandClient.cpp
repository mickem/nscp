// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CommandClient.h"

#include <atomic>
#include <boost/algorithm/string.hpp>
#include <boost/thread.hpp>
#include <iostream>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <nscapi/protobuf/log.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <str/utf8.hpp>

#include "console_editor.hpp"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN  // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#pragma warning(disable : 4100)
#else
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <csignal>
#include <string>
#endif

namespace sh = nscapi::settings_helper;

// Atomic so the Windows console handler, the POSIX signal_set callback,
// the stdin-driven read loops and the `exit` command path all
// see a consistent value without surprising the compiler. Lock-free on
// every platform we care about.
std::atomic<bool> is_running{false};
boost::thread input_thread;

namespace {
// The prompt currently drawing the console, if any. Held behind a mutex and
// handed out as a shared_ptr because handleLogMessage runs on the core's
// logging thread: a bare pointer cleared on shutdown would still let a log
// message in flight write into a half-destroyed editor.
boost::mutex editor_mutex;
std::shared_ptr<command_client::console_editor> active_editor;

std::shared_ptr<command_client::console_editor> get_editor() {
  boost::mutex::scoped_lock lock(editor_mutex);
  return active_editor;
}

void set_editor(std::shared_ptr<command_client::console_editor> editor) {
  boost::mutex::scoped_lock lock(editor_mutex);
  active_editor.swap(editor);
}

command_client::log_severity severity_of(const PB::Log::LogEntry::Entry::Level level) {
  switch (level) {
    case PB::Log::LogEntry_Entry_Level_LOG_CRITICAL:
      return command_client::log_severity::critical;
    case PB::Log::LogEntry_Entry_Level_LOG_ERROR:
      return command_client::log_severity::error;
    case PB::Log::LogEntry_Entry_Level_LOG_WARNING:
      return command_client::log_severity::warning;
    case PB::Log::LogEntry_Entry_Level_LOG_DEBUG:
      return command_client::log_severity::debug;
    case PB::Log::LogEntry_Entry_Level_LOG_TRACE:
      return command_client::log_severity::trace;
    default:
      return command_client::log_severity::info;
  }
}

// Commands after which the set of registered queries and loaded modules can
// have changed, so the prompt's completion and highlighting need re-reading.
bool changes_the_registry(const std::string &command) {
  static const char *verbs[] = {"load", "unload", "enable", "disable", "reload"};
  for (const char *verb : verbs) {
    const std::string prefix(verb);
    if (command == prefix || command.compare(0, prefix.size() + 1, prefix + " ") == 0) return true;
  }
  return false;
}
}  // namespace

void client_handler::output_message(const std::string &msg) {
  std::string msg_copy = msg;
  std::size_t p = msg_copy.find_last_not_of(" \t\n\r");
  if (p != std::string::npos) {
    msg_copy = msg_copy.substr(0, p + 1);
  }
  // With a prompt up, command output goes straight to the line editor: it
  // knows how to draw around the prompt, and multi-line output can stay
  // multi-line instead of being folded into a single log record with "..."
  // separators (which is all the fallback below can do).
  if (const std::shared_ptr<command_client::console_editor> editor = get_editor()) {
    editor->print(msg_copy + "\n");
    return;
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

  try {
    sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
    settings.set_alias(alias, "cli");

    settings.alias().add_path_to_settings()("CLI", "Section for the interactive command line client (nscp test).");

    settings.alias()
        .add_key_to_settings()
        .add_string("history file", sh::string_key(&history_file_, ""), "HISTORY FILE",
                    "Where to keep the interactive prompt's command history. Empty means the per-user default: "
                    "%APPDATA%\\NSClient++\\console-history.txt on Windows, $XDG_STATE_HOME/nscp/console-history or "
                    "~/.nscp_history on other platforms. History is only read and written when the prompt is attached "
                    "to a terminal.",
                    true)
        .add_int("history size", sh::int_key(&history_size_, 500), "HISTORY SIZE",
                 "How many commands to keep in the history file. Set to 0 to turn persistent history off entirely - "
                 "nothing is then read from or written to disk, which is what you want if commands typed at the prompt "
                 "may carry credentials.",
                 true)
        .add_bool("color", sh::bool_key(&color_, true), "COLOR",
                  "Colour the prompt, syntax highlight what is typed and colour log messages by severity. Turn this "
                  "off for a terminal that renders the colours badly.",
                  true);

    settings.register_all();
    settings.notify();
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("Failed to register settings: " + utf8::utf8_from_native(e.what()));
  } catch (...) {
    NSC_LOG_ERROR("Failed to register settings");
  }
  return true;
}

bool CommandClient::unloadModule() { return true; }

void CommandClient::handleLogMessage(const PB::Log::LogEntry::Entry &message) {
  // Only meaningful while we own the console: the core has been asked to stop
  // writing it (see commandLineExec), so this is now the only thing rendering
  // the log. Without a prompt up we do nothing and the core prints as usual.
  const std::shared_ptr<command_client::console_editor> editor = get_editor();
  if (!editor) return;
  editor->print_log(severity_of(message.level()), message.sender(), message.message(), message.file(), message.line());
}

#ifdef WIN32
BOOL WINAPI consoleHandler(DWORD signal) {
  if (signal == CTRL_C_EVENT) {
    is_running = false;
    if (const std::shared_ptr<command_client::console_editor> editor = get_editor()) editor->interrupt();
  }
  return TRUE;
}
#endif

bool input_available() {
#ifdef WIN32
  HANDLE input_handle = GetStdHandle(STD_INPUT_HANDLE);
  DWORD num_events = 0;
  if (!GetNumberOfConsoleInputEvents(input_handle, &num_events)) {
    // Not a console handle: stdin is a file, a pipe or NUL. Say yes and let
    // the caller do a blocking read, which either returns a line or reports
    // end of input (which the caller then parks on). Returning false here is
    // what used to make `nscp test < commands.txt` silently do nothing on
    // Windows while the same thing worked on POSIX.
    return true;
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

std::shared_ptr<command_client::console_editor> CommandClient::make_editor() const {
  std::shared_ptr<command_client::console_editor> editor(new command_client::console_editor());

  command_client::editor_hooks hooks;
  client::cli_client *cli = client.get();
  hooks.queries = [cli]() {
    std::vector<command_client::editor_item> items;
    for (const client::command_info &c : cli->list_queries()) items.push_back({c.name, c.description});
    return items;
  };
  hooks.modules = [cli]() {
    std::vector<command_client::editor_module> items;
    for (const client::module_info &m : cli->list_modules()) items.push_back({m.name, m.description, m.loaded, m.enabled});
    return items;
  };
  hooks.all_modules = [cli]() {
    std::vector<command_client::editor_module> items;
    for (const client::module_info &m : cli->list_all_modules()) items.push_back({m.name, m.description, m.loaded, m.enabled});
    return items;
  };
  hooks.parameters = [cli](const std::string &query) { return cli->list_parameters(query); };
  editor->install(hooks);

  std::vector<command_client::editor_item> builtins;
  for (const client::command_info &c : client::builtin_commands()) {
    // Lead with the argument placeholders: for a builtin, "what do I type
    // next" is the more useful thing to see while the cursor sits after it.
    builtins.push_back({c.name, c.args.empty() ? c.description : c.args + " - " + c.description});
  }
  editor->set_builtins(builtins);

  editor->set_color(color_);
  const std::string history = history_file_.empty() ? command_client::console_editor::default_history_file() : get_core()->expand_path(history_file_);
  editor->set_history(history, history_size_);
  return editor;
}

void CommandClient::interactive_input_loop(const std::shared_ptr<command_client::console_editor> &editor) const {
  editor->refresh_vocabulary();
  std::string line;
  while (is_running && editor->read_line(line)) {
    boost::algorithm::trim(line);
    if (line.empty()) continue;
    editor->remember(line);
    if (line == "exit") break;
    client->handle_command(line);
    if (changes_the_registry(line)) editor->refresh_vocabulary();
    // Persist as we go: `nscp test` is routinely killed rather than exited,
    // and history that only survives a clean shutdown is history you cannot
    // rely on.
    editor->save_history();
  }
  editor->save_history();
  // Whatever ended the loop - `exit`, Ctrl+C, Ctrl+D on an empty line, or
  // interrupt() from a signal - the process is done.
  is_running = false;
}

void CommandClient::piped_input_loop() const {
  // No terminal: `nscp test` with stdin redirected, closed, or fed from a
  // script. Read what there is, then stay up. Exiting at end of input would
  // break the common case of running the agent with stdin closed (which is
  // how the integration harness and most container setups start it), and
  // there is no prompt to protect so the core keeps writing the log itself.
  while (is_running) {
    // Once stdin is exhausted select() keeps reporting it readable, so the
    // eof check is what stops this from spinning at 100% CPU for the rest of
    // the process's life.
    if (std::cin.eof() || std::cin.fail()) {
      boost::this_thread::sleep(boost::posix_time::milliseconds(100));
      continue;
    }
    if (!input_available()) {
      boost::this_thread::sleep(boost::posix_time::milliseconds(100));
      continue;
    }
    std::string s;
    if (!std::getline(std::cin, s)) continue;
    boost::algorithm::trim(s);
    if (s.empty()) continue;
    if (s == "exit") {
      is_running = false;
      return;
    }
    client->handle_command(utf8::utf8_from_native(s));
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
    // The prompt is blocked in a terminal read and will not notice the flag
    // above on its own.
    if (const std::shared_ptr<command_client::console_editor> editor = get_editor()) editor->interrupt();
    NSC_LOG_MESSAGE("Command client shutdown was requested!");
    nscapi::protobuf::functions::set_response_good(*response, "Shutdown requested");
    return true;
  }
  if (is_running) {
    NSC_LOG_ERROR("Command client is already running!");
  }

  // Mark running *before* installing the signal/console handlers and starting
  // the input thread. If a SIGTERM/SIGINT (or Ctrl+C) arrives in that window
  // it sets is_running=false; were the input thread to set it true on startup
  // it would clobber that and lose the shutdown request.
  is_running = true;

#ifdef WIN32
  if (!SetConsoleCtrlHandler(consoleHandler, TRUE)) {
    NSC_LOG_MESSAGE("Could not set control handler");
  }
#endif
  // 	if (core_->get_service_control().is_started())
  // 		info(__LINE__, "Service seems to be started (Sockets and such will probably not work)...");

#ifndef WIN32
  // Graceful shutdown on POSIX SIGINT / SIGTERM. Without this nscp dies
  // abruptly when the test harness (or systemd, or `docker stop`) sends
  // SIGTERM — which bypasses atexit handlers and hides leaks from
  // LeakSanitizer. boost::asio::signal_set is async-signal-safe by
  // construction: the OS-level handler does a self-pipe write and the
  // user callback runs on our io_context thread in normal context.
  boost::asio::io_context signal_ioc;
  boost::asio::signal_set signals(signal_ioc, SIGINT, SIGTERM);
  signals.async_wait([](const boost::system::error_code &ec, int sig) {
    if (ec) return;  // cancelled during shutdown — nothing to do
    NSC_LOG_MESSAGE("Received signal " + std::to_string(sig) + ", shutting down");
    is_running = false;
    if (const std::shared_ptr<command_client::console_editor> editor = get_editor()) editor->interrupt();
  });
  boost::thread signal_thread([&signal_ioc] { signal_ioc.run(); });
#endif

  const bool interactive = command_client::console_editor::is_interactive();
  std::shared_ptr<command_client::console_editor> editor;
  if (interactive) {
    editor = make_editor();
    // From here until the editor is torn down, the prompt owns the console:
    // the core must stop writing to it or its output lands in the middle of
    // whatever is being typed. Log messages reach us through
    // handleLogMessage instead (module.json declares "log messages").
    get_core()->set_log_option("no-console");
    set_editor(editor);
    NSC_LOG_MESSAGE("Enter a command, tab to complete, up for history, help for help, exit to leave.");
  } else {
    NSC_DEBUG_MSG("Enter command to execute, help for help or exit to exit...");
  }

  input_thread = boost::thread([this, editor]() {
    if (editor) {
      this->interactive_input_loop(editor);
    } else {
      this->piped_input_loop();
    }
  });
  input_thread.join();
  // Release the thread object so it stops holding a copy of the functor (and
  // through it a reference to the editor we are about to tear down).
  input_thread = boost::thread();

  if (editor) {
    // Drop the shared reference the logging thread reaches us through, and
    // hand the console back to the core, before the editor is destroyed.
    set_editor(nullptr);
    get_core()->set_log_option("console");
    editor.reset();
  }

#ifndef WIN32
  // Drain the signal handler naturally: cancel the pending async_wait
  // (callback runs once with operation_aborted and returns), io_context
  // exits because no work remains, signal thread joins.
  {
    boost::system::error_code cancel_ec;
    signals.cancel(cancel_ec);
  }
  if (signal_thread.joinable()) signal_thread.join();
#endif

  nscapi::protobuf::functions::set_response_good(*response, "Done");
  return true;
}

void CommandClient::submitMetrics(const PB::Metrics::MetricsMessage &response) { client->push_metrics(response); }
