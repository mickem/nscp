// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// The interactive prompt for `nscp test`: line editing, persistent history,
// tab completion, syntax highlighting and hints, built on the vendored replxx
// (libs/replxx).
//
// The one thing that shapes this class is that NSClient++ logs continuously
// from a background thread while the prompt is up. print()/print_log() are
// safe to call from that thread: replxx queues the text and the input loop
// redraws the prompt around it, so a log line never lands in the middle of
// what the user is typing. The other half of that bargain lives in
// CommandClient, which asks the core to stop writing the console itself
// (core_wrapper::set_log_option("no-console")) for as long as we own it.

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "console_syntax.hpp"

namespace command_client {

// Severity of a log line, decoupled from the protobuf enum so this header (and
// its tests) do not need the generated protocol.
enum class log_severity { trace, debug, info, warning, error, critical };

// A name the prompt knows, with the one-line description it shows as a hint.
struct editor_item {
  std::string name;
  std::string description;
};

// A module, plus the two states that decide whether a given verb can act on
// it. See module_vocabulary in console_syntax.hpp.
struct editor_module {
  std::string name;
  std::string description;
  bool loaded = false;
  bool enabled = false;
};

// What the editor needs from the outside world. Supplied by CommandClient,
// which answers them out of the command registry; kept as callbacks so the
// editor itself never touches the plugin API.
//
// `queries` and `modules` are asked only when the vocabulary is refreshed
// (startup, and after anything that can change the registry) and their answers
// are cached, descriptions included: a registry round trip per keystroke to
// find a hint would be a poor trade. `parameters` is the exception - it is
// asked on tab, for one query at a time.
struct editor_hooks {
  std::function<std::vector<editor_item>()> queries;
  // The loaded modules. Cheap; asked on every refresh.
  std::function<std::vector<editor_module>()> modules;
  // Every module that could be loaded. Expensive - the core opens each module
  // in the module directory to answer it - so this is asked lazily, the first
  // time a completion actually needs it, and not again until the vocabulary is
  // refreshed. (The core caches its side too, so the cost is paid once per
  // process, not once per refresh.)
  std::function<std::vector<editor_module>()> all_modules;
  std::function<std::vector<std::string>(const std::string &)> parameters;
};

class console_editor {
 public:
  console_editor();
  ~console_editor();

  console_editor(const console_editor &) = delete;
  console_editor &operator=(const console_editor &) = delete;

  // Whether we are attached to a terminal we can draw on. False for a piped,
  // redirected or closed stdin - `nscp test` under the integration harness,
  // or `echo ... | nscp test`. In that case the caller must leave the core's
  // own console logging alone: there is no prompt to protect, and the harness
  // is reading stdout.
  static bool is_interactive();

  // Where history is kept when no explicit file is configured: under %APPDATA%
  // on Windows, $XDG_STATE_HOME or $HOME on POSIX. Empty when neither is set,
  // which means history stays in memory for the session only.
  static std::string default_history_file();

  void install(const editor_hooks &hooks);
  // The verbs the prompt handles itself; from client::builtin_commands().
  void set_builtins(const std::vector<editor_item> &builtins);
  // max_entries <= 0 disables persistent history entirely: nothing is read
  // from or written to `file`.
  void set_history(const std::string &file, int max_entries);
  void set_color(bool enabled);

  // Re-read queries and loaded modules from the hooks. Called at startup and
  // after anything that can change what is registered (load, unload, reload,
  // ...). Deliberately does not pull the full module list; see editor_hooks.
  void refresh_vocabulary();

  // Blocks until a line is entered. Returns false on EOF (Ctrl+D, or the end
  // of piped input) and on interrupt(), leaving `line` untouched.
  bool read_line(std::string &line);
  void remember(const std::string &line);
  void save_history();

  // Both are safe from any thread, including while read_line() is blocked.
  void print(const std::string &text);
  void print_log(log_severity severity, const std::string &sender, const std::string &message, const std::string &file, int line);

  // Wake a blocked read_line() so the caller can shut down.
  void interrupt();

 private:
  // The lazy half of refresh_vocabulary(): asks `all_modules` once and folds
  // the answer into the vocabulary. Called from the completion callback.
  void load_all_modules();

  struct impl;
  std::unique_ptr<impl> impl_;
};

}  // namespace command_client
