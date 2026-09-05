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
  vocabulary vocab = make_vocabulary({"help", "exit", "desc", "load", "unload", "enable", "disable", "reload", "queries"},
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
  vocabulary vocab = make_vocabulary({"help", "exit", "desc", "load", "unload", "enable", "disable", "reload", "queries"},
                                     {"check_cpu", "check_drive", "check_uptime"}, {"CheckSystem"});
  vocab.modules.enabled.insert("CheckSystem");
  return vocab;
}

// The kind assigned to the code point at `offset`.
token_kind kind_at(const std::string &input, const std::size_t offset) {
  const std::vector<token_kind> kinds = classify(input, test_vocabulary());
  EXPECT_LT(offset, kinds.size()) << "offset past the end of '" << input << "'";
  if (offset >= kinds.size()) return token_kind::plain;
  return kinds[offset];
}

// Every code point of the run starting at `offset`, as one kind, or plain if
// the run is not uniform (which is a failure in every test that uses it).
token_kind kind_of_run(const std::string &input, const std::size_t offset, const std::size_t length) {
  const std::vector<token_kind> kinds = classify(input, test_vocabulary());
  EXPECT_LE(offset + length, kinds.size());
  if (offset + length > kinds.size()) return token_kind::plain;
  for (std::size_t i = 1; i < length; i++) {
    EXPECT_EQ(kinds[offset], kinds[offset + i]) << "run at " << offset << " of '" << input << "' is not uniform";
  }
  return kinds[offset];
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

TEST(ConsoleSyntaxClassify, QuotedValueIsAString) {
  const std::string input = "check_drive filter=\"free < 10%\"";
  EXPECT_EQ(kind_of_run(input, 12, 6), token_kind::option);
  EXPECT_EQ(kind_at(input, 18), token_kind::punctuation);
  EXPECT_EQ(kind_of_run(input, 19, 12), token_kind::quoted);
}

TEST(ConsoleSyntaxClassify, EqualsInsideQuotesIsNotASplit) {
  // A filter expression may contain '='; splitting on it would colour half of
  // the expression as an option name.
  const std::string input = "check_drive \"a=b\"";
  EXPECT_EQ(kind_of_run(input, 12, 5), token_kind::quoted);
}

TEST(ConsoleSyntaxClassify, WhitespaceStaysPlain) { EXPECT_EQ(kind_at("help me", 4), token_kind::plain); }

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
