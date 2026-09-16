// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "console_syntax.hpp"

#include <algorithm>
#include <cctype>

namespace command_client {

namespace {

// The verbs whose first argument names a module rather than a query.
bool takes_module(const std::string &verb) { return verb == "load" || verb == "unload" || verb == "enable" || verb == "disable"; }

// ... and of those, the ones that act on a module which is *not* currently in
// the state they establish. Completing `load` with modules that are already
// loaded offers exactly the set that cannot usefully be loaded.
bool takes_absent_module(const std::string &verb) { return verb == "load" || verb == "enable"; }

// Verbs whose first argument is a query name: completed and checked against
// the registered queries rather than the modules.
bool takes_query(const std::string &verb) { return verb == "desc" || verb == "keywords"; }

// ... and the verbs that take flags rather than a name at all, so that
// `plugins --<tab>` offers what there is to pick from. They accept more than
// one, hence every argument position rather than just the first.
bool takes_module_filter(const std::string &verb) { return verb == "plugins" || verb == "modules"; }

// Check options whose value is a filter expression in the where language -
// the set modern_filter::cli_helper registers (add_filter_option,
// add_warn_option, add_crit_option, add_ok_option), short aliases included.
bool takes_expression(const std::string &option) {
  return option == "filter" || option == "warning" || option == "warn" || option == "critical" || option == "crit" || option == "ok";
}

// ... and those whose value is a syntax template: literal text carrying
// ${...} and %(...) placeholders, each holding an expression in that same
// language. `perf-config` is deliberately not here: it is a third grammar
// whose keys name performance counters rather than filter keywords.
bool takes_template(const std::string &option) {
  return option == "top-syntax" || option == "ok-syntax" || option == "empty-syntax" || option == "detail-syntax" || option == "perf-syntax";
}

bool takes_keywords(const std::string &option) { return takes_expression(option) || takes_template(option); }

// The words the expression language reserves for itself: the boolean
// connectives, the spelled-out comparison operators, and `str`. Matched
// case-insensitively, as the grammar matches them (charset::no_case).
bool is_expression_word(const std::string &lower) {
  return lower == "and" || lower == "or" || lower == "not" || lower == "in" || lower == "like" || lower == "regexp" || lower == "le" || lower == "lt" ||
         lower == "eq" || lower == "ne" || lower == "ge" || lower == "gt" || lower == "str";
}

std::string to_lower(const std::string &text) {
  std::string ret = text;
  for (char &c : ret) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return ret;
}

// The option name as the parser sees it: `--filter`, `-filter` and `filter`
// are the same option, and option names are lower case throughout.
std::string option_name(const std::string &text) {
  std::size_t start = 0;
  while (start < text.size() && text[start] == '-') start++;
  return to_lower(text.substr(start));
}

bool is_word_char(const char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_'; }
bool is_word_start(const char c) { return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_'; }
bool is_digit(const char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }

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

// Split on unquoted whitespace. Quoting rules match
// str::utils::parse_prompt_command closely enough for highlighting: a quote
// runs to the matching quote, an unterminated quote runs to the end of the
// line (which is exactly what the user sees while still typing it), and a
// single quote only opens a string at the start of an argument or right after
// its first '=' - elsewhere it is an ordinary character, so
// filter=core='total' is one plain token.
std::vector<token> tokenize(const std::string &input) {
  std::vector<token> tokens;
  std::size_t i = 0;
  while (i < input.size()) {
    while (i < input.size() && is_space(input[i])) i++;
    if (i >= input.size()) break;
    token t;
    t.begin = i;
    char quote = 0;
    std::size_t equals = 0;
    while (i < input.size()) {
      const char c = input[i];
      const bool value_start = i == t.begin || (equals == 1 && input[i - 1] == '=');
      if (quote != 0) {
        if (c == quote) quote = 0;
      } else if (c == '"' || (c == '\'' && value_start)) {
        quote = c;
        t.quoted = true;
      } else if (is_space(c)) {
        break;
      } else if (c == '=') {
        equals++;
      }
      i++;
    }
    t.end = i;
    t.text = input.substr(t.begin, t.end - t.begin);
    tokens.push_back(t);
  }
  return tokens;
}

// Offset of the first unquoted '=' in `text`, or npos. Before the first '='
// a single quote only counts at the very start of the token (see tokenize).
std::size_t split_point(const std::string &text) {
  char quote = 0;
  for (std::size_t i = 0; i < text.size(); i++) {
    const char c = text[i];
    if (quote != 0) {
      if (c == quote) quote = 0;
    } else if (c == '"' || (c == '\'' && i == 0)) {
      quote = c;
    } else if (c == '=') {
      return i;
    }
  }
  return std::string::npos;
}

bool contains(const std::set<std::string> &haystack, const std::string &needle) { return haystack.find(needle) != haystack.end(); }

// Case-insensitive prefix test. Module names are CamelCase and nobody
// remembers which letters; the editor replaces exactly the typed prefix with
// the match, so completing "check" to "CheckDisk" also corrects the case.
bool starts_with_ci(const std::string &candidate, const std::string &prefix) {
  if (prefix.size() > candidate.size()) return false;
  for (std::size_t i = 0; i < prefix.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(candidate[i])) != std::tolower(static_cast<unsigned char>(prefix[i]))) return false;
  }
  return true;
}

// ... and the same test anywhere in the name, for the fallback in complete().
// Every module is called Check-something and every query check_something, so
// the part you remember is rarely the part you have to type first.
bool contains_ci(const std::string &candidate, const std::string &needle) {
  if (needle.size() > candidate.size()) return false;
  for (std::size_t start = 0; start + needle.size() <= candidate.size(); ++start) {
    std::size_t i = 0;
    while (i < needle.size() && std::tolower(static_cast<unsigned char>(candidate[start + i])) == std::tolower(static_cast<unsigned char>(needle[i]))) i++;
    if (i == needle.size()) return true;
  }
  return false;
}

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

// What a name inside an expression is. `call` says it was followed by '(' and
// so names a filter function rather than a variable.
//
// Silence is the default: with no keyword list yet (the first keystroke after
// an `=`, or a query that turned out to declare none) nothing is known, and a
// name we cannot check is not a name we can call wrong. The same restraint
// classify() shows towards a module name before the full lookup has run.
token_kind classify_keyword(const std::string &name, const bool call, const keyword_vocabulary *kv) {
  // A dotted name is not a filter keyword at all - the grammar's variable_name
  // admits no '.' - so it reached the template from some other expansion
  // layer. Say nothing about it.
  if (name.find('.') != std::string::npos) return token_kind::plain;
  if (kv == nullptr || !kv->complete) return token_kind::plain;
  if (contains(call ? kv->functions : kv->variables, name)) return call ? token_kind::function : token_kind::keyword;
  return token_kind::unknown_keyword;
}

// Paint `text` - which occupies the line from byte `from` - as a filter
// expression, following include/parsers/where/grammar/grammar.cpp. Partial
// input is the normal case here (the user is still typing), so every scan
// ends politely at the end of the text rather than insisting on a closer.
void paint_expression(std::vector<token_kind> &colors, const std::vector<std::size_t> &index, const std::size_t from, const std::string &text,
                      const keyword_vocabulary *kv) {
  const std::size_t n = text.size();
  std::size_t i = 0;
  while (i < n) {
    const char c = text[i];
    if (is_space(c)) {
      i++;
      continue;
    }
    // 'a string literal'. The grammar has no escapes inside one, so the first
    // closing quote ends it.
    if (c == '\'') {
      std::size_t j = i + 1;
      while (j < n && text[j] != '\'') j++;
      if (j < n) j++;
      paint(colors, index, from + i, from + j, token_kind::quoted);
      i = j;
      continue;
    }
    if (is_word_start(c)) {
      std::size_t j = i;
      while (j < n && (is_word_char(text[j]) || text[j] == '.')) j++;
      const std::string word = text.substr(i, j - i);
      const std::string lower = to_lower(word);
      if (is_expression_word(lower)) {
        paint(colors, index, from + i, from + j, token_kind::expression_op);
        i = j;
        // `str(...)` takes a raw run of characters rather than an expression:
        // the grammar's string_literal_ex is everything up to the first ')'.
        if (lower == "str") {
          std::size_t k = j;
          while (k < n && is_space(text[k])) k++;
          if (k < n && text[k] == '(') {
            std::size_t close = k + 1;
            while (close < n && text[close] != ')') close++;
            if (close < n) close++;
            paint(colors, index, from + k, from + close, token_kind::quoted);
            i = close;
          }
        }
        continue;
      }
      // A name followed by '(' is a function call. The grammar runs under a
      // space skipper, so `convert_size (size)` is one too - look past it.
      std::size_t k = j;
      while (k < n && is_space(text[k])) k++;
      const bool call = k < n && text[k] == '(';
      paint(colors, index, from + i, from + j, classify_keyword(word, call, kv));
      i = j;
      continue;
    }
    if (is_digit(c)) {
      std::size_t j = i;
      while (j < n && (is_digit(text[j]) || text[j] == '.')) j++;
      // One trailing letter or '%' is the unit of the number+unit lexeme
      // (10%, 1G, 5m); a longer run is a separate token.
      if (j < n && (std::isalpha(static_cast<unsigned char>(text[j])) != 0 || text[j] == '%') && (j + 1 >= n || !is_word_char(text[j + 1]))) j++;
      paint(colors, index, from + i, from + j, token_kind::number);
      i = j;
      continue;
    }
    if (c == '<' || c == '>' || c == '!' || c == '=' || c == '&' || c == '|' || c == '(' || c == ')' || c == ',') {
      std::size_t j = i + 1;
      if ((c == '<' || c == '>' || c == '!') && j < n && text[j] == '=') j++;
      paint(colors, index, from + i, from + j, token_kind::expression_op);
      i = j;
      continue;
    }
    i++;
  }
}

// Where the ')' closing a `%(` placeholder is, mirroring
// find_placeholder_close in parsers/expression/expression.cpp: balanced
// parens with quoted strings skipped, falling back to the first ')'. Kept
// local because this file stays clear of the parser libraries - but the two
// must agree, or the prompt would colour a placeholder differently from the
// way the engine reads it.
std::size_t placeholder_close(const std::string &text, const std::size_t body_start) {
  const std::size_t n = text.size();
  std::size_t j = body_start;
  int depth = 1;
  while (j < n && depth > 0) {
    const char c = text[j];
    if (c == '\'') {
      j++;
      while (j < n && text[j] != '\'') j++;
      if (j < n) j++;
      continue;
    }
    if (c == '(') {
      depth++;
    } else if (c == ')' && --depth == 0) {
      return j;
    }
    j++;
  }
  j = body_start;
  while (j < n && text[j] != ')') j++;
  return j < n ? j : std::string::npos;
}

// Paint `text` as a syntax template: literal text carrying ${...} and %(...)
// placeholders. The literal parts stay a plain value - they are output, not
// code - and each placeholder body is an expression in its own right.
void paint_template(std::vector<token_kind> &colors, const std::vector<std::size_t> &index, const std::size_t from, const std::string &text,
                    const keyword_vocabulary *kv) {
  const std::size_t n = text.size();
  paint(colors, index, from, from + n, token_kind::value);
  std::size_t i = 0;
  while (i + 1 < n) {
    const bool dollar = text[i] == '$' && text[i + 1] == '{';
    const bool percent = text[i] == '%' && text[i + 1] == '(';
    if (!dollar && !percent) {
      i++;
      continue;
    }
    const std::size_t body = i + 2;
    const std::size_t close = dollar ? text.find('}', body) : placeholder_close(text, body);
    if (close == std::string::npos || close <= body) {
      // Still being typed, or empty. The engine emits the '$' or '%' as
      // literal text in that case, so leave it looking like one.
      i++;
      continue;
    }
    paint(colors, index, from + i, from + body, token_kind::expression_op);
    paint_expression(colors, index, from + body, text.substr(body, close - body), kv);
    paint(colors, index, from + close, from + close + 1, token_kind::expression_op);
    i = close + 1;
  }
}

// Classify one argument token: `--flag`, `key=value`, `"quoted"` or a bare
// word. `is_query` says the command on the line is a registered query, which
// is what makes `filter=` a filter expression rather than an opaque value;
// `kv` is that query's keyword list, when it has been fetched.
void paint_argument(std::vector<token_kind> &colors, const std::vector<std::size_t> &index, const std::size_t begin, const std::string &text, const bool quoted,
                    const bool is_query, const keyword_vocabulary *kv) {
  const std::size_t eq = split_point(text);
  if (eq == std::string::npos) {
    // `"filter=free < 10%"` - the whole argument wrapped in quotes, which is
    // how most command lines spell it - is a single token whose '=' sits
    // inside the quoted run, where split_point cannot see it. Unwrap and
    // classify what is inside; the quotes themselves stay quoted. Only when
    // there is an '=' in there: a plain quoted value is still a quoted value.
    if (text.size() >= 2 && is_quote(text[0]) && text[text.size() - 1] == text[0]) {
      const std::string inner = text.substr(1, text.size() - 2);
      if (split_point(inner) != std::string::npos) {
        paint(colors, index, begin, begin + 1, token_kind::quoted);
        paint(colors, index, begin + text.size() - 1, begin + text.size(), token_kind::quoted);
        paint_argument(colors, index, begin + 1, inner, false, is_query, kv);
        return;
      }
    }
    if (!text.empty() && text[0] == '-') {
      paint(colors, index, begin, begin + text.size(), token_kind::option);
    } else {
      paint(colors, index, begin, begin + text.size(), quoted ? token_kind::quoted : token_kind::value);
    }
    return;
  }
  paint(colors, index, begin, begin + eq, token_kind::option);
  paint(colors, index, begin + eq, begin + eq + 1, token_kind::punctuation);
  const std::string value = text.substr(eq + 1);
  const bool value_quoted = !value.empty() && is_quote(value[0]);
  const std::string option = option_name(text.substr(0, eq));
  if (is_query && takes_keywords(option)) {
    // A quoted value is an expression with quotes around it, not an opaque
    // string: colour the quotes and read what is between them.
    std::size_t body_begin = begin + eq + 1;
    std::string body = value;
    if (value_quoted) {
      const bool closed = body.size() >= 2 && body[body.size() - 1] == body[0];
      paint(colors, index, body_begin, body_begin + 1, token_kind::quoted);
      if (closed) paint(colors, index, begin + text.size() - 1, begin + text.size(), token_kind::quoted);
      body = body.substr(1, closed ? body.size() - 2 : std::string::npos);
      body_begin++;
    }
    if (takes_expression(option)) {
      paint_expression(colors, index, body_begin, body, kv);
    } else {
      paint_template(colors, index, body_begin, body, kv);
    }
    return;
  }
  paint(colors, index, begin + eq + 1, begin + text.size(), value_quoted ? token_kind::quoted : token_kind::value);
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

  // A filter expression only means something on a query, and only its own
  // keywords can appear in it. Looked up once for the whole line; absent
  // simply means nobody has fetched them yet (see keyword_vocabulary).
  const bool is_query = contains(vocab.queries, verb);
  const std::map<std::string, keyword_vocabulary>::const_iterator kv_it = vocab.keywords.find(verb);
  const keyword_vocabulary *kv = kv_it == vocab.keywords.end() ? nullptr : &kv_it->second;

  for (std::size_t n = 1; n < tokens.size(); n++) {
    const token &t = tokens[n];
    // The first argument of a module verb, or of `desc`, names something we
    // can check - so check it, and say so when it does not resolve.
    if (n == 1 && (takes_module(verb) || takes_query(verb))) {
      const std::string name = unquote(t.text);
      if (takes_query(verb)) {
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
    paint_argument(colors, index, t.begin, t.text, t.quoted, is_query, kv);
  }
  return colors;
}

keyword_vocabulary make_keyword_vocabulary(const std::vector<std::string> &fields) {
  keyword_vocabulary kv;
  // The registry spells a filter function with a trailing "()" and a variable
  // without (filter_handler_impl::get_filter_syntax), which is the only thing
  // that tells the two apart in the field list.
  for (const std::string &field : fields) {
    if (field.size() > 2 && field.compare(field.size() - 2, 2, "()") == 0) {
      kv.functions.insert(field.substr(0, field.size() - 2));
    } else {
      kv.variables.insert(field);
    }
  }
  // A query that reported no fields at all is not a filter based check (or is
  // not one we could read), so there is nothing to check names against and
  // nothing worth saying about them. The caller still caches the answer, so
  // this is not asked again.
  kv.complete = !kv.variables.empty() || !kv.functions.empty();
  return kv;
}

std::string needs_keywords(const std::string &input, const vocabulary &vocab) {
  const std::vector<token> tokens = tokenize(input);
  if (tokens.size() < 2) return "";
  const std::string verb = unquote(tokens[0].text);
  if (!contains(vocab.queries, verb)) return "";
  if (vocab.keywords.find(verb) != vocab.keywords.end()) return "";
  // Only once something on the line can actually use them: a query called
  // with nothing but a `drive=c:` never needs a keyword list, and asking for
  // one would be a registry round trip spent on nothing.
  for (std::size_t n = 1; n < tokens.size(); n++) {
    std::string text = tokens[n].text;
    if (text.size() >= 2 && is_quote(text[0]) && text[text.size() - 1] == text[0]) text = text.substr(1, text.size() - 2);
    const std::size_t eq = split_point(text);
    if (eq == std::string::npos) continue;
    if (takes_keywords(option_name(text.substr(0, eq)))) return verb;
  }
  return "";
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
  } else if (ctx.word_index == 1 && takes_query(ctx.command)) {
    candidates.insert(candidates.end(), vocab.queries.begin(), vocab.queries.end());
  } else if (ctx.word_index >= 1 && takes_module_filter(ctx.command)) {
    candidates.push_back("--all");
    candidates.push_back("--loaded");
    candidates.push_back("--unloaded");
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
    if (starts_with_ci(candidate, ctx.prefix)) matches.push_back(candidate);
  }
  // Nothing starts with what was typed, so try it as a substring: `load syst`
  // finds CheckSystem. Strictly a fallback, because a prefix that does match
  // is the stronger signal - mixing the two would dilute the common prefix
  // the editor extends the line by, and `load check` would stop filling in
  // "Check" the moment some unrelated module had "check" in the middle.
  if (matches.empty()) {
    for (const std::string &candidate : candidates) {
      if (contains_ci(candidate, ctx.prefix)) matches.push_back(candidate);
    }
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
