// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

// Provide the NSCAPI singleton so modern_filter.cpp can link.
// The core_wrapper is constructed with null function pointers, which means
// all log calls are harmless no-ops.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {
struct demand_object {
  std::string show() const { return "demand"; }
  long long cheap() const { return 1; }
  long long expensive() const { return 42; }
};
using demand_context = parsers::where::filter_handler_impl<std::shared_ptr<demand_object>>;
struct demand_handler : demand_context {
  bool needs_expensive = false;
  demand_handler() {
    registry_.add_int_var("cheap", &demand_object::cheap, "Cheap value").add_int_var("expensive", &demand_object::expensive, "Expensive value");
  }
  parsers::where::node_type create_variable(const std::string &name, bool human_readable) override {
    if (name == "expensive") needs_expensive = true;
    return demand_context::create_variable(name, human_readable);
  }
};
}  // namespace

TEST(ModernFilter, BindsOnlyRequestedValuesUntilHashOutputIsEnabled) {
  modern_filter::modern_filters<demand_object, demand_handler> filter;
  ASSERT_TRUE(filter.build_syntax(false, "${list}", "${cheap}", "", "", "", ""));
  EXPECT_FALSE(filter.context->needs_expensive);
  // Real-time events request hashes after syntax construction. They must
  // still bind and capture every keyword, including ones absent from syntax.
  filter.fetch_hash(true);
  EXPECT_TRUE(filter.context->needs_expensive);
  filter.start_match();
  filter.match(std::make_shared<demand_object>());
  ASSERT_EQ(filter.records_.size(), 1u);
  EXPECT_EQ(filter.records_.front().at("expensive"), "42");
}

// ============================================================================
// function_registry accessor adapters
// ============================================================================

namespace {
struct accessor_object {
  long long number = 7;
  std::string text = "seven";
  std::string show() const { return text; }
  long long get_number() const { return number; }
  std::string get_text() const { return text; }
};
using accessor_ptr = std::shared_ptr<accessor_object>;
using accessor_registry = parsers::where::function_registry<accessor_ptr>;

std::shared_ptr<parsers::where::filter_variable<accessor_ptr>> variable(const accessor_registry &registry, const std::string &key) {
  return registry.variables.at(key);
}
}  // namespace

TEST(FunctionRegistry, AcceptsEveryAccessorShape) {
  accessor_registry registry;
  const long long offset = 100;
  registry.add_int_var("member", &accessor_object::get_number, "member function pointer")
      .add_int_var("generic", [](auto o) { return static_cast<int>(o->number); }, "captureless generic lambda returning int")
      .add_int_var("by_ref", [](const accessor_ptr &o) { return o->number; }, "lambda taking the record by reference")
      .add_int_var("captured", [offset](auto o) { return o->number + offset; }, "lambda with a capture")
      .add_string_var("text", &accessor_object::get_text, "member function pointer")
      .add_float("half", [](auto o) { return o->number / 2.0; }, "float accessor")
      .add_string_var_w_context(
          "ctx", [](auto, parsers::where::evaluation_context context) { return std::string(context ? "context" : "none"); }, "context accessor");

  const auto obj = std::make_shared<accessor_object>();
  const parsers::where::evaluation_context no_context;
  EXPECT_EQ(variable(registry, "member")->i_function(obj, no_context), 7);
  EXPECT_EQ(variable(registry, "generic")->i_function(obj, no_context), 7);
  EXPECT_EQ(variable(registry, "by_ref")->i_function(obj, no_context), 7);
  EXPECT_EQ(variable(registry, "captured")->i_function(obj, no_context), 107);
  EXPECT_EQ(variable(registry, "text")->s_function(obj, no_context), "seven");
  EXPECT_DOUBLE_EQ(variable(registry, "half")->f_function(obj, no_context), 3.5);
  EXPECT_EQ(variable(registry, "ctx")->s_function(obj, no_context), "none");
}

TEST(FunctionRegistry, StatefulAccessorKeepsItsState) {
  accessor_registry registry;
  registry.add_int_var("calls", [n = 0LL](auto) mutable { return ++n; }, "counts its own calls");
  const auto obj = std::make_shared<accessor_object>();
  const auto var = variable(registry, "calls");
  EXPECT_EQ(var->i_function(obj, {}), 1);
  EXPECT_EQ(var->i_function(obj, {}), 2);
}

TEST(FunctionRegistry, ValueTypeIsNotMistakenForAnAccessor) {
  accessor_registry registry;
  registry.add_int_var("typed", parsers::where::type_bool, [](auto o) { return o->number; }, "value_type then accessor")
      .add_int_var("dual", [](auto o) { return o->number; }, [](auto o) { return o->text; }, "number and text accessors");
  const auto obj = std::make_shared<accessor_object>();
  EXPECT_EQ(variable(registry, "typed")->type, parsers::where::type_bool);
  EXPECT_FALSE(variable(registry, "typed")->s_function);
  EXPECT_EQ(variable(registry, "dual")->type, parsers::where::type_int);
  EXPECT_EQ(variable(registry, "dual")->i_function(obj, {}), 7);
  EXPECT_EQ(variable(registry, "dual")->s_function(obj, {}), "seven");
}

// The point of the adapters: an accessor that decays to a pointer is stored
// as one shared type, so a check with dozens of keywords instantiates the
// boost::function machinery once per accessor signature, not once per
// keyword. A capturing lambda cannot share and gets a type of its own.
TEST(FunctionRegistry, PointerLikeAccessorsShareOneStoredType) {
  accessor_registry registry;
  const long long offset = 1;
  registry.add_int_var("a", [](auto o) { return o->number; }, "")
      .add_int_var("b", [](auto o) { return o->number * 2; }, "")
      .add_int_var("c", &accessor_object::get_number, "")
      .add_int_var("d", [offset](auto o) { return o->number + offset; }, "");
  const auto type_of = [&](const char *key) -> const boost::core::typeinfo & { return variable(registry, key)->i_function.target_type(); };
  EXPECT_EQ(type_of("a"), type_of("b"));
  EXPECT_NE(type_of("a"), type_of("c"));  // a member pointer is a different signature
  EXPECT_NE(type_of("a"), type_of("d"));
}

// ============================================================================
// match_result tests
// ============================================================================

TEST(MatchResult, DefaultConstruction) {
  const modern_filter::match_result r;
  EXPECT_FALSE(r.matched_filter);
  EXPECT_FALSE(r.matched_bound);
  EXPECT_FALSE(r.is_done_);
}

TEST(MatchResult, TwoArgConstruction) {
  const modern_filter::match_result r(true, false);
  EXPECT_TRUE(r.matched_filter);
  EXPECT_FALSE(r.matched_bound);
  EXPECT_FALSE(r.is_done_);

  const modern_filter::match_result r2(false, true);
  EXPECT_FALSE(r2.matched_filter);
  EXPECT_TRUE(r2.matched_bound);
  EXPECT_FALSE(r2.is_done_);
}

TEST(MatchResult, CopyConstruction) {
  modern_filter::match_result original(true, true);
  original.is_done_ = true;

  const modern_filter::match_result copy(original);
  EXPECT_TRUE(copy.matched_filter);
  EXPECT_TRUE(copy.matched_bound);
  EXPECT_TRUE(copy.is_done_);
}

TEST(MatchResult, CopyAssignment) {
  modern_filter::match_result original(true, false);
  original.is_done_ = true;

  const modern_filter::match_result assigned = original;
  EXPECT_TRUE(assigned.matched_filter);
  EXPECT_FALSE(assigned.matched_bound);
  EXPECT_TRUE(assigned.is_done_);
}

TEST(MatchResult, AppendBothFalse) {
  modern_filter::match_result base;
  const modern_filter::match_result other;
  base.append(other);

  EXPECT_FALSE(base.matched_filter);
  EXPECT_FALSE(base.matched_bound);
  EXPECT_FALSE(base.is_done_);
}

TEST(MatchResult, AppendSetsFilterFromOther) {
  modern_filter::match_result base;
  const modern_filter::match_result other(true, false);
  base.append(other);

  EXPECT_TRUE(base.matched_filter);
  EXPECT_FALSE(base.matched_bound);
  EXPECT_FALSE(base.is_done_);
}

TEST(MatchResult, AppendSetsBoundFromOther) {
  modern_filter::match_result base;
  const modern_filter::match_result other(false, true);
  base.append(other);

  EXPECT_FALSE(base.matched_filter);
  EXPECT_TRUE(base.matched_bound);
  EXPECT_FALSE(base.is_done_);
}

TEST(MatchResult, AppendSetsDoneFromOther) {
  modern_filter::match_result base;
  modern_filter::match_result other;
  other.is_done_ = true;
  base.append(other);

  EXPECT_FALSE(base.matched_filter);
  EXPECT_FALSE(base.matched_bound);
  EXPECT_TRUE(base.is_done_);
}

TEST(MatchResult, AppendPreservesExistingTrue) {
  modern_filter::match_result base(true, true);
  base.is_done_ = true;
  const modern_filter::match_result other;
  base.append(other);

  EXPECT_TRUE(base.matched_filter);
  EXPECT_TRUE(base.matched_bound);
  EXPECT_TRUE(base.is_done_);
}

TEST(MatchResult, AppendCombinesBothTrue) {
  modern_filter::match_result base(true, false);
  modern_filter::match_result other(false, true);
  other.is_done_ = true;
  base.append(other);

  EXPECT_TRUE(base.matched_filter);
  EXPECT_TRUE(base.matched_bound);
  EXPECT_TRUE(base.is_done_);
}

TEST(MatchResult, AppendMultipleTimes) {
  modern_filter::match_result base;

  const modern_filter::match_result r1(true, false);
  base.append(r1);
  EXPECT_TRUE(base.matched_filter);
  EXPECT_FALSE(base.matched_bound);
  EXPECT_FALSE(base.is_done_);

  const modern_filter::match_result r2(false, true);
  base.append(r2);
  EXPECT_TRUE(base.matched_filter);
  EXPECT_TRUE(base.matched_bound);
  EXPECT_FALSE(base.is_done_);

  modern_filter::match_result r3;
  r3.is_done_ = true;
  base.append(r3);
  EXPECT_TRUE(base.matched_filter);
  EXPECT_TRUE(base.matched_bound);
  EXPECT_TRUE(base.is_done_);
}

TEST(MatchResult, AppendIsIdempotent) {
  modern_filter::match_result base(true, true);
  base.is_done_ = true;
  modern_filter::match_result other(true, true);
  other.is_done_ = true;
  base.append(other);

  EXPECT_TRUE(base.matched_filter);
  EXPECT_TRUE(base.matched_bound);
  EXPECT_TRUE(base.is_done_);
}

// ============================================================================
// error_handler_impl tests
// ============================================================================

TEST(ErrorHandlerImpl, InitialStateNoErrors) {
  const modern_filter::error_handler_impl handler(false);
  EXPECT_FALSE(handler.has_errors());
  EXPECT_TRUE(handler.get_errors().empty());
}

TEST(ErrorHandlerImpl, DebugFlagFromConstructor) {
  const modern_filter::error_handler_impl handler_no_debug(false);
  EXPECT_FALSE(handler_no_debug.is_debug());

  const modern_filter::error_handler_impl handler_debug(true);
  EXPECT_TRUE(handler_debug.is_debug());
}

TEST(ErrorHandlerImpl, SetDebug) {
  modern_filter::error_handler_impl handler(false);
  EXPECT_FALSE(handler.is_debug());

  handler.set_debug(true);
  EXPECT_TRUE(handler.is_debug());

  handler.set_debug(false);
  EXPECT_FALSE(handler.is_debug());
}

TEST(ErrorHandlerImpl, LogErrorStoresMessage) {
  modern_filter::error_handler_impl handler(false);
  handler.log_error("something went wrong");

  EXPECT_TRUE(handler.has_errors());
  EXPECT_EQ(handler.get_errors(), "something went wrong");
}

TEST(ErrorHandlerImpl, LogErrorOverwritesPreviousError) {
  modern_filter::error_handler_impl handler(false);
  handler.log_error("first error");
  handler.log_error("second error");

  EXPECT_TRUE(handler.has_errors());
  EXPECT_EQ(handler.get_errors(), "second error");
}

TEST(ErrorHandlerImpl, LogWarningDoesNotSetError) {
  modern_filter::error_handler_impl handler(false);
  handler.log_warning("a warning");

  EXPECT_FALSE(handler.has_errors());
  EXPECT_TRUE(handler.get_errors().empty());
}

TEST(ErrorHandlerImpl, LogDebugDoesNotSetError) {
  modern_filter::error_handler_impl handler(true);
  handler.log_debug("debug info");

  EXPECT_FALSE(handler.has_errors());
  EXPECT_TRUE(handler.get_errors().empty());
}

TEST(ErrorHandlerImpl, LogErrorAfterWarningAndDebug) {
  modern_filter::error_handler_impl handler(true);
  handler.log_warning("warn1");
  handler.log_debug("dbg1");
  EXPECT_FALSE(handler.has_errors());

  handler.log_error("real error");
  EXPECT_TRUE(handler.has_errors());
  EXPECT_EQ(handler.get_errors(), "real error");
}

TEST(ErrorHandlerImpl, LogErrorWithEmptyString) {
  modern_filter::error_handler_impl handler(false);
  handler.log_error("");

  // Empty string is stored — has_errors() checks !error.empty()
  EXPECT_FALSE(handler.has_errors());
  EXPECT_EQ(handler.get_errors(), "");
}

TEST(ErrorHandlerImpl, PolymorphicUsageThroughInterface) {
  std::shared_ptr<parsers::where::error_handler_interface> iface(new modern_filter::error_handler_impl(false));

  EXPECT_FALSE(iface->is_debug());
  iface->set_debug(true);
  EXPECT_TRUE(iface->is_debug());

  iface->log_error("interface error");
  // Downcast to check has_errors (not on the interface)
  auto *impl = dynamic_cast<modern_filter::error_handler_impl *>(iface.get());
  ASSERT_NE(impl, nullptr);
  EXPECT_TRUE(impl->has_errors());
  EXPECT_EQ(impl->get_errors(), "interface error");
}

// ============================================================================
// perf_config_parser tests
// ============================================================================

// Minimal factory exposing only what perf_config_parser::parse needs.
struct mock_perf_factory {
  std::map<std::string, std::map<std::string, std::string> > configs;
  void add_perf_config(const std::string &name, const std::map<std::string, std::string> &options) { configs[name] = options; }
};

namespace {
std::shared_ptr<parsers::where::error_handler_interface> make_error_handler() {
  return std::shared_ptr<parsers::where::error_handler_interface>(new modern_filter::error_handler_impl(false));
}
}  // namespace

// Regression test for "perf-config=none" reporting "Failed to parse syntax".
// "none" (and an empty string) must be accepted as "no perf-config" rather
// than fed to the grammar, which only accepts keyword(options) form.
TEST(PerfConfigParser, NoneIsAcceptedAndAddsNoConfig) {
  modern_filter::perf_config_parser<mock_perf_factory> parser;
  std::shared_ptr<mock_perf_factory> factory(new mock_perf_factory());
  auto error = make_error_handler();

  EXPECT_TRUE(parser.parse(factory, "none", error));
  EXPECT_TRUE(factory->configs.empty());
  EXPECT_FALSE(error->is_debug());
}

TEST(PerfConfigParser, EmptyStringIsAccepted) {
  modern_filter::perf_config_parser<mock_perf_factory> parser;
  std::shared_ptr<mock_perf_factory> factory(new mock_perf_factory());

  EXPECT_TRUE(parser.parse(factory, "", make_error_handler()));
  EXPECT_TRUE(factory->configs.empty());
}

TEST(PerfConfigParser, ValidConfigIsParsedAndStored) {
  modern_filter::perf_config_parser<mock_perf_factory> parser;
  std::shared_ptr<mock_perf_factory> factory(new mock_perf_factory());

  EXPECT_TRUE(parser.parse(factory, "cpu(unit:%)", make_error_handler()));
  ASSERT_EQ(1u, factory->configs.size());
  ASSERT_EQ(1u, factory->configs.count("cpu"));
  EXPECT_EQ("%", factory->configs["cpu"]["unit"]);
}

TEST(PerfConfigParser, InvalidConfigStillFails) {
  modern_filter::perf_config_parser<mock_perf_factory> parser;
  std::shared_ptr<mock_perf_factory> factory(new mock_perf_factory());
  auto error = make_error_handler();

  // Bare keyword without "(options)" is not "none" and is not valid syntax.
  EXPECT_FALSE(parser.parse(factory, "bogus", error));
}

// ============================================================================
// generic_summary list separator tests (issue #1370)
//
// %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and
// %(detail_list) are all assembled here, so the separator is configured once
// and every check that renders a list follows it.
// ============================================================================

namespace {
typedef parsers::where::generic_summary<int> test_summary;
}

TEST(GenericSummary, DefaultSeparatorIsCommaSpace) {
  test_summary summary;
  EXPECT_EQ(summary.list_separator, ", ");
  summary.matched("a");
  summary.matched("b");
  EXPECT_EQ(summary.get_list_match(), "a, b");
}

TEST(GenericSummary, SeparatorAppliesToEveryList) {
  test_summary summary;
  summary.list_separator = "\n";
  summary.matched("m1");
  summary.matched("m2");
  summary.matched_ok("o1");
  summary.matched_ok("o2");
  summary.matched_warn("w1");
  summary.matched_warn("w2");
  summary.matched_crit("c1");
  summary.matched_crit("c2");

  EXPECT_EQ(summary.get_list_match(), "m1\nm2");
  EXPECT_EQ(summary.get_list_ok(), "o1\no2");
  EXPECT_EQ(summary.get_list_warn(), "w1\nw2");
  EXPECT_EQ(summary.get_list_crit(), "c1\nc2");
  // problem_list interleaves warnings and criticals in arrival order.
  EXPECT_EQ(summary.get_list_problem(), "w1\nw2\nc1\nc2");
}

// The severity groups of detail_list break on the separator too, so a
// newline-separated list does not end up as one long "critical(...)" line.
TEST(GenericSummary, SeparatorAppliesBetweenDetailListGroups) {
  test_summary summary;
  summary.list_separator = "\n";
  summary.matched_crit("c1");
  summary.matched_warn("w1");
  summary.matched_ok("o1");
  EXPECT_EQ(summary.get_list_detail(), "critical(c1)\nwarning(w1)\no1");
}

TEST(GenericSummary, DetailListKeepsDefaultShape) {
  test_summary summary;
  summary.matched_crit("c1");
  summary.matched_warn("w1");
  summary.matched_ok("o1");
  EXPECT_EQ(summary.get_list_detail(), "critical(c1), warning(w1), o1");
}

// A single item is never prefixed or suffixed with the separator - the summary
// line stays clean when only one thing matched.
TEST(GenericSummary, SingleItemIsNotSeparated) {
  test_summary summary;
  summary.list_separator = "\n";
  summary.matched("only");
  EXPECT_EQ(summary.get_list_match(), "only");
}

// The separator is configuration: a filter that resets between runs (the
// real-time path reuses one filter instance) must keep it.
TEST(GenericSummary, ResetKeepsSeparator) {
  test_summary summary;
  summary.list_separator = "; ";
  summary.matched("a");
  summary.reset();
  EXPECT_EQ(summary.list_separator, "; ");
  summary.matched("b");
  summary.matched("c");
  EXPECT_EQ(summary.get_list_match(), "b; c");
}

TEST(GenericSummary, MultiCharSeparator) {
  test_summary summary;
  summary.list_separator = " | ";
  summary.matched("a");
  summary.matched("b");
  EXPECT_EQ(summary.get_list_match(), "a | b");
}

// %(sep) exposes the decoded separator to the templates. Templates are never
// escape-decoded (a literal C:\temp must survive), so this variable is the
// supported way to break the line before the first list item:
//   top-syntax=%(status): %(count) items:%(sep)%(list)
TEST(GenericSummary, SepIsExposedAsASummaryVariable) {
  test_summary summary;
  EXPECT_TRUE(summary.has_variable("sep"));
  EXPECT_EQ(1u, summary.get_filter_syntax().count("sep"));
}

TEST(GenericSummary, SepRendersTheCurrentSeparator) {
  test_summary summary;
  EXPECT_EQ(", ", summary.get_list_separator());
  summary.list_separator = "\n";
  EXPECT_EQ("\n", summary.get_list_separator());
}

// reset() must clear every list, problem_list included: the real-time path
// reuses one filter instance and resets it per event batch, so anything left
// behind leaks previous batches' items into every later message.
TEST(GenericSummary, ResetClearsEveryList) {
  test_summary summary;
  summary.matched("m1");
  summary.matched_ok("o1");
  summary.matched_warn("w1");
  summary.matched_crit("c1");
  EXPECT_EQ(summary.get_list_problem(), "w1, c1");

  summary.reset();

  EXPECT_EQ(summary.get_list_match(), "");
  EXPECT_EQ(summary.get_list_ok(), "");
  EXPECT_EQ(summary.get_list_warn(), "");
  EXPECT_EQ(summary.get_list_crit(), "");
  EXPECT_EQ(summary.get_list_problem(), "");
  EXPECT_EQ(summary.get_count_total(), 0);

  // And a later run starts clean.
  summary.matched_warn("w2");
  EXPECT_EQ(summary.get_list_problem(), "w2");
}
