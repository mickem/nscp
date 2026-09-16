// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Syntax analysis for a check's argument line: what each part of it means,
// and what could come next.
//
// This is a port of the interactive console's highlighter
// (modules/CommandClient/console_syntax.cpp) to the argument line the web UI
// edits. The console owns a whole command line - a verb, then its arguments -
// while here the command is already chosen by the page, so only the argument
// half is ported. Everything below that line is meant to behave identically to
// the C++: the two must agree, or the same filter would be painted one way in
// the prompt and another in the browser.
//
// Deliberately free of React and of the API layer: everything here is a pure
// function of the typed text plus a keyword snapshot, so it can be unit tested
// without rendering anything.

/** What a run of characters in the argument line means. */
export type TokenKind =
  // Whitespace and anything we have nothing to say about.
  | "plain"
  // An option name: `--foo`, or the `foo` of `foo=bar`.
  | "option"
  // A bare argument, or the `bar` of `foo=bar`.
  | "value"
  // A quoted run, quotes included.
  | "quoted"
  // The `=` joining an option to its value.
  | "punctuation"
  // Inside a filter expression (filter=, warning=, ...) or a syntax template
  // (top-syntax=, detail-syntax=, ...): a name the check actually offers as a
  // keyword, and one it does not. The second is the whole point - `fre < 10%`
  // is visibly wrong before you press Execute, which is otherwise a check that
  // runs and quietly matches nothing.
  | "keyword"
  | "unknownKeyword"
  // A filter function, `convert_size(...)`. Only the name is painted; its
  // arguments are classified on their own.
  | "function"
  // An operator or a keyword of the expression language itself: `<`, `like`,
  // `and`, `not in`, and the `${`/`%(`/`}`/`)` that delimit a template
  // reference.
  | "expressionOp"
  // A number (with its unit, if it has one) inside an expression.
  | "number";

/**
 * The filter keywords of one query: what a filter expression or a syntax
 * template may name. Both sets come from the same registry field list, split
 * on the trailing "()" the registry uses to mark a function.
 *
 * `complete` is false until the fetch has happened, and until it has, an
 * unrecognised name is merely unknown to us rather than wrong - calling it a
 * typo before we have looked would be crying wolf.
 */
export interface KeywordVocabulary {
  variables: Set<string>;
  functions: Set<string>;
  complete: boolean;
}

export const emptyKeywordVocabulary: KeywordVocabulary = {
  variables: new Set(),
  functions: new Set(),
  complete: false,
};

/**
 * Splits a query's registry field list into variables and functions. The
 * registry spells a filter function with a trailing "()" and a variable
 * without (filter_handler_impl::get_filter_syntax); that suffix is not part of
 * the name and is dropped here.
 */
export function makeKeywordVocabulary(fields: string[]): KeywordVocabulary {
  const variables = new Set<string>();
  const functions = new Set<string>();
  for (const field of fields) {
    if (field.length > 2 && field.endsWith("()")) {
      functions.add(field.slice(0, -2));
    } else {
      variables.add(field);
    }
  }
  // A query that reported no fields at all is not a filter based check (or is
  // not one we could read), so there is nothing to check names against and
  // nothing worth saying about them.
  return { variables, functions, complete: variables.size > 0 || functions.size > 0 };
}

// --- the grammar ------------------------------------------------------------

/**
 * Check options whose value is a filter expression in the where language - the
 * set modern_filter::cli_helper registers (add_filter_option, add_warn_option,
 * add_crit_option, add_ok_option), short aliases included.
 */
export function takesExpression(option: string): boolean {
  return (
    option === "filter" ||
    option === "warning" ||
    option === "warn" ||
    option === "critical" ||
    option === "crit" ||
    option === "ok"
  );
}

/**
 * ... and those whose value is a syntax template: literal text carrying
 * `${...}` and `%(...)` placeholders, each holding an expression in that same
 * language. `perf-config` is deliberately not here: it is a third grammar
 * whose keys name performance counters rather than filter keywords.
 */
export function takesTemplate(option: string): boolean {
  return (
    option === "top-syntax" ||
    option === "ok-syntax" ||
    option === "empty-syntax" ||
    option === "detail-syntax" ||
    option === "perf-syntax"
  );
}

export function takesKeywords(option: string): boolean {
  return takesExpression(option) || takesTemplate(option);
}

// The words the expression language reserves for itself: the boolean
// connectives, the spelled-out comparison operators, and `str`. Matched
// case-insensitively, as the grammar matches them (charset::no_case).
function isExpressionWord(lower: string): boolean {
  return (
    lower === "and" ||
    lower === "or" ||
    lower === "not" ||
    lower === "in" ||
    lower === "like" ||
    lower === "regexp" ||
    lower === "le" ||
    lower === "lt" ||
    lower === "eq" ||
    lower === "ne" ||
    lower === "ge" ||
    lower === "gt" ||
    lower === "str"
  );
}

/**
 * The option name as the parser sees it: `--filter`, `-filter` and `filter`
 * are the same option, and option names are lower case throughout.
 */
export function optionName(text: string): string {
  let start = 0;
  while (start < text.length && text[start] === "-") start++;
  return text.slice(start).toLowerCase();
}

// ASCII classes, matching the <cctype> calls in the C++ - the argument line is
// parsed byte-wise there, and a non-ASCII letter is not a keyword character in
// either.
const isWordChar = (c: string) => /[A-Za-z0-9_]/.test(c);
const isWordStart = (c: string) => /[A-Za-z_]/.test(c);
const isAlpha = (c: string) => /[A-Za-z]/.test(c);
const isDigit = (c: string) => /[0-9]/.test(c);
const isSpace = (c: string) => c === " " || c === "\t";
const isQuote = (c: string) => c === '"' || c === "'";

/** A token as typed: offsets into the line plus the raw text. */
export interface Token {
  begin: number;
  end: number;
  text: string;
  quoted: boolean;
}

/**
 * Split on unquoted whitespace. Quoting rules match
 * str::utils::parse_prompt_command closely enough for highlighting: a quote
 * runs to the matching quote, an unterminated quote runs to the end of the
 * line (which is exactly what the user sees while still typing it), and a
 * single quote only opens a string at the start of an argument or right after
 * its first `=` - elsewhere it is an ordinary character, so
 * `filter=core='total'` is one plain token.
 */
export function tokenize(input: string): Token[] {
  const tokens: Token[] = [];
  let i = 0;
  while (i < input.length) {
    while (i < input.length && isSpace(input[i])) i++;
    if (i >= input.length) break;
    const begin = i;
    let quote = "";
    let quoted = false;
    let equals = 0;
    while (i < input.length) {
      const c = input[i];
      const valueStart = i === begin || (equals === 1 && input[i - 1] === "=");
      if (quote !== "") {
        if (c === quote) quote = "";
      } else if (c === '"' || (c === "'" && valueStart)) {
        quote = c;
        quoted = true;
      } else if (isSpace(c)) {
        break;
      } else if (c === "=") {
        equals++;
      }
      i++;
    }
    tokens.push({ begin, end: i, text: input.slice(begin, i), quoted });
  }
  return tokens;
}

/**
 * Offset of the first unquoted `=` in `text`, or -1. Before the first `=` a
 * single quote only counts at the very start of the token (see tokenize).
 */
export function splitPoint(text: string): number {
  let quote = "";
  for (let i = 0; i < text.length; i++) {
    const c = text[i];
    if (quote !== "") {
      if (c === quote) quote = "";
    } else if (c === '"' || (c === "'" && i === 0)) {
      quote = c;
    } else if (c === "=") {
      return i;
    }
  }
  return -1;
}

/** Strip surrounding quotes, when the token is wrapped in a matched pair. */
export function unquote(text: string): string {
  if (text.length >= 2 && isQuote(text[0]) && text[text.length - 1] === text[0]) return text.slice(1, -1);
  return text;
}

// --- painting ---------------------------------------------------------------

function paint(kinds: TokenKind[], from: number, to: number, kind: TokenKind) {
  for (let i = Math.max(0, from); i < to && i < kinds.length; i++) kinds[i] = kind;
}

/**
 * What a name inside an expression is. `call` says it was followed by `(` and
 * so names a filter function rather than a variable.
 *
 * Silence is the default: with no keyword list yet (the first keystroke after
 * an `=`, or a query that turned out to declare none) nothing is known, and a
 * name we cannot check is not a name we can call wrong.
 */
function classifyKeyword(name: string, call: boolean, kv: KeywordVocabulary): TokenKind {
  // A dotted name is not a filter keyword at all - the grammar's variable_name
  // admits no '.' - so it reached the template from some other expansion
  // layer. Say nothing about it.
  if (name.includes(".")) return "plain";
  if (!kv.complete) return "plain";
  if ((call ? kv.functions : kv.variables).has(name)) return call ? "function" : "keyword";
  return "unknownKeyword";
}

/**
 * Paint `text` - which occupies the line from offset `from` - as a filter
 * expression, following include/parsers/where/grammar/grammar.cpp. Partial
 * input is the normal case here (the user is still typing), so every scan ends
 * politely at the end of the text rather than insisting on a closer.
 */
function paintExpression(kinds: TokenKind[], from: number, text: string, kv: KeywordVocabulary) {
  const n = text.length;
  let i = 0;
  while (i < n) {
    const c = text[i];
    if (isSpace(c)) {
      i++;
      continue;
    }
    // 'a string literal'. The grammar has no escapes inside one, so the first
    // closing quote ends it.
    if (c === "'") {
      let j = i + 1;
      while (j < n && text[j] !== "'") j++;
      if (j < n) j++;
      paint(kinds, from + i, from + j, "quoted");
      i = j;
      continue;
    }
    if (isWordStart(c)) {
      let j = i;
      while (j < n && (isWordChar(text[j]) || text[j] === ".")) j++;
      const word = text.slice(i, j);
      const lower = word.toLowerCase();
      if (isExpressionWord(lower)) {
        paint(kinds, from + i, from + j, "expressionOp");
        i = j;
        // `str(...)` takes a raw run of characters rather than an expression:
        // the grammar's string_literal_ex is everything up to the first ')'.
        if (lower === "str") {
          let k = j;
          while (k < n && isSpace(text[k])) k++;
          if (k < n && text[k] === "(") {
            let close = k + 1;
            while (close < n && text[close] !== ")") close++;
            if (close < n) close++;
            paint(kinds, from + k, from + close, "quoted");
            i = close;
          }
        }
        continue;
      }
      // A name followed by '(' is a function call. The grammar runs under a
      // space skipper, so `convert_size (size)` is one too - look past it.
      let k = j;
      while (k < n && isSpace(text[k])) k++;
      const call = k < n && text[k] === "(";
      paint(kinds, from + i, from + j, classifyKeyword(word, call, kv));
      i = j;
      continue;
    }
    if (isDigit(c)) {
      let j = i;
      while (j < n && (isDigit(text[j]) || text[j] === ".")) j++;
      // One trailing letter or '%' is the unit of the number+unit lexeme
      // (10%, 1G, 5m); a longer run is a separate token.
      if (j < n && (isAlpha(text[j]) || text[j] === "%") && (j + 1 >= n || !isWordChar(text[j + 1]))) j++;
      paint(kinds, from + i, from + j, "number");
      i = j;
      continue;
    }
    if (c === "<" || c === ">" || c === "!" || c === "=" || c === "&" || c === "|" || c === "(" || c === ")" || c === ",") {
      let j = i + 1;
      if ((c === "<" || c === ">" || c === "!") && j < n && text[j] === "=") j++;
      paint(kinds, from + i, from + j, "expressionOp");
      i = j;
      continue;
    }
    i++;
  }
}

/**
 * Where the `)` closing a `%(` placeholder is, mirroring
 * find_placeholder_close in parsers/expression/expression.cpp: balanced parens
 * with quoted strings skipped, falling back to the first `)`.
 */
function placeholderClose(text: string, bodyStart: number): number {
  const n = text.length;
  let j = bodyStart;
  let depth = 1;
  while (j < n && depth > 0) {
    const c = text[j];
    if (c === "'") {
      j++;
      while (j < n && text[j] !== "'") j++;
      if (j < n) j++;
      continue;
    }
    if (c === "(") {
      depth++;
    } else if (c === ")" && --depth === 0) {
      return j;
    }
    j++;
  }
  j = bodyStart;
  while (j < n && text[j] !== ")") j++;
  return j < n ? j : -1;
}

/**
 * Every `${...}` / `%(...)` placeholder in `text`, as offsets of its opener,
 * its body and its closer. Shared by the painter and by the cursor analysis,
 * which needs to know whether the caret sits inside one.
 */
function placeholders(text: string): { open: number; body: number; close: number }[] {
  const found: { open: number; body: number; close: number }[] = [];
  const n = text.length;
  let i = 0;
  while (i + 1 < n) {
    const dollar = text[i] === "$" && text[i + 1] === "{";
    const percent = text[i] === "%" && text[i + 1] === "(";
    if (!dollar && !percent) {
      i++;
      continue;
    }
    const body = i + 2;
    const close = dollar ? text.indexOf("}", body) : placeholderClose(text, body);
    if (close < 0 || close <= body) {
      // Still being typed, or empty. The engine emits the '$' or '%' as
      // literal text in that case, so leave it looking like one.
      i++;
      continue;
    }
    found.push({ open: i, body, close });
    i = close + 1;
  }
  return found;
}

/**
 * Paint `text` as a syntax template: literal text carrying `${...}` and
 * `%(...)` placeholders. The literal parts stay a plain value - they are
 * output, not code - and each placeholder body is an expression in its own
 * right.
 */
function paintTemplate(kinds: TokenKind[], from: number, text: string, kv: KeywordVocabulary) {
  paint(kinds, from, from + text.length, "value");
  for (const p of placeholders(text)) {
    paint(kinds, from + p.open, from + p.body, "expressionOp");
    paintExpression(kinds, from + p.body, text.slice(p.body, p.close), kv);
    paint(kinds, from + p.close, from + p.close + 1, "expressionOp");
  }
}

/**
 * Classify one argument token: `--flag`, `key=value`, `"quoted"` or a bare
 * word. `kv` is the query's keyword list, when it has been fetched.
 */
function paintArgument(kinds: TokenKind[], begin: number, text: string, quoted: boolean, kv: KeywordVocabulary) {
  const eq = splitPoint(text);
  if (eq < 0) {
    // `"filter=free < 10%"` - the whole argument wrapped in quotes, which is
    // how most command lines spell it - is a single token whose '=' sits
    // inside the quoted run, where splitPoint cannot see it. Unwrap and
    // classify what is inside; the quotes themselves stay quoted. Only when
    // there is an '=' in there: a plain quoted value is still a quoted value.
    if (text.length >= 2 && isQuote(text[0]) && text[text.length - 1] === text[0]) {
      const inner = text.slice(1, -1);
      if (splitPoint(inner) >= 0) {
        paint(kinds, begin, begin + 1, "quoted");
        paint(kinds, begin + text.length - 1, begin + text.length, "quoted");
        paintArgument(kinds, begin + 1, inner, false, kv);
        return;
      }
    }
    if (text.length > 0 && text[0] === "-") {
      paint(kinds, begin, begin + text.length, "option");
    } else {
      paint(kinds, begin, begin + text.length, quoted ? "quoted" : "value");
    }
    return;
  }
  paint(kinds, begin, begin + eq, "option");
  paint(kinds, begin + eq, begin + eq + 1, "punctuation");
  const value = text.slice(eq + 1);
  const valueQuoted = value.length > 0 && isQuote(value[0]);
  const option = optionName(text.slice(0, eq));
  if (takesKeywords(option)) {
    // A quoted value is an expression with quotes around it, not an opaque
    // string: colour the quotes and read what is between them.
    let bodyBegin = begin + eq + 1;
    let body = value;
    if (valueQuoted) {
      const closed = body.length >= 2 && body[body.length - 1] === body[0];
      paint(kinds, bodyBegin, bodyBegin + 1, "quoted");
      if (closed) paint(kinds, begin + text.length - 1, begin + text.length, "quoted");
      body = closed ? body.slice(1, -1) : body.slice(1);
      bodyBegin++;
    }
    if (takesExpression(option)) {
      paintExpression(kinds, bodyBegin, body, kv);
    } else {
      paintTemplate(kinds, bodyBegin, body, kv);
    }
    return;
  }
  paint(kinds, begin + eq + 1, begin + text.length, valueQuoted ? "quoted" : "value");
}

/** One TokenKind per character of `input`. */
export function classify(input: string, kv: KeywordVocabulary = emptyKeywordVocabulary): TokenKind[] {
  const kinds: TokenKind[] = new Array<TokenKind>(input.length).fill("plain");
  for (const t of tokenize(input)) paintArgument(kinds, t.begin, t.text, t.quoted, kv);
  return kinds;
}

/** A run of consecutive characters sharing one kind - what the editor draws. */
export interface Span {
  start: number;
  end: number;
  kind: TokenKind;
  text: string;
}

/** Merges the per-character kinds of `classify` into runs. */
export function toSpans(input: string, kinds: TokenKind[]): Span[] {
  const spans: Span[] = [];
  let start = 0;
  for (let i = 1; i <= input.length; i++) {
    if (i === input.length || kinds[i] !== kinds[start]) {
      spans.push({ start, end: i, kind: kinds[start], text: input.slice(start, i) });
      start = i;
    }
  }
  return spans;
}

/** Convenience: `input` straight to the runs the editor draws. */
export function highlight(input: string, kv: KeywordVocabulary = emptyKeywordVocabulary): Span[] {
  return toSpans(input, classify(input, kv));
}

// --- where the caret is -----------------------------------------------------

/** What completing the word under the caret should offer. */
export type CompletionTarget = "option" | "keyword" | "none";

export interface CursorContext {
  /** The option the caret sits in - its name or its value. "" when in neither. */
  option: string;
  /** True when the caret sits in the value of an option that takes keywords. */
  inKeywords: boolean;
  /** The word under the caret, which a completion replaces. */
  word: string;
  wordStart: number;
  wordEnd: number;
  target: CompletionTarget;
}

const noContext = (caret: number): CursorContext => ({
  option: "",
  inKeywords: false,
  word: "",
  wordStart: caret,
  wordEnd: caret,
  target: "option",
});

// The word around `caret` in `text` (which starts at `from` in the line),
// extended over word characters in both directions. Used inside expressions,
// where a name is a bare identifier.
function wordAround(text: string, from: number, caret: number) {
  let start = caret - from;
  let end = start;
  while (start > 0 && isWordChar(text[start - 1])) start--;
  while (end < text.length && isWordChar(text[end])) end++;
  return { word: text.slice(start, end), wordStart: from + start, wordEnd: from + end };
}

/**
 * Where the caret is, in terms of the grammar above: which option it is in,
 * whether that option's value is a filter expression or a template, and which
 * word a completion would replace. This is what lets the editor offer the
 * check's own parameters where an option name goes and its filter keywords
 * where a keyword goes - and what lets the page show the description of the
 * thing being typed.
 */
export function contextAt(input: string, caret: number): CursorContext {
  const tokens = tokenize(input);
  // The token the caret is in, or the one it is at the end of. A caret in
  // whitespace belongs to no token: a new argument is starting.
  const token = tokens.find((t) => caret >= t.begin && caret <= t.end);
  if (!token) return noContext(caret);
  return contextInToken(token.text, token.begin, caret);
}

function contextInToken(text: string, begin: number, caret: number): CursorContext {
  const eq = splitPoint(text);
  if (eq < 0) {
    // The whole argument wrapped in quotes - `"filter=free < 10%"` - hides its
    // '=' from splitPoint exactly as it does in paintArgument. Look inside.
    if (text.length >= 2 && isQuote(text[0]) && text[text.length - 1] === text[0] && splitPoint(text.slice(1, -1)) >= 0) {
      if (caret > begin && caret < begin + text.length) return contextInToken(text.slice(1, -1), begin + 1, caret);
      return noContext(caret);
    }
    // Still typing an option name. Leading dashes are not part of it, and are
    // left in place so a completion does not eat them.
    let start = 0;
    while (start < text.length && text[start] === "-") start++;
    return {
      option: "",
      inKeywords: false,
      word: text.slice(start),
      wordStart: begin + start,
      wordEnd: begin + text.length,
      target: "option",
    };
  }
  const option = optionName(text.slice(0, eq));
  if (caret <= begin + eq) {
    let start = 0;
    while (start < eq && text[start] === "-") start++;
    return {
      option,
      inKeywords: false,
      word: text.slice(start, eq),
      wordStart: begin + start,
      wordEnd: begin + eq,
      target: "option",
    };
  }
  if (!takesKeywords(option)) return { ...noContext(caret), option, target: "none" };

  let body = text.slice(eq + 1);
  let bodyBegin = begin + eq + 1;
  if (body.length > 0 && isQuote(body[0])) {
    const closed = body.length >= 2 && body[body.length - 1] === body[0];
    body = closed ? body.slice(1, -1) : body.slice(1);
    bodyBegin++;
  }
  if (caret < bodyBegin) return { ...noContext(caret), option, target: "none" };
  const offset = Math.min(caret, bodyBegin + body.length) - bodyBegin;

  if (takesExpression(option)) {
    return { option, inKeywords: true, ...wordAround(body, bodyBegin, bodyBegin + offset), target: "keyword" };
  }
  // In a template only the inside of a `${...}` / `%(...)` placeholder names a
  // keyword; the rest is literal output, where a completion would be noise.
  for (const p of placeholders(body)) {
    if (offset >= p.body && offset <= p.close) {
      return { option, inKeywords: true, ...wordAround(body, bodyBegin, bodyBegin + offset), target: "keyword" };
    }
  }
  // An unterminated placeholder - which is what one looks like while it is
  // being typed - counts from its opener to the end of the value.
  const open = Math.max(body.lastIndexOf("${", offset), body.lastIndexOf("%(", offset));
  if (open >= 0 && offset >= open + 2 && !/[})]/.test(body.slice(open + 2, offset))) {
    return { option, inKeywords: true, ...wordAround(body, bodyBegin, bodyBegin + offset), target: "keyword" };
  }
  return { ...noContext(caret), option, inKeywords: true, target: "none" };
}

// --- completion -------------------------------------------------------------

// Case-insensitive prefix test. The editor replaces exactly the typed prefix
// with the match, so completing "fre" to "free" also corrects the case.
function startsWithCi(candidate: string, prefix: string): boolean {
  return candidate.slice(0, prefix.length).toLowerCase() === prefix.toLowerCase();
}

function containsCi(candidate: string, needle: string): boolean {
  return candidate.toLowerCase().includes(needle.toLowerCase());
}

/**
 * The candidates matching `prefix`, sorted by name and de-duplicated by the
 * caller's ordering.
 *
 * Matched on the prefix first and, only when that finds nothing, anywhere in
 * the name - so `size` still resolves `convert_size`. Strictly a fallback,
 * because a prefix that does match is the stronger signal.
 */
export function filterCandidates<T>(candidates: T[], prefix: string, nameOf: (candidate: T) => string): T[] {
  let matches = candidates.filter((c) => startsWithCi(nameOf(c), prefix));
  if (matches.length === 0 && prefix !== "") matches = candidates.filter((c) => containsCi(nameOf(c), prefix));
  return [...matches].sort((a, b) => nameOf(a).localeCompare(nameOf(b)));
}
