// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "console_syntax.hpp"

#include <algorithm>

namespace command_client {

namespace {

// The verbs whose first argument names a module rather than a query.
bool takes_module(const std::string &verb) { return verb == "load" || verb == "unload" || verb == "enable" || verb == "disable"; }

// ... and of those, the ones that act on a module which is *not* currently in
// the state they establish. Completing `load` with modules that are already
// loaded offers exactly the set that cannot usefully be loaded.
bool takes_absent_module(const std::string &verb) { return verb == "load" || verb == "enable"; }

bool is_space(const char c) { return c == ' ' || c == '\t'; }
bool is_quote(const char c) { return c == '"' || c == '\''; }

// A token as typed: byte offsets into the line plus the raw text. Quotes are
// kept in `text` because the highlighter colours them along with the string.
struct token {
  std::size_t begin = 0;
  std::size_t end = 0;
  std::string text;
  bool quoted = false;
};

// Split on unquoted whitespace. Quoting rules match str::utils::parse_command
// closely enough for highlighting: a quote runs to the matching quote, and an
// unterminated quote runs to the end of the line (which is exactly what the
// user sees while still typing it).
std::vector<token> tokenize(const std::string &input) {
  std::vector<token> tokens;
  std::size_t i = 0;
  while (i < input.size()) {
    while (i < input.size() && is_space(input[i])) i++;
    if (i >= input.size()) break;
    token t;
    t.begin = i;
    char quote = 0;
    while (i < input.size()) {
      const char c = input[i];
      if (quote != 0) {
        if (c == quote) quote = 0;
      } else if (is_quote(c)) {
        quote = c;
        t.quoted = true;
      } else if (is_space(c)) {
        break;
      }
      i++;
    }
    t.end = i;
    t.text = input.substr(t.begin, t.end - t.begin);
    tokens.push_back(t);
  }
  return tokens;
}

// Offset of the first unquoted '=' in `text`, or npos.
std::size_t split_point(const std::string &text) {
  char quote = 0;
  for (std::size_t i = 0; i < text.size(); i++) {
    const char c = text[i];
    if (quote != 0) {
      if (c == quote) quote = 0;
    } else if (is_quote(c)) {
      quote = c;
    } else if (c == '=') {
      return i;
    }
  }
  return std::string::npos;
}

bool contains(const std::set<std::string> &haystack, const std::string &needle) { return haystack.find(needle) != haystack.end(); }

// Strip surrounding quotes so `load "CheckDisk"` still resolves.
std::string unquote(const std::string &text) {
  if (text.size() >= 2 && is_quote(text.front()) && text.back() == text.front()) return text.substr(1, text.size() - 2);
  return text;
}

// index[i] = how many code points start strictly before byte i, so a byte
// range [from, to) maps to the code point range [index[from], index[to]).
// replxx sizes its colour buffer in code points, so a line containing a
// non-ASCII argument would otherwise colour the wrong columns.
std::vector<std::size_t> code_point_index(const std::string &input) {
  std::vector<std::size_t> index(input.size() + 1, 0);
  std::size_t cp = 0;
  for (std::size_t i = 0; i < input.size(); i++) {
    index[i] = cp;
    // Every byte that is not a UTF-8 continuation byte starts a code point.
    if ((static_cast<unsigned char>(input[i]) & 0xC0) != 0x80) cp++;
  }
  index[input.size()] = cp;
  return index;
}

std::size_t code_point_count(const std::string &input) {
  std::size_t cp = 0;
  for (std::size_t i = 0; i < input.size(); i++) {
    if ((static_cast<unsigned char>(input[i]) & 0xC0) != 0x80) cp++;
  }
  return cp;
}

void paint(std::vector<token_kind> &colors, const std::vector<std::size_t> &index, std::size_t from, std::size_t to, token_kind kind) {
  if (from >= to) return;
  const std::size_t first = index[from];
  const std::size_t last = index[to];
  for (std::size_t i = first; i < last && i < colors.size(); i++) colors[i] = kind;
}

// Classify one argument token: `--flag`, `key=value`, `"quoted"` or a bare word.
void paint_argument(std::vector<token_kind> &colors, const std::vector<std::size_t> &index, const token &t) {
  const std::size_t eq = split_point(t.text);
  if (eq == std::string::npos) {
    if (!t.text.empty() && t.text[0] == '-') {
      paint(colors, index, t.begin, t.end, token_kind::option);
    } else {
      paint(colors, index, t.begin, t.end, t.quoted ? token_kind::quoted : token_kind::value);
    }
    return;
  }
  paint(colors, index, t.begin, t.begin + eq, token_kind::option);
  paint(colors, index, t.begin + eq, t.begin + eq + 1, token_kind::punctuation);
  const std::string value = t.text.substr(eq + 1);
  const bool value_quoted = !value.empty() && is_quote(value[0]);
  paint(colors, index, t.begin + eq + 1, t.end, value_quoted ? token_kind::quoted : token_kind::value);
}

}  // namespace

vocabulary make_vocabulary(const std::vector<std::string> &builtins, const std::vector<std::string> &queries, const std::vector<std::string> &loaded_modules) {
  vocabulary vocab;
  vocab.builtins.insert(builtins.begin(), builtins.end());
  vocab.queries.insert(queries.begin(), queries.end());
  vocab.modules.loaded.insert(loaded_modules.begin(), loaded_modules.end());
  // A loaded module is by definition one we know exists; `all` is a superset
  // of `loaded` even before the full lookup has run.
  vocab.modules.all = vocab.modules.loaded;
  return vocab;
}

std::vector<token_kind> classify(const std::string &input, const vocabulary &vocab) {
  std::vector<token_kind> colors(code_point_count(input), token_kind::plain);
  if (colors.empty()) return colors;
  const std::vector<std::size_t> index = code_point_index(input);
  const std::vector<token> tokens = tokenize(input);
  if (tokens.empty()) return colors;

  const std::string verb = unquote(tokens[0].text);
  token_kind verb_kind = token_kind::unknown_name;
  if (contains(vocab.builtins, verb)) {
    verb_kind = token_kind::builtin;
  } else if (contains(vocab.queries, verb)) {
    verb_kind = token_kind::known_name;
  }
  paint(colors, index, tokens[0].begin, tokens[0].end, verb_kind);

  for (std::size_t n = 1; n < tokens.size(); n++) {
    const token &t = tokens[n];
    // The first argument of a module verb, or of `desc`, names something we
    // can check - so check it, and say so when it does not resolve.
    if (n == 1 && (takes_module(verb) || verb == "desc")) {
      const std::string name = unquote(t.text);
      if (verb == "desc") {
        paint(colors, index, t.begin, t.end, contains(vocab.queries, name) ? token_kind::known_name : token_kind::unknown_name);
      } else if (contains(vocab.modules.all, name)) {
        paint(colors, index, t.begin, t.end, token_kind::known_name);
      } else {
        // Only call it wrong once we have actually looked at every module. Up
        // to then all we know is that it is not loaded, which is precisely
        // what you would type after `load`.
        paint(colors, index, t.begin, t.end, vocab.modules.complete ? token_kind::unknown_name : token_kind::plain);
      }
      continue;
    }
    paint_argument(colors, index, t);
  }
  return colors;
}

completion_context analyze(const std::string &input) {
  completion_context ctx;
  const std::vector<token> tokens = tokenize(input);
  const bool trailing_space = !input.empty() && is_space(input[input.size() - 1]);
  if (!tokens.empty()) ctx.command = unquote(tokens[0].text);
  if (tokens.empty() || trailing_space) {
    // Starting a new word: the prefix is empty and the index is one past the
    // last complete word.
    ctx.word_index = static_cast<int>(tokens.size());
    if (ctx.word_index == 0) ctx.command.clear();
    return ctx;
  }
  ctx.word_index = static_cast<int>(tokens.size()) - 1;
  ctx.prefix = tokens.back().text;
  if (ctx.word_index == 0) ctx.command.clear();
  return ctx;
}

std::vector<std::string> complete(const std::string &input, const vocabulary &vocab,
                                  const std::function<std::vector<std::string>(const std::string &)> &parameters_of) {
  const completion_context ctx = analyze(input);
  std::vector<std::string> candidates;

  if (ctx.word_index == 0) {
    candidates.insert(candidates.end(), vocab.builtins.begin(), vocab.builtins.end());
    candidates.insert(candidates.end(), vocab.queries.begin(), vocab.queries.end());
  } else if (ctx.word_index == 1 && takes_module(ctx.command)) {
    // Offer the modules the verb can actually do something to: what is not
    // loaded for `load`, not enabled for `enable`, and the converse for their
    // opposites.
    const std::set<std::string> &established = ctx.command == "enable" || ctx.command == "disable" ? vocab.modules.enabled : vocab.modules.loaded;
    if (takes_absent_module(ctx.command)) {
      for (const std::string &name : vocab.modules.all) {
        if (!contains(established, name)) candidates.push_back(name);
      }
    } else {
      candidates.insert(candidates.end(), established.begin(), established.end());
    }
  } else if (ctx.word_index == 1 && ctx.command == "desc") {
    candidates.insert(candidates.end(), vocab.queries.begin(), vocab.queries.end());
  } else if (split_point(ctx.prefix) == std::string::npos && parameters_of) {
    // Argument position of a real query: offer its parameters as `name=`, and
    // only while the user is still typing the name - once there is an `=` the
    // value is theirs and we have nothing useful to add.
    if (contains(vocab.queries, ctx.command)) {
      for (const std::string &name : parameters_of(ctx.command)) candidates.push_back(name + "=");
    }
  }

  std::vector<std::string> matches;
  for (const std::string &candidate : candidates) {
    if (candidate.compare(0, ctx.prefix.size(), ctx.prefix) == 0) matches.push_back(candidate);
  }
  std::sort(matches.begin(), matches.end());
  matches.erase(std::unique(matches.begin(), matches.end()), matches.end());
  return matches;
}

bool needs_all_modules(const std::string &input) {
  const completion_context ctx = analyze(input);
  return ctx.word_index == 1 && takes_absent_module(ctx.command);
}

std::string hint(const std::string &input, const vocabulary &vocab, const std::function<std::string(const std::string &)> &describe) {
  if (!describe) return "";
  const std::vector<token> tokens = tokenize(input);
  if (tokens.empty()) return "";
  const std::string verb = unquote(tokens[0].text);
  // Only once the command is settled: while it is still the word under the
  // cursor, completion is the useful thing to show, not a description.
  const bool still_typing_command = tokens.size() == 1 && !is_space(input[input.size() - 1]);
  if (still_typing_command) return "";
  if (!contains(vocab.builtins, verb) && !contains(vocab.queries, verb)) return "";
  const std::string description = describe(verb);
  if (description.empty()) return "";
  // One line only: registry descriptions are often several paragraphs, and the
  // hint is drawn on the prompt line.
  const std::size_t eol = description.find('\n');
  return "  " + (eol == std::string::npos ? description : description.substr(0, eol));
}

}  // namespace command_client
