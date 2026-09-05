// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "console_editor.hpp"

#include <boost/filesystem.hpp>
#include <cstdlib>
#include <map>
#include <replxx.hxx>

#ifdef _WIN32
#include <io.h>
#include <stdio.h>
#else
#include <unistd.h>
#endif

namespace command_client {

namespace {

using Replxx = replxx::Replxx;

Replxx::Color color_for(const token_kind kind) {
  switch (kind) {
    case token_kind::builtin:
      return Replxx::Color::BRIGHTCYAN;
    case token_kind::known_name:
      return Replxx::Color::BRIGHTGREEN;
    case token_kind::unknown_name:
      return Replxx::Color::BRIGHTRED;
    case token_kind::option:
      return Replxx::Color::YELLOW;
    case token_kind::quoted:
      return Replxx::Color::BRIGHTMAGENTA;
    case token_kind::punctuation:
      return Replxx::Color::GRAY;
    case token_kind::value:
    case token_kind::plain:
    default:
      return Replxx::Color::DEFAULT;
  }
}

// SGR sequences rather than Replxx::Color, because print()/write() take raw
// text. replxx normalises these on Windows: it enables virtual terminal
// processing where available and otherwise translates the escapes to console
// attributes itself, so the same bytes work on conhost and Windows Terminal.
const char *ansi_reset() { return "\033[0m"; }

const char *ansi_for(const log_severity severity) {
  switch (severity) {
    case log_severity::critical:
      return "\033[1;91m";
    case log_severity::error:
      return "\033[91m";
    case log_severity::warning:
      return "\033[93m";
    case log_severity::debug:
    case log_severity::trace:
      return "\033[90m";
    case log_severity::info:
    default:
      return "";
  }
}

char letter_for(const log_severity severity) {
  switch (severity) {
    case log_severity::critical:
      return 'C';
    case log_severity::error:
      return 'E';
    case log_severity::warning:
      return 'W';
    case log_severity::debug:
      return 'D';
    case log_severity::trace:
      return 'T';
    case log_severity::info:
    default:
      return 'L';
  }
}

std::string rpad(const std::string &value, const std::size_t width) {
  if (value.size() >= width) return value;
  return value + std::string(width - value.size(), ' ');
}

// Number of code points, which is the unit replxx counts completion context in.
int code_points(const std::string &value) {
  int count = 0;
  for (std::size_t i = 0; i < value.size(); i++) {
    if ((static_cast<unsigned char>(value[i]) & 0xC0) != 0x80) count++;
  }
  return count;
}

bool stdin_is_a_tty() {
#ifdef _WIN32
  return _isatty(_fileno(stdin)) != 0;
#else
  return isatty(STDIN_FILENO) != 0;
#endif
}

bool stdout_is_a_tty() {
#ifdef _WIN32
  return _isatty(_fileno(stdout)) != 0;
#else
  return isatty(STDOUT_FILENO) != 0;
#endif
}

std::string env(const char *name) {
  const char *value = std::getenv(name);
  return value == nullptr ? std::string() : std::string(value);
}

}  // namespace

struct console_editor::impl {
  Replxx replxx;
  editor_hooks hooks;
  std::vector<editor_item> builtins;
  vocabulary vocab;
  // Name -> one-line description, for the hint. Rebuilt with the vocabulary;
  // looking this up in the registry per keystroke would be a round trip for
  // every character typed.
  std::map<std::string, std::string> descriptions;
  std::string history_file;
  bool history_dirty = false;
};

console_editor::console_editor() : impl_(new impl()) {}

console_editor::~console_editor() = default;

bool console_editor::is_interactive() { return stdin_is_a_tty() && stdout_is_a_tty(); }

std::string console_editor::default_history_file() {
#ifdef _WIN32
  const std::string base = env("APPDATA");
  if (base.empty()) return "";
  return base + "\\NSClient++\\console-history.txt";
#else
  const std::string state = env("XDG_STATE_HOME");
  if (!state.empty()) return state + "/nscp/console-history";
  const std::string home = env("HOME");
  if (home.empty()) return "";
  return home + "/.nscp_history";
#endif
}

void console_editor::install(const editor_hooks &hooks) {
  impl_->hooks = hooks;
  Replxx &rx = impl_->replxx;

  // '=' is a word breaker so that `check_drive fil<tab>` completes the option
  // name and `check_drive filter=<tab>` does not try to complete the value.
  rx.set_word_break_characters(" \t=");
  rx.set_completion_count_cutoff(128);
  rx.set_double_tab_completion(false);
  rx.set_complete_on_empty(false);
  rx.set_beep_on_ambiguous_completion(false);
  rx.set_unique_history(true);
  rx.set_max_hint_rows(1);
  rx.install_window_change_handler();

  rx.set_completion_callback([this](const std::string &input, int &context_len) -> Replxx::completions_t {
    // `parameters` and the module lookup below reach into the command
    // registry, which can itself log. A log message raised from here is
    // written straight out (we are on the thread replxx is drawing from, so it
    // does not take the queued path) and can smear one frame of the redraw;
    // the next key repaints it. Worth knowing about, not worth suppressing the
    // queries for.
    Replxx::completions_t completions;
    // Pay for the full module list only when the word being completed is one
    // that needs it - the argument of `load` or `enable`. The first tab there
    // is visibly slower than the rest; every one after it is not.
    if (needs_all_modules(input) && !impl_->vocab.modules.complete) load_all_modules();
    const std::vector<std::string> matches = command_client::complete(input, impl_->vocab, impl_->hooks.parameters);
    const completion_context ctx = analyze(input);
    context_len = code_points(ctx.prefix);
    for (const std::string &match : matches) completions.emplace_back(match);
    return completions;
  });

  rx.set_highlighter_callback([this](const std::string &input, Replxx::colors_t &colors) {
    const std::vector<token_kind> kinds = classify(input, impl_->vocab);
    const std::size_t count = colors.size() < kinds.size() ? colors.size() : kinds.size();
    for (std::size_t i = 0; i < count; i++) colors[i] = color_for(kinds[i]);
  });

  rx.set_hint_callback([this](const std::string &input, int &context_len, Replxx::Color &color) -> Replxx::hints_t {
    Replxx::hints_t hints;
    const std::string text = command_client::hint(input, impl_->vocab, [this](const std::string &name) {
      const std::map<std::string, std::string>::const_iterator it = impl_->descriptions.find(name);
      return it == impl_->descriptions.end() ? std::string() : it->second;
    });
    if (text.empty()) return hints;
    // The hint is a standalone note, not a continuation of the word under the
    // cursor, so the context it should overwrite is nothing at all. replxx
    // renders hint[context_len..], which would otherwise eat its first
    // characters.
    context_len = 0;
    color = Replxx::Color::GRAY;
    hints.push_back(text);
    return hints;
  });
}

void console_editor::set_history(const std::string &file, const int max_entries) {
  // max_entries 0 means "remember nothing across sessions": no file is read or
  // written, and the in-session recall is capped at one screenful. That is the
  // switch for an operator who would rather not have typed commands (which can
  // carry a password on the command line) land in a file at all.
  if (max_entries <= 0) {
    impl_->history_file.clear();
    impl_->replxx.set_max_history_size(50);
    return;
  }
  impl_->history_file = file;
  impl_->replxx.set_max_history_size(max_entries);
  if (!file.empty()) {
    // A missing file is the normal first-run case; replxx reports it by
    // returning false and there is nothing to say about it.
    impl_->replxx.history_load(file);
  }
}

void console_editor::set_color(const bool enabled) { impl_->replxx.set_no_color(!enabled); }

void console_editor::set_builtins(const std::vector<editor_item> &builtins) { impl_->builtins = builtins; }

void console_editor::refresh_vocabulary() {
  std::vector<editor_item> queries;
  std::vector<editor_module> modules;
  if (impl_->hooks.queries) queries = impl_->hooks.queries();
  if (impl_->hooks.modules) modules = impl_->hooks.modules();

  std::vector<std::string> query_names;
  std::vector<std::string> loaded_names;
  std::vector<std::string> builtin_names;
  impl_->descriptions.clear();
  // Builtins last so a verb the prompt handles itself wins the description,
  // the same way it wins in classify().
  for (const editor_item &item : queries) {
    query_names.push_back(item.name);
    impl_->descriptions[item.name] = item.description;
  }
  for (const editor_module &item : modules) loaded_names.push_back(item.name);
  for (const editor_item &item : impl_->builtins) {
    builtin_names.push_back(item.name);
    impl_->descriptions[item.name] = item.description;
  }
  impl_->vocab = make_vocabulary(builtin_names, query_names, loaded_names);
  for (const editor_module &item : modules) {
    if (item.enabled) impl_->vocab.modules.enabled.insert(item.name);
  }
  // The expensive half is deliberately not fetched here; the completion
  // callback asks for it if and when a completion needs it.
}

void console_editor::load_all_modules() {
  if (!impl_->hooks.all_modules) return;
  const std::vector<editor_module> modules = impl_->hooks.all_modules();
  for (const editor_module &item : modules) {
    impl_->vocab.modules.all.insert(item.name);
    if (item.loaded) impl_->vocab.modules.loaded.insert(item.name);
    if (item.enabled) impl_->vocab.modules.enabled.insert(item.name);
  }
  // Set even when the lookup came back empty: it is the answer, and repeating
  // a lookup that costs this much on every keystroke would be worse than a
  // prompt that offers nothing.
  impl_->vocab.modules.complete = true;
}

bool console_editor::read_line(std::string &line) {
  const char *result = impl_->replxx.input("\033[1;36mnscp\033[0m\033[90m>\033[0m ");
  if (result == nullptr) return false;
  line = result;
  return true;
}

void console_editor::remember(const std::string &line) {
  if (line.empty()) return;
  impl_->replxx.history_add(line);
  impl_->history_dirty = true;
}

void console_editor::save_history() {
  if (impl_->history_file.empty() || !impl_->history_dirty) return;
  try {
    // The default location is a per-user directory that will not exist on a
    // first run (%APPDATA%\NSClient++, $XDG_STATE_HOME/nscp); replxx just
    // fails to open the file if it is missing.
    const boost::filesystem::path path(impl_->history_file);
    if (path.has_parent_path() && !path.parent_path().empty()) {
      boost::filesystem::create_directories(path.parent_path());
    }
    impl_->replxx.history_save(impl_->history_file);
    impl_->history_dirty = false;
  } catch (const std::exception &) {
    // A read-only or unwritable home is not worth interrupting the session
    // over, and complaining once per command would be worse. Keep the history
    // in memory and stop trying to write it.
    impl_->history_file.clear();
    impl_->history_dirty = false;
  }
}

void console_editor::print(const std::string &text) {
  // write() rather than print(): print() is printf-style and log messages are
  // arbitrary text that may well contain a '%'.
  impl_->replxx.write(text.data(), static_cast<int>(text.size()));
}

void console_editor::print_log(const log_severity severity, const std::string &sender, const std::string &message, const std::string &file, const int line) {
  std::string text;
  text += ansi_for(severity);
  text += letter_for(severity);
  text += ansi_reset();
  text += " \033[90m" + rpad(sender, 10) + ansi_reset() + " ";
  text += message;
  text += "\n";
  if (severity == log_severity::error || severity == log_severity::critical) {
    text += "\033[90m                     " + file + ":" + std::to_string(line) + ansi_reset() + "\n";
  }
  print(text);
}

void console_editor::interrupt() {
  // Ctrl+C is bound to abort_line, which always bails out of input() whatever
  // is on the line - unlike Ctrl+D, which only does so when the line is empty
  // and deletes a character otherwise.
  impl_->replxx.emulate_key_press(Replxx::KEY::control('C'));
}

}  // namespace command_client
