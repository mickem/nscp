// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "console_syntax.hpp"

#include <gtest/gtest.h>

using namespace command_client;

namespace {

// CheckSystem is loaded and enabled; CheckDisk and CheckWMI are only sitting
// in the module directory. `complete` is set, i.e. the expensive full-module
// lookup has already run - see incomplete_vocabulary() for the other case.
vocabulary test_vocabulary() {
  vocabulary vocab = make_vocabulary({"help", "exit", "desc", "keywords", "load", "unload", "enable", "disable", "reload", "queries", "plugins", "modules"},
                                     {"check_cpu", "check_drive", "check_uptime"}, {"CheckSystem"});
  vocab.modules.enabled.insert("CheckSystem");
  vocab.modules.all.insert("CheckDisk");
  vocab.modules.all.insert("CheckWMI");
  vocab.modules.complete = true;
  return vocab;
}

// What the prompt knows before anyone has tabbed in a `load` argument: the
// loaded modules and nothing else.
vocabulary incomplete_vocabulary() {
  vocabulary vocab = make_vocabulary({"help", "exit", "desc", "keywords", "load", "unload", "enable", "disable", "reload", "queries", "plugins", "modules"},
                                     {"check_cpu", "check_drive", "check_uptime"}, {"CheckSystem"});
  vocab.modules.enabled.insert("CheckSystem");
  return vocab;
}

// test_vocabulary(), plus the filter keywords of check_drive - what the prompt
// knows once the highlighter has fetched them. check_cpu deliberately keeps
// none, so both sides of "have we looked yet" stay covered.
vocabulary keyword_vocabulary_for_check_drive() {
  vocabulary vocab = test_vocabulary();
  vocab.keywords["check_drive"] = make_keyword_vocabulary({"drive", "free", "size", "used", "convert_size()", "format_bytes()"});
  return vocab;
}

// The kind assigned to the code point at `offset`.
token_kind kind_at(const std::string &input, const std::size_t offset, const vocabulary &vocab = test_vocabulary()) {
  const std::vector<token_kind> kinds = classify(input, vocab);
  EXPECT_LT(offset, kinds.size()) << "offset past the end of '" << input << "'";
  if (offset >= kinds.size()) return token_kind::plain;
  return kinds[offset];
}

// Every code point of the run starting at `offset`, as one kind, or plain if
// the run is not uniform (which is a failure in every test that uses it).
token_kind kind_of_run(const std::string &input, const std::size_t offset, const std::size_t length, const vocabulary &vocab = test_vocabulary()) {
  const std::vector<token_kind> kinds = classify(input, vocab);
  EXPECT_LE(offset + length, kinds.size());
  if (offset + length > kinds.size()) return token_kind::plain;
  for (std::size_t i = 1; i < length; i++) {
    EXPECT_EQ(kinds[offset], kinds[offset + i]) << "run at " << offset << " of '" << input << "' is not uniform";
  }
  return kinds[offset];
}

// The kind of the first occurrence of `word`. A filter expression has far more
// columns than anyone can count by hand, and a test that miscounts one passes
// for the wrong reason.
token_kind kind_of(const std::string &input, const std::string &word, const vocabulary &vocab = test_vocabulary()) {
  const std::size_t offset = input.find(word);
  EXPECT_NE(offset, std::string::npos) << "'" << word << "' is not in '" << input << "'";
  if (offset == std::string::npos) return token_kind::plain;
  return kind_of_run(input, offset, word.size(), vocab);
}

std::vector<std::string> no_parameters(const std::string &) { return {}; }

}  // namespace

TEST(ConsoleSyntaxClassify, SizeMatchesCodePointsNotBytes) {
  // Two-byte code points: the colour buffer replxx hands us is sized in code
  // points, so one entry per byte would overrun it (and colour the wrong
  // columns on the way).
  EXPECT_EQ(classify("check_cpu \xc3\xa5\xc3\xa4", test_vocabulary()).size(), 12u);
  EXPECT_EQ(classify("", test_vocabulary()).size(), 0u);
  EXPECT_EQ(classify("help", test_vocabulary()).size(), 4u);
}

TEST(ConsoleSyntaxClassify, MultiByteArgumentDoesNotShiftLaterColours) {
  // "load" then a two-code-point non-ASCII name: the name must be flagged as
  // unknown across both code points, not across the four bytes it occupies.
  const std::string input = "load \xc3\xa5\xc3\xa4";
  const std::vector<token_kind> kinds = classify(input, test_vocabulary());
  ASSERT_EQ(kinds.size(), 7u);
  EXPECT_EQ(kinds[5], token_kind::unknown_name);
  EXPECT_EQ(kinds[6], token_kind::unknown_name);
}

TEST(ConsoleSyntaxClassify, BuiltinVerb) { EXPECT_EQ(kind_of_run("help", 0, 4), token_kind::builtin); }

TEST(ConsoleSyntaxClassify, KnownQueryInCommandPosition) { EXPECT_EQ(kind_of_run("check_cpu", 0, 9), token_kind::known_name); }

TEST(ConsoleSyntaxClassify, UnknownCommandIsFlagged) {
  // The point of the feature: a typo is visible before you press enter.
  EXPECT_EQ(kind_of_run("check_cpuu", 0, 10), token_kind::unknown_name);
}

TEST(ConsoleSyntaxClassify, ModuleArgumentIsCheckedForModuleVerbs) {
  // Known means "a module that exists", loaded or not - `load CheckDisk` is
  // exactly the case where the module is deliberately not loaded yet.
  EXPECT_EQ(kind_of_run("load CheckDisk", 5, 9), token_kind::known_name);
  EXPECT_EQ(kind_of_run("unload CheckNope", 7, 9), token_kind::unknown_name);
  EXPECT_EQ(kind_of_run("enable CheckSystem", 7, 11), token_kind::known_name);
  EXPECT_EQ(kind_of_run("disable CheckDisk", 8, 9), token_kind::known_name);
}

TEST(ConsoleSyntaxClassify, AnUnknownModuleIsOnlyFlaggedOnceWeHaveLookedAtThemAll) {
  // Crying wolf is worse than staying quiet: before the full module list has
  // been fetched, all we know about a name is that it is not loaded - which is
  // what you type after `load`.
  const std::vector<token_kind> incomplete = classify("load CheckDisk", incomplete_vocabulary());
  ASSERT_EQ(incomplete.size(), 14u);
  EXPECT_EQ(incomplete[5], token_kind::plain);

  const std::vector<token_kind> complete = classify("load CheckNope", test_vocabulary());
  ASSERT_EQ(complete.size(), 14u);
  EXPECT_EQ(complete[5], token_kind::unknown_name);
}

TEST(ConsoleSyntaxClassify, DescArgumentIsCheckedAgainstQueriesNotModules) {
  EXPECT_EQ(kind_of_run("desc check_drive", 5, 11), token_kind::known_name);
  // A module name is not a query, so `desc CheckDisk` is wrong and says so.
  EXPECT_EQ(kind_of_run("desc CheckDisk", 5, 9), token_kind::unknown_name);
}

TEST(ConsoleSyntaxClassify, QuotedModuleNameStillResolves) { EXPECT_EQ(kind_of_run("load \"CheckDisk\"", 5, 11), token_kind::known_name); }

TEST(ConsoleSyntaxClassify, KeyValueArgumentSplitsAroundEquals) {
  const std::string input = "check_drive drive=c:";
  EXPECT_EQ(kind_of_run(input, 12, 5), token_kind::option);
  EXPECT_EQ(kind_at(input, 17), token_kind::punctuation);
  EXPECT_EQ(kind_of_run(input, 18, 2), token_kind::value);
}

TEST(ConsoleSyntaxClassify, DashedOptionIsAnOption) { EXPECT_EQ(kind_of_run("check_drive --drive", 12, 7), token_kind::option); }

TEST(ConsoleSyntaxClassify, QuotedFilterValueIsReadThroughItsQuotes) {
  // The quotes are the prompt's, not the expression's: they stay coloured as a
  // string, and what they hold is still an expression.
  const std::string input = "check_drive filter=\"free < 10%\"";
  EXPECT_EQ(kind_of_run(input, 12, 6), token_kind::option);
  EXPECT_EQ(kind_at(input, 18), token_kind::punctuation);
  EXPECT_EQ(kind_at(input, 19), token_kind::quoted);
  EXPECT_EQ(kind_at(input, input.size() - 1), token_kind::quoted);
  EXPECT_EQ(kind_of(input, "<"), token_kind::expression_op);
  EXPECT_EQ(kind_of(input, "10%"), token_kind::number);
}

TEST(ConsoleSyntaxClassify, QuotedValueOfAnOpaqueOptionStaysAString) {
  const std::string input = "check_drive drive=\"C D\"";
  EXPECT_EQ(kind_of_run(input, 12, 5), token_kind::option);
  EXPECT_EQ(kind_at(input, 17), token_kind::punctuation);
  EXPECT_EQ(kind_of_run(input, 18, 5), token_kind::quoted);
}

TEST(ConsoleSyntaxClassify, WhollyQuotedArgumentStillSplitsAroundEquals) {
  // `"filter=..."` - the whole argument in quotes, which is how most command
  // lines spell it - reaches the check as the token `filter=...`
  // (str::utils::parse_prompt_command drops the quotes), so it is an option
  // and a value rather than one opaque string.
  const std::string input = "check_drive \"filter=free < 10%\"";
  EXPECT_EQ(kind_at(input, 12), token_kind::quoted);
  EXPECT_EQ(kind_at(input, input.size() - 1), token_kind::quoted);
  EXPECT_EQ(kind_of_run(input, 13, 6), token_kind::option);
  EXPECT_EQ(kind_at(input, 19), token_kind::punctuation);
  EXPECT_EQ(kind_of(input, "<"), token_kind::expression_op);
}

TEST(ConsoleSyntaxClassify, WhollyQuotedArgumentWithoutEqualsStaysAString) {
  // Nothing to split: a quoted bare argument is still one string.
  const std::string input = "check_drive \"some words\"";
  EXPECT_EQ(kind_of_run(input, 12, 12), token_kind::quoted);
}

TEST(ConsoleSyntaxClassify, EqualsInsideQuotesIsNotASplit) {
  // The tokenizer must not break the argument at the quoted '=': it stays one
  // token, whatever the highlighter then makes of its two halves.
  const std::string input = "check_drive \"a=b\"";
  EXPECT_EQ(kind_at(input, 12), token_kind::quoted);
  EXPECT_EQ(kind_at(input, 16), token_kind::quoted);
  EXPECT_EQ(kind_at(input, 13), token_kind::option);
  EXPECT_EQ(kind_at(input, 14), token_kind::punctuation);
  EXPECT_EQ(kind_at(input, 15), token_kind::value);
}

TEST(ConsoleSyntaxClassify, SingleQuotedValueIsAString) {
  const std::string input = "check_files path='C:\\x y'";
  EXPECT_EQ(kind_of_run(input, 12, 4), token_kind::option);
  EXPECT_EQ(kind_of_run(input, 17, 8), token_kind::quoted);
}

TEST(ConsoleSyntaxClassify, SingleQuoteInsideAValueIsOrdinary) {
  // The filter language's own string quotes: filter=core='total' is one
  // argument, and its value is an expression - so the 'total' in it is the
  // expression's string literal rather than the prompt's.
  const std::string input = "check_cpu filter=core='total'";
  EXPECT_EQ(kind_of_run(input, 10, 6), token_kind::option);
  EXPECT_EQ(kind_at(input, 16), token_kind::punctuation);
  EXPECT_EQ(kind_at(input, 21), token_kind::expression_op);
  EXPECT_EQ(kind_of(input, "'total'"), token_kind::quoted);
}

TEST(ConsoleSyntaxClassify, WhitespaceStaysPlain) { EXPECT_EQ(kind_at("help me", 4), token_kind::plain); }

// --- filter expressions ------------------------------------------------------

TEST(ConsoleSyntaxExpression, KnownKeywordIsAKeyword) {
  const std::string input = "check_drive filter=free < 10%";
  EXPECT_EQ(kind_of(input, "free", keyword_vocabulary_for_check_drive()), token_kind::keyword);
}

TEST(ConsoleSyntaxExpression, MisspeltKeywordIsFlagged) {
  // The case that earns the feature: `fre` parses, matches nothing, and the
  // check comes back a cheerful OK. Red before enter is the whole point.
  const std::string input = "check_drive filter=fre < 10%";
  EXPECT_EQ(kind_of(input, "fre", keyword_vocabulary_for_check_drive()), token_kind::unknown_keyword);
}

TEST(ConsoleSyntaxExpression, NothingIsFlaggedBeforeTheKeywordsAreFetched) {
  // Until the lookup has run we know nothing, and a name we cannot check is
  // not a name we may call wrong - the same restraint as an unloaded module.
  const std::string input = "check_drive \"filter=fre < 10%\"";
  EXPECT_EQ(kind_of(input, "fre"), token_kind::plain);
  // The shape of the expression needs no lookup, though, so it is coloured
  // from the first keystroke.
  EXPECT_EQ(kind_of(input, "<"), token_kind::expression_op);
  EXPECT_EQ(kind_of(input, "10%"), token_kind::number);
}

TEST(ConsoleSyntaxExpression, AnUnquotedExpressionIsStillSeveralArguments) {
  // The prompt splits on unquoted whitespace, so `filter=free < 10%` is three
  // arguments and only the first is a filter - which is exactly the shape the
  // check itself sees, and why the expression wants quoting.
  const std::string input = "check_drive filter=free < 10%";
  const vocabulary vocab = keyword_vocabulary_for_check_drive();
  EXPECT_EQ(kind_of(input, "free", vocab), token_kind::keyword);
  EXPECT_EQ(kind_of(input, "<", vocab), token_kind::value);
  EXPECT_EQ(kind_of(input, "10%", vocab), token_kind::value);
}

TEST(ConsoleSyntaxExpression, AQueryWithNoKeywordsFlagsNothing) {
  // An empty field list means "not a filter based check", which is a reason to
  // say nothing rather than to call every name in sight wrong.
  vocabulary vocab = test_vocabulary();
  vocab.keywords["check_cpu"] = make_keyword_vocabulary(std::vector<std::string>());
  EXPECT_EQ(kind_of("check_cpu filter=load", "load", vocab), token_kind::plain);
}

TEST(ConsoleSyntaxExpression, EveryExpressionOptionIsAnExpression) {
  const vocabulary vocab = keyword_vocabulary_for_check_drive();
  for (const std::string &option : std::vector<std::string>{"filter", "warning", "warn", "critical", "crit", "ok"}) {
    const std::string input = "check_drive " + option + "=fre";
    EXPECT_EQ(kind_of(input, "fre", vocab), token_kind::unknown_keyword) << "for option " << option;
  }
}

TEST(ConsoleSyntaxExpression, AnOpaqueOptionIsNotAnExpression) {
  EXPECT_EQ(kind_of("check_drive drive=fre", "fre", keyword_vocabulary_for_check_drive()), token_kind::value);
}

TEST(ConsoleSyntaxExpression, OnlyOnAQuery) {
  // `filter=` means nothing on a verb that is not a check, so its value is
  // just a value - and the verb is what is wrong on that line.
  const std::string input = "check_dive filter=fre";
  const vocabulary vocab = keyword_vocabulary_for_check_drive();
  EXPECT_EQ(kind_of_run(input, 0, 10, vocab), token_kind::unknown_name);
  EXPECT_EQ(kind_of(input, "fre", vocab), token_kind::value);
}

TEST(ConsoleSyntaxExpression, DashedOptionNamesTheSameOption) {
  EXPECT_EQ(kind_of("check_drive --filter=fre", "fre", keyword_vocabulary_for_check_drive()), token_kind::unknown_keyword);
}

TEST(ConsoleSyntaxExpression, ConnectivesAndWordOperatorsAreOperators) {
  const std::string input = "check_drive \"filter=size > 1G and free not like 'x'\"";
  const vocabulary vocab = keyword_vocabulary_for_check_drive();
  EXPECT_EQ(kind_of(input, "and", vocab), token_kind::expression_op);
  EXPECT_EQ(kind_of(input, "not", vocab), token_kind::expression_op);
  EXPECT_EQ(kind_of(input, "like", vocab), token_kind::expression_op);
  EXPECT_EQ(kind_of(input, "size", vocab), token_kind::keyword);
  EXPECT_EQ(kind_of(input, "free", vocab), token_kind::keyword);
  EXPECT_EQ(kind_of(input, "1G", vocab), token_kind::number);
  EXPECT_EQ(kind_of(input, "'x'", vocab), token_kind::quoted);
}

TEST(ConsoleSyntaxExpression, WordOperatorsAreCaseInsensitive) {
  // The grammar matches them under charset::no_case, so AND is an operator and
  // not a keyword that happens to be missing.
  const std::string input = "check_drive \"filter=size > 1G AND free LT 2G\"";
  const vocabulary vocab = keyword_vocabulary_for_check_drive();
  EXPECT_EQ(kind_of(input, "AND", vocab), token_kind::expression_op);
  EXPECT_EQ(kind_of(input, "LT", vocab), token_kind::expression_op);
}

TEST(ConsoleSyntaxExpression, TwoCharacterOperatorsAreOneToken) {
  const std::string input = "check_drive \"filter=size >= 1G\"";
  EXPECT_EQ(kind_of_run(input, input.find(">="), 2, keyword_vocabulary_for_check_drive()), token_kind::expression_op);
}

TEST(ConsoleSyntaxExpression, MultiLetterUnitIsFlagged) {
  // `size > 100GB` does not parse - the grammar's unit is a single letter - so
  // the `GB` lands as a name, and a name that is not a keyword is wrong. A
  // documented foot-gun that until now only surfaced at run time.
  const std::string input = "check_drive \"filter=size > 100GB\"";
  const vocabulary vocab = keyword_vocabulary_for_check_drive();
  EXPECT_EQ(kind_of(input, "100", vocab), token_kind::number);
  EXPECT_EQ(kind_of(input, "GB", vocab), token_kind::unknown_keyword);
}

TEST(ConsoleSyntaxExpression, FunctionCallIsAFunction) {
  const std::string input = "check_drive \"filter=convert_size(size) > 1G\"";
  const vocabulary vocab = keyword_vocabulary_for_check_drive();
  EXPECT_EQ(kind_of(input, "convert_size", vocab), token_kind::function);
  // Its argument is classified in its own right, not swallowed by the call.
  EXPECT_EQ(kind_of_run(input, input.find("(size)") + 1, 4, vocab), token_kind::keyword);
}

TEST(ConsoleSyntaxExpression, UnknownFunctionIsFlagged) {
  EXPECT_EQ(kind_of("check_drive \"filter=convert_bytes(size) > 1G\"", "convert_bytes", keyword_vocabulary_for_check_drive()), token_kind::unknown_keyword);
}

TEST(ConsoleSyntaxExpression, AFunctionNameIsNotAVariableName) {
  // The registry keeps the two apart (a function carries a trailing "()"), and
  // so does the highlighter: `convert_size` alone is not a keyword.
  EXPECT_EQ(kind_of("check_drive filter=convert_size > 1G", "convert_size", keyword_vocabulary_for_check_drive()), token_kind::unknown_keyword);
}

TEST(ConsoleSyntaxExpression, StrTakesARawRun) {
  // string_literal_ex is everything up to the first ')', not an expression, so
  // a name inside it is text and must not be flagged.
  const std::string input = "check_drive \"filter=drive = str(C:)\"";
  const vocabulary vocab = keyword_vocabulary_for_check_drive();
  EXPECT_EQ(kind_of(input, "str", vocab), token_kind::expression_op);
  EXPECT_EQ(kind_of(input, "(C:)", vocab), token_kind::quoted);
}

TEST(ConsoleSyntaxExpression, UnterminatedStringRunsToTheEnd) {
  // Every line is partial while it is being typed; an unterminated literal
  // keeps its colour to the end rather than losing it.
  const std::string input = "check_drive \"filter=drive = 'C\"";
  EXPECT_EQ(kind_of(input, "'C", keyword_vocabulary_for_check_drive()), token_kind::quoted);
}

// --- syntax templates --------------------------------------------------------

TEST(ConsoleSyntaxTemplate, DollarBracePlaceholderResolves) {
  const std::string input = "check_drive \"detail-syntax=${free} free on ${drive}\"";
  const vocabulary vocab = keyword_vocabulary_for_check_drive();
  EXPECT_EQ(kind_of(input, "${", vocab), token_kind::expression_op);
  EXPECT_EQ(kind_of_run(input, input.find("${free}") + 2, 4, vocab), token_kind::keyword);
  EXPECT_EQ(kind_at(input, input.find("${free}") + 6, vocab), token_kind::expression_op);
  EXPECT_EQ(kind_of_run(input, input.find("${drive}") + 2, 5, vocab), token_kind::keyword);
  // The literal text between placeholders is output, not code.
  EXPECT_EQ(kind_of_run(input, input.find(" free on "), 9, vocab), token_kind::value);
}

TEST(ConsoleSyntaxTemplate, PercentParenPlaceholderResolves) {
  const std::string input = "check_drive \"top-syntax=%(status): %(count)/%(total)\"";
  vocabulary vocab = test_vocabulary();
  // The generic summary keywords arrive in the same field list as the check's
  // own, so they resolve the same way.
  vocab.keywords["check_drive"] = make_keyword_vocabulary({"drive", "free", "status", "count", "total"});
  EXPECT_EQ(kind_of(input, "%(", vocab), token_kind::expression_op);
  EXPECT_EQ(kind_of_run(input, input.find("%(status)") + 2, 6, vocab), token_kind::keyword);
  EXPECT_EQ(kind_of_run(input, input.find("%(count)") + 2, 5, vocab), token_kind::keyword);
  EXPECT_EQ(kind_of_run(input, input.find("%(total)") + 2, 5, vocab), token_kind::keyword);
}

TEST(ConsoleSyntaxTemplate, MisspeltPlaceholderIsFlagged) {
  const std::string input = "check_drive detail-syntax=${fre}";
  EXPECT_EQ(kind_of_run(input, input.find("${fre}") + 2, 3, keyword_vocabulary_for_check_drive()), token_kind::unknown_keyword);
}

TEST(ConsoleSyntaxTemplate, EveryTemplateOptionIsATemplate) {
  const vocabulary vocab = keyword_vocabulary_for_check_drive();
  for (const std::string &option : std::vector<std::string>{"top-syntax", "ok-syntax", "empty-syntax", "detail-syntax", "perf-syntax"}) {
    const std::string input = "check_drive " + option + "=${fre}";
    EXPECT_EQ(kind_of_run(input, input.find("${fre}") + 2, 3, vocab), token_kind::unknown_keyword) << "for option " << option;
  }
}

TEST(ConsoleSyntaxTemplate, PlaceholderBodyIsAnExpression) {
  // %(...) holds an expression, not merely a name - nested calls included
  // (the balanced-paren rule of issue #281).
  const std::string input = "check_drive \"detail-syntax=%(convert_size(free))\"";
  const vocabulary vocab = keyword_vocabulary_for_check_drive();
  EXPECT_EQ(kind_of(input, "convert_size", vocab), token_kind::function);
  EXPECT_EQ(kind_of_run(input, input.find("(free)") + 1, 4, vocab), token_kind::keyword);
  // The outer ')' closes the placeholder; the inner one closes the call.
  EXPECT_EQ(kind_at(input, input.size() - 2, vocab), token_kind::expression_op);
}

TEST(ConsoleSyntaxTemplate, UnterminatedPlaceholderStaysLiteral) {
  // With no '}' the engine emits the bare '$' as literal text, so the prompt
  // shows it as literal text too.
  const std::string input = "check_drive detail-syntax=${free";
  EXPECT_EQ(kind_of_run(input, input.find("${free"), 6, keyword_vocabulary_for_check_drive()), token_kind::value);
}

TEST(ConsoleSyntaxTemplate, LiteralTextIsNotAKeyword) {
  // Prose outside a placeholder is output. Flagging the words in it would turn
  // every message red.
  EXPECT_EQ(kind_of("check_drive \"detail-syntax=fre is not a keyword here\"", "fre", keyword_vocabulary_for_check_drive()), token_kind::value);
}

TEST(ConsoleSyntaxTemplate, DottedNameIsNotFlagged) {
  // No filter keyword has a '.' in it, so a dotted name came from some other
  // expansion layer and is none of our business.
  EXPECT_EQ(kind_of("check_drive detail-syntax=${host.name}", "host.name", keyword_vocabulary_for_check_drive()), token_kind::plain);
}

// --- the keyword vocabulary, and when it is fetched --------------------------

TEST(ConsoleSyntaxKeywords, FunctionsAreSplitOutByTheirParens) {
  const keyword_vocabulary kv = make_keyword_vocabulary({"free", "size", "convert_size()"});
  EXPECT_EQ(kv.variables, (std::set<std::string>{"free", "size"}));
  EXPECT_EQ(kv.functions, (std::set<std::string>{"convert_size"}));
  EXPECT_TRUE(kv.complete);
}

TEST(ConsoleSyntaxKeywords, AnEmptyFieldListIsNotAnAnswer) { EXPECT_FALSE(make_keyword_vocabulary(std::vector<std::string>()).complete); }

TEST(ConsoleSyntaxKeywords, NeededOnlyByAnOptionThatCouldUseThem) {
  const vocabulary vocab = test_vocabulary();
  EXPECT_EQ(needs_keywords("check_drive filter=", vocab), "check_drive");
  EXPECT_EQ(needs_keywords("check_drive detail-syntax=${", vocab), "check_drive");
  EXPECT_EQ(needs_keywords("check_drive \"filter=free\"", vocab), "check_drive");
  // Nothing on these lines could use a keyword, so nothing is fetched - the
  // lookup is a registry round trip and it happens on the drawing thread.
  EXPECT_EQ(needs_keywords("check_drive", vocab), "");
  EXPECT_EQ(needs_keywords("check_drive drive=c:", vocab), "");
  EXPECT_EQ(needs_keywords("check_drive filter", vocab), "");
  EXPECT_EQ(needs_keywords("desc check_drive", vocab), "");
  EXPECT_EQ(needs_keywords("check_dive filter=free", vocab), "");
}

TEST(ConsoleSyntaxKeywords, NotFetchedTwice) {
  vocabulary vocab = test_vocabulary();
  vocab.keywords["check_drive"] = make_keyword_vocabulary({"free"});
  EXPECT_EQ(needs_keywords("check_drive filter=free", vocab), "");
}

TEST(ConsoleSyntaxAnalyze, EmptyInput) {
  const completion_context ctx = analyze("");
  EXPECT_EQ(ctx.word_index, 0);
  EXPECT_EQ(ctx.prefix, "");
  EXPECT_EQ(ctx.command, "");
}

TEST(ConsoleSyntaxAnalyze, PartialCommand) {
  const completion_context ctx = analyze("che");
  EXPECT_EQ(ctx.word_index, 0);
  EXPECT_EQ(ctx.prefix, "che");
  EXPECT_EQ(ctx.command, "");
}

TEST(ConsoleSyntaxAnalyze, TrailingSpaceStartsANewWord) {
  const completion_context ctx = analyze("load ");
  EXPECT_EQ(ctx.word_index, 1);
  EXPECT_EQ(ctx.prefix, "");
  EXPECT_EQ(ctx.command, "load");
}

TEST(ConsoleSyntaxAnalyze, SecondWordInProgress) {
  const completion_context ctx = analyze("load Check");
  EXPECT_EQ(ctx.word_index, 1);
  EXPECT_EQ(ctx.prefix, "Check");
  EXPECT_EQ(ctx.command, "load");
}

TEST(ConsoleSyntaxComplete, CommandPositionOffersBuiltinsAndQueries) {
  const std::vector<std::string> matches = complete("che", test_vocabulary(), no_parameters);
  EXPECT_EQ(matches, (std::vector<std::string>{"check_cpu", "check_drive", "check_uptime"}));
}

TEST(ConsoleSyntaxComplete, CommandPositionIncludesBuiltins) {
  const std::vector<std::string> matches = complete("e", test_vocabulary(), no_parameters);
  EXPECT_EQ(matches, (std::vector<std::string>{"enable", "exit"}));
}

TEST(ConsoleSyntaxComplete, ResultsAreSortedAndUnique) {
  // "queries" is a builtin; nothing should appear twice even if a query of the
  // same name were ever registered.
  vocabulary vocab = test_vocabulary();
  vocab.queries.insert("queries");
  const std::vector<std::string> matches = complete("quer", vocab, no_parameters);
  EXPECT_EQ(matches, (std::vector<std::string>{"queries"}));
}

TEST(ConsoleSyntaxComplete, LoadOffersWhatIsNotLoaded) {
  // The point: CheckSystem is already loaded, so offering it under `load` is
  // offering the one module that cannot usefully be loaded.
  EXPECT_EQ(complete("load Check", test_vocabulary(), no_parameters), (std::vector<std::string>{"CheckDisk", "CheckWMI"}));
}

TEST(ConsoleSyntaxComplete, ModulePrefixMatchesRegardlessOfCase) {
  // The typed prefix is what the editor replaces, so a match in another case
  // corrects what was typed: "load check" becomes "load CheckDisk".
  EXPECT_EQ(complete("load check", test_vocabulary(), no_parameters), (std::vector<std::string>{"CheckDisk", "CheckWMI"}));
  EXPECT_EQ(complete("load CHECKd", test_vocabulary(), no_parameters), (std::vector<std::string>{"CheckDisk"}));
  EXPECT_EQ(complete("unload checksys", test_vocabulary(), no_parameters), (std::vector<std::string>{"CheckSystem"}));
  EXPECT_TRUE(complete("load checkx", test_vocabulary(), no_parameters).empty());
}

TEST(ConsoleSyntaxComplete, PluginsOffersItsFilters) {
  const std::vector<std::string> all{"--all", "--loaded", "--unloaded"};
  EXPECT_EQ(complete("plugins ", test_vocabulary(), no_parameters), all);
  EXPECT_EQ(complete("modules ", test_vocabulary(), no_parameters), all);
  EXPECT_EQ(complete("plugins --l", test_vocabulary(), no_parameters), (std::vector<std::string>{"--loaded"}));
  // The substring fallback covers the spelling without the dashes.
  EXPECT_EQ(complete("plugins unload", test_vocabulary(), no_parameters), (std::vector<std::string>{"--unloaded"}));
  // More than one is accepted, so every position offers them.
  EXPECT_EQ(complete("plugins --all --l", test_vocabulary(), no_parameters), (std::vector<std::string>{"--loaded"}));
  // ... but it is not a verb that takes a module name.
  EXPECT_TRUE(complete("plugins Check", test_vocabulary(), no_parameters).empty());
}

TEST(ConsoleSyntaxComplete, ModuleMatchesOnASubstringWhenNoPrefixDoes) {
  // `load syst` has to find CheckSystem: every module is Check-something, so
  // the part anyone remembers is never the part they have to type first.
  EXPECT_EQ(complete("unload syst", test_vocabulary(), no_parameters), (std::vector<std::string>{"CheckSystem"}));
  EXPECT_EQ(complete("disable SYST", test_vocabulary(), no_parameters), (std::vector<std::string>{"CheckSystem"}));
  EXPECT_EQ(complete("load disk", test_vocabulary(), no_parameters), (std::vector<std::string>{"CheckDisk"}));
  EXPECT_EQ(complete("load WMI", test_vocabulary(), no_parameters), (std::vector<std::string>{"CheckWMI"}));
  // Still nothing for a name that is nowhere in the list.
  EXPECT_TRUE(complete("load checkx", test_vocabulary(), no_parameters).empty());
  EXPECT_TRUE(complete("load nsca", test_vocabulary(), no_parameters).empty());
}

TEST(ConsoleSyntaxComplete, APrefixMatchKeepsTheSubstringsOut) {
  // The fallback only runs when the prefix finds nothing. Were the two mixed,
  // the common prefix the editor extends the line by would collapse as soon
  // as one unrelated module had "check" somewhere in the middle.
  EXPECT_EQ(complete("load check", test_vocabulary(), no_parameters), (std::vector<std::string>{"CheckDisk", "CheckWMI"}));
  EXPECT_EQ(complete("load c", test_vocabulary(), no_parameters), (std::vector<std::string>{"CheckDisk", "CheckWMI"}));
}

TEST(ConsoleSyntaxComplete, QueriesAndParametersMatchOnASubstringToo) {
  // Same rule everywhere it completes a name: every query is check_something,
  // and an option name is as easy to remember from the middle.
  EXPECT_EQ(complete("cpu", test_vocabulary(), no_parameters), (std::vector<std::string>{"check_cpu"}));
  EXPECT_EQ(complete("desc drive", test_vocabulary(), no_parameters), (std::vector<std::string>{"check_drive"}));
  const auto parameters = [](const std::string &query) -> std::vector<std::string> {
    if (query == "check_drive") return {"drive", "filter", "warning"};
    return {};
  };
  EXPECT_EQ(complete("check_drive lter", test_vocabulary(), parameters), (std::vector<std::string>{"filter="}));
}

TEST(ConsoleSyntaxComplete, CommandPrefixMatchesRegardlessOfCase) {
  const std::vector<std::string> lower = complete("che", test_vocabulary(), no_parameters);
  EXPECT_EQ(complete("CHE", test_vocabulary(), no_parameters), lower);
  EXPECT_FALSE(lower.empty());
}

TEST(ConsoleSyntaxComplete, KeywordsTakesAQueryLikeDesc) {
  EXPECT_EQ(complete("keywords che", test_vocabulary(), no_parameters), complete("desc che", test_vocabulary(), no_parameters));
  EXPECT_FALSE(complete("keywords che", test_vocabulary(), no_parameters).empty());
}

TEST(ConsoleSyntaxComplete, UnloadOffersWhatIsLoaded) {
  EXPECT_EQ(complete("unload Check", test_vocabulary(), no_parameters), (std::vector<std::string>{"CheckSystem"}));
}

TEST(ConsoleSyntaxComplete, EnableAndDisableGoByTheConfiguredState) {
  // enable/disable act on the configuration, not on what happens to be running
  // - so they read the enabled set, not the loaded one.
  EXPECT_EQ(complete("enable Check", test_vocabulary(), no_parameters), (std::vector<std::string>{"CheckDisk", "CheckWMI"}));
  EXPECT_EQ(complete("disable Check", test_vocabulary(), no_parameters), (std::vector<std::string>{"CheckSystem"}));
}

TEST(ConsoleSyntaxComplete, LoadOffersNothingUntilTheModuleListIsComplete) {
  // Everything we know about is loaded, so there is nothing to load; the
  // editor is expected to have fetched the full list before asking.
  EXPECT_TRUE(complete("load Check", incomplete_vocabulary(), no_parameters).empty());
  // unload still works from the cheap half.
  EXPECT_EQ(complete("unload Check", incomplete_vocabulary(), no_parameters), (std::vector<std::string>{"CheckSystem"}));
}

TEST(ConsoleSyntaxNeedsAllModules, OnlyForTheVerbsThatOfferAbsentModules) {
  EXPECT_TRUE(needs_all_modules("load Check"));
  EXPECT_TRUE(needs_all_modules("load "));
  EXPECT_TRUE(needs_all_modules("enable Check"));
  // These answer from the loaded/enabled sets, which cost nothing.
  EXPECT_FALSE(needs_all_modules("unload Check"));
  EXPECT_FALSE(needs_all_modules("disable Check"));
  // Not a module position at all.
  EXPECT_FALSE(needs_all_modules("load"));
  EXPECT_FALSE(needs_all_modules("desc check_cpu"));
  EXPECT_FALSE(needs_all_modules("check_drive drive=c:"));
  EXPECT_FALSE(needs_all_modules(""));
}

TEST(ConsoleSyntaxComplete, DescOffersQueries) {
  EXPECT_EQ(complete("desc check_d", test_vocabulary(), no_parameters), (std::vector<std::string>{"check_drive"}));
}

TEST(ConsoleSyntaxComplete, ArgumentPositionOffersQueryParameters) {
  const auto parameters = [](const std::string &query) -> std::vector<std::string> {
    if (query == "check_drive") return {"drive", "filter", "warning"};
    return {};
  };
  // Offered as `name=`, because that is the whole token the user needs.
  EXPECT_EQ(complete("check_drive ", test_vocabulary(), parameters), (std::vector<std::string>{"drive=", "filter=", "warning="}));
  EXPECT_EQ(complete("check_drive f", test_vocabulary(), parameters), (std::vector<std::string>{"filter="}));
}

TEST(ConsoleSyntaxComplete, NoParameterCompletionOnceTheValueStarts) {
  const auto parameters = [](const std::string &) -> std::vector<std::string> { return {"drive", "filter"}; };
  EXPECT_TRUE(complete("check_drive filter=fr", test_vocabulary(), parameters).empty());
}

TEST(ConsoleSyntaxComplete, NoParametersForAnUnknownCommand) {
  const auto parameters = [](const std::string &) -> std::vector<std::string> { return {"drive"}; };
  EXPECT_TRUE(complete("check_nope ", test_vocabulary(), parameters).empty());
}

TEST(ConsoleSyntaxComplete, HandlesAMissingParameterCallback) { EXPECT_TRUE(complete("check_drive ", test_vocabulary(), nullptr).empty()); }

TEST(ConsoleSyntaxHint, NothingWhileTheCommandIsStillBeingTyped) {
  const auto describe = [](const std::string &) { return std::string("check the cpu"); };
  EXPECT_EQ(hint("check_cpu", test_vocabulary(), describe), "");
}

TEST(ConsoleSyntaxHint, DescriptionOnceTheCommandIsSettled) {
  const auto describe = [](const std::string &name) { return name == "check_cpu" ? std::string("check the cpu") : std::string(); };
  EXPECT_EQ(hint("check_cpu ", test_vocabulary(), describe), "  check the cpu");
}

TEST(ConsoleSyntaxHint, FirstLineOnly) {
  // Registry descriptions run to several paragraphs; the hint is drawn on the
  // prompt line and must stay on it.
  const auto describe = [](const std::string &) { return std::string("short summary\nand a long tail\nover several lines"); };
  EXPECT_EQ(hint("check_cpu warn=1", test_vocabulary(), describe), "  short summary");
}

TEST(ConsoleSyntaxHint, NothingForAnUnknownCommand) {
  const auto describe = [](const std::string &) { return std::string("never shown"); };
  EXPECT_EQ(hint("check_nope ", test_vocabulary(), describe), "");
}

TEST(ConsoleSyntaxHint, HandlesAMissingDescribeCallback) { EXPECT_EQ(hint("check_cpu ", test_vocabulary(), nullptr), ""); }
