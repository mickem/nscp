// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Syntax analysis for the interactive prompt: what each part of a typed line
// means, what could come next, and what to say about it.
//
// Deliberately free of replxx (and of any core/plugin API): everything here is
// a pure function of the typed line plus a vocabulary snapshot, so it can be
// unit tested without a terminal, a core, or a loaded module. console_editor
// is the thin layer that maps token_kind onto replxx colours and feeds these
// functions from the registry.

#include <functional>
#include <set>
#include <string>
#include <vector>

namespace command_client {

// What a run of characters in the typed line means. One of these is produced
// per Unicode code point, because that is the granularity replxx colours at.
enum class token_kind {
  // Whitespace and anything we have nothing to say about.
  plain,
  // A verb the prompt handles itself (help, load, desc, ...).
  builtin,
  // A name that resolves in the position it appears in: a registered query in
  // command position, a known module after `load`, and so on.
  known_name,
  // A name in a position where one was expected, that does not resolve. This
  // is the one that earns its keep: a typo is visible before you hit enter.
  unknown_name,
  // An option name: `--foo`, or the `foo` of `foo=bar`.
  option,
  // A bare argument, or the `bar` of `foo=bar`.
  value,
  // A quoted run, quotes included.
  quoted,
  // The `=` joining an option to its value.
  punctuation,
};

// What the prompt knows about modules.
//
// The split exists because the two halves cost wildly different amounts. The
// loaded (and enabled) sets come out of the core's in-memory plugin list for
// nothing. `all` - which is the interesting one for `load`, since you can only
// usefully load what is *not* already loaded - makes the core scan the module
// directory and open every module it finds there. So `all` is fetched lazily,
// the first time someone actually completes in a position that needs it, and
// `complete` says whether that has happened yet.
//
// Until it has, a module name we do not recognise is merely unknown to us, not
// wrong: `all` holds only the loaded modules, so calling a valid but unloaded
// name a typo would be crying wolf. classify() takes that into account.
struct module_vocabulary {
  std::set<std::string> loaded;
  std::set<std::string> enabled;
  // Always a superset of `loaded`.
  std::set<std::string> all;
  bool complete = false;
};

// A snapshot of what names currently mean something. Cheap to copy; the prompt
// refreshes it after anything that can change the registry (load, unload,
// enable, disable, reload).
struct vocabulary {
  std::set<std::string> builtins;
  // Queries and query aliases merged: they are interchangeable when typed.
  std::set<std::string> queries;
  module_vocabulary modules;
};

// Builds a vocabulary that knows only the loaded modules - the cheap half.
// Fill in vocab.modules.all / .enabled / .complete afterwards once the
// expensive lookup has run.
vocabulary make_vocabulary(const std::vector<std::string> &builtins, const std::vector<std::string> &queries, const std::vector<std::string> &loaded_modules);

// One token_kind per code point of `input`. Size always equals the number of
// code points, which is what replxx's highlighter callback requires - not the
// number of bytes.
std::vector<token_kind> classify(const std::string &input, const vocabulary &vocab);

// Where the cursor is, in terms of the grammar above.
struct completion_context {
  // The word being completed. Empty when the line ends in whitespace, which
  // means a new word is starting.
  std::string prefix;
  // 0 for the command itself, 1 for its first argument, and so on.
  int word_index = 0;
  // Word 0, or empty when word_index is 0.
  std::string command;
};
completion_context analyze(const std::string &input);

// Whether completing this line needs the full module list (and so the
// expensive lookup behind it). True for the argument of `load`/`enable`, which
// offer what is *not* already loaded/enabled and therefore cannot be answered
// from the loaded set alone.
bool needs_all_modules(const std::string &input);

// Candidates for the word under the cursor, sorted and de-duplicated. Each
// candidate is a whole word: the caller replaces `prefix` with it.
//
// `parameters_of` supplies the parameter names a query accepts, so that
// `check_drive fi<tab>` can offer `filter=`. It is a callback rather than part
// of `vocabulary` because answering it costs a registry round trip per query,
// which is only worth paying when the user actually asks.
std::vector<std::string> complete(const std::string &input, const vocabulary &vocab,
                                  const std::function<std::vector<std::string>(const std::string &)> &parameters_of);

// The greyed-out note shown after the cursor, or empty for none. `describe`
// maps a name to its one-line description; the hint is the description of the
// command on the line, shown once that command is unambiguous.
std::string hint(const std::string &input, const vocabulary &vocab, const std::function<std::string(const std::string &)> &describe);

}  // namespace command_client
