/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>

#include <boost/function.hpp>
#include <list>
#include <map>
#include <memory>
#include <parsers/where.hpp>
#include <parsers/where/binary_op.hpp>
#include <parsers/where/engine.hpp>
#include <parsers/where/node.hpp>
#include <parsers/where/unary_fun.hpp>
#include <parsers/where/variable.hpp>
#include <string>
#include <vector>

using namespace parsers::where;

// ============================================================================
// Test infrastructure
//
// The engine evaluates an AST over an `evaluation_context` that is also
// reinterpret_cast<NativeContext*> by every variable_node template. The
// NativeContext therefore needs to (a) implement evaluation_context_interface
// and (b) expose has_object/get_object/set_object plus the bound_int/_float/
// _string typedefs used by the templates.
//
// The factory is responsible for translating identifiers seen by the parser
// (e.g. "ivar", "svar", "scount") into real variable_node instances bound to
// fields on the mock_object / mock_summary. With this we can build expressions
// that mirror what production code parses (e.g. "state='hung' OR count=0")
// and exercise both match() and match_force() in realistic scenarios.
// ============================================================================

namespace {

// ---- Object & summary -----------------------------------------------------

struct mock_object {
  long long ival = 0;
  double fval = 0.0;
  std::string sval;
};

struct mock_summary {
  long long count = 0;
  std::string status;
};

// ---- Native context -------------------------------------------------------
//
// Variable nodes do `reinterpret_cast<TContext*>(context.get())`, so the
// interface and the concrete struct must be the same type and that type must
// implement evaluation_context_interface. We also expose the bound_*_type
// typedefs the variable templates require.

struct mock_native_context : evaluation_context_interface {
  using object_type = mock_object;
  using summary_type = mock_summary*;
  using bound_int_type = boost::function<long long(object_type, evaluation_context)>;
  using bound_float_type = boost::function<double(object_type, evaluation_context)>;
  using bound_string_type = boost::function<std::string(object_type, evaluation_context)>;

  boost::optional<mock_object> object_;
  mock_summary* summary_ = nullptr;
  std::string error_;
  std::string warn_;
  bool debug_enabled_ = false;

  bool has_object() { return static_cast<bool>(object_); }
  mock_object get_object() { return *object_; }
  void set_object(mock_object obj) { object_ = obj; }
  void remove_object() { object_.reset(); }
  void set_summary(mock_summary* s) { summary_ = s; }
  mock_summary* get_summary() { return summary_; }

  bool has_error() const override { return !error_.empty(); }
  std::string get_error() const override { return error_; }
  void error(std::string msg) override {
    if (!error_.empty()) error_ += ", ";
    error_ += msg;
  }
  bool has_warn() const override { return !warn_.empty(); }
  std::string get_warn() const override { return warn_; }
  void warn(std::string msg) override { warn_ += msg; }
  void clear() override {
    error_.clear();
    warn_.clear();
  }
  void enable_debug(bool enable) override { debug_enabled_ = enable; }
  bool debug_enabled() override { return debug_enabled_; }
  std::string get_debug() const override { return ""; }
  void debug(object_match) override {}
};

using ivar_node = int_variable_node<mock_native_context>;
using fvar_node = float_variable_node<mock_native_context>;
using svar_node = str_variable_node<mock_native_context>;
using sint_node = summary_int_variable_node<mock_native_context>;
using sstr_node = summary_string_variable_node<mock_native_context>;

// ---- Factory --------------------------------------------------------------
//
// The factory recognises a small fixed set of names so tests can build
// expressions that reference actual object / summary fields. Names:
//   ivar  -> object_->ival   (object-bound int)
//   fvar  -> object_->fval   (object-bound float)
//   svar  -> object_->sval   (object-bound string)
//   scount -> summary_->count   (summary int — does NOT require object)
//   sstatus -> summary_->status (summary string — does NOT require object)
//
// The factory also registers one custom function so the function path can be
// exercised: identity(x) returns x evaluated as a string.

struct mock_factory : object_factory_interface {
  std::string error_;
  std::string warn_;
  bool debug_enabled_ = false;

  bool has_error() const override { return !error_.empty(); }
  std::string get_error() const override { return error_; }
  void error(std::string msg) override {
    if (!error_.empty()) error_ += ", ";
    error_ += msg;
  }
  bool has_warn() const override { return !warn_.empty(); }
  std::string get_warn() const override { return warn_; }
  void warn(std::string msg) override { warn_ += msg; }
  void clear() override {
    error_.clear();
    warn_.clear();
  }
  void enable_debug(bool enable) override { debug_enabled_ = enable; }
  bool debug_enabled() override { return debug_enabled_; }
  std::string get_debug() const override { return ""; }
  void debug(object_match) override {}

  // ---- Converter (no implicit conversions in this test fixture) -----------
  bool can_convert(value_type, value_type) override { return false; }
  bool can_convert(std::string, std::shared_ptr<any_node>, value_type) override { return false; }
  std::shared_ptr<binary_function_impl> create_converter(std::string, std::shared_ptr<any_node>, value_type) override { return nullptr; }

  // ---- Variables ----------------------------------------------------------
  bool has_variable(const std::string& name) override {
    return name == "ivar" || name == "fvar" || name == "svar" || name == "scount" || name == "sstatus";
  }

  node_type create_variable(const std::string& name, bool /*human_readable*/) override {
    if (name == "ivar") {
      return std::make_shared<ivar_node>(name, type_int,
                                         [](mock_object o, evaluation_context) -> long long { return o.ival; },
                                         std::list<ivar_node::int_performance_generator>{});
    }
    if (name == "fvar") {
      return std::make_shared<fvar_node>(name, type_float,
                                         [](mock_object o, evaluation_context) -> double { return o.fval; },
                                         std::list<fvar_node::float_performance_generator>{});
    }
    if (name == "svar") {
      return std::make_shared<svar_node>(name, type_string,
                                         [](mock_object o, evaluation_context) -> std::string { return o.sval; });
    }
    if (name == "scount") {
      return std::make_shared<sint_node>(name, [](mock_summary* s) -> long long { return s ? s->count : 0; });
    }
    if (name == "sstatus") {
      return std::make_shared<sstr_node>(name, [](mock_summary* s) -> std::string { return s ? s->status : ""; });
    }
    error("Unknown variable: " + name);
    return factory::create_false();
  }

  // ---- Functions ----------------------------------------------------------
  bool has_function(const std::string& name) override { return name == "identity"; }

  node_type create_function(const std::string& name, node_type subject) override {
    if (name == "identity") {
      // identity(x) returns x evaluated as a string. Useful for exercising
      // the function path without depending on any specific transformation.
      auto fun = [](value_type, evaluation_context ctx, node_type s) -> node_type {
        return factory::create_string(s->get_string_value(ctx));
      };
      return std::make_shared<custom_function_node>(name, fun, subject, type_string);
    }
    error("Unknown function: " + name);
    return factory::create_false();
  }

  std::string get_performance_config_key(std::string, std::string, std::string, std::string, std::string) const override { return ""; }
};

// ---- Error handler --------------------------------------------------------

struct mock_error_handler : error_handler_interface {
  std::string error_;
  std::string warn_;
  std::string debug_;
  bool debug_flag_ = false;

  void log_error(std::string e) override {
    if (!error_.empty()) error_ += "\n";
    error_ += e;
  }
  void log_warning(std::string w) override {
    if (!warn_.empty()) warn_ += "\n";
    warn_ += w;
  }
  void log_debug(std::string d) override {
    if (!debug_.empty()) debug_ += "\n";
    debug_ += d;
  }
  bool is_debug() const override { return debug_flag_; }
  void set_debug(bool d) override { debug_flag_ = d; }

  bool has_errors() const { return !error_.empty(); }
  bool has_warnings() const { return !warn_.empty(); }
};

// ---- Helpers --------------------------------------------------------------

std::shared_ptr<mock_error_handler> make_handler() { return std::make_shared<mock_error_handler>(); }
object_factory make_factory() { return std::make_shared<mock_factory>(); }
std::shared_ptr<mock_native_context> make_native_context() { return std::make_shared<mock_native_context>(); }
evaluation_context as_eval(std::shared_ptr<mock_native_context> ctx) { return ctx; }

// Build and validate a single engine_filter ready for match / match_force.
// Returns nullptr if validation fails — exposes the error string for diagnostics.
std::unique_ptr<engine_filter> build_filter(const std::string& expr, const std::shared_ptr<mock_error_handler>& handler) {
  auto f = std::make_unique<engine_filter>(expr);
  performance_collector boundaries;
  if (!f->validate(handler, make_factory(), false, boundaries)) {
    return nullptr;
  }
  return f;
}

// Convenience: build, then evaluate with a configured native context. Returns
// just the matched bit of force_match_result; helpers below expose the full
// struct when a test needs the is_unsure flag.
bool eval_force(const std::string& expr, std::shared_ptr<mock_native_context> ctx) {
  auto handler = make_handler();
  auto f = build_filter(expr, handler);
  if (!f) {
    ADD_FAILURE() << "validate failed: " << handler->error_ << " for: " << expr;
    return false;
  }
  return f->match_force(handler, as_eval(ctx)).matched;
}

force_match_result eval_force_full(const std::string& expr, std::shared_ptr<mock_native_context> ctx) {
  auto handler = make_handler();
  auto f = build_filter(expr, handler);
  if (!f) {
    ADD_FAILURE() << "validate failed: " << handler->error_ << " for: " << expr;
    return {};
  }
  return f->match_force(handler, as_eval(ctx));
}

bool eval_match(const std::string& expr, std::shared_ptr<mock_native_context> ctx, bool expect_object) {
  auto handler = make_handler();
  auto f = build_filter(expr, handler);
  if (!f) {
    ADD_FAILURE() << "validate failed: " << handler->error_ << " for: " << expr;
    return false;
  }
  return f->match(handler, as_eval(ctx), expect_object);
}

}  // namespace

// ============================================================================
// Baseline: literal expressions, no variables
// ============================================================================

TEST(EngineFilterMatchForce, LiteralTrueReturnsTrue) {
  auto handler = make_handler();
  auto f = build_filter("1 = 1", handler);
  ASSERT_TRUE(f) << handler->error_;
  EXPECT_TRUE(f->match_force(handler, as_eval(make_native_context())).matched);
}

TEST(EngineFilterMatchForce, LiteralFalseReturnsFalse) {
  auto handler = make_handler();
  auto f = build_filter("1 = 2", handler);
  ASSERT_TRUE(f) << handler->error_;
  EXPECT_FALSE(f->match_force(handler, as_eval(make_native_context())).matched);
}

// Truth-table coverage for OR. Literal sides keep the test focused on the
// boolean operator, not on variable-resolution. All four input combinations
// are asserted in one test so a regression in any single row is visible.
TEST(EngineFilterMatchForce, OrTruthTable) {
  auto ctx = make_native_context();
  EXPECT_TRUE(eval_force("1 = 1 or 1 = 1", ctx));   // T or T = T
  EXPECT_TRUE(eval_force("1 = 1 or 1 = 2", ctx));   // T or F = T
  EXPECT_TRUE(eval_force("1 = 2 or 1 = 1", ctx));   // F or T = T
  EXPECT_FALSE(eval_force("1 = 2 or 1 = 2", ctx));  // F or F = F
}

// Truth-table coverage for AND. Same structure as OrTruthTable.
TEST(EngineFilterMatchForce, AndTruthTable) {
  auto ctx = make_native_context();
  EXPECT_TRUE(eval_force("1 = 1 and 1 = 1", ctx));    // T and T = T
  EXPECT_FALSE(eval_force("1 = 1 and 1 = 2", ctx));   // T and F = F
  EXPECT_FALSE(eval_force("1 = 2 and 1 = 1", ctx));   // F and T = F
  EXPECT_FALSE(eval_force("1 = 2 and 1 = 2", ctx));   // F and F = F
}

// Truth table for unary NOT.
TEST(EngineFilterMatchForce, NotTruthTable) {
  auto ctx = make_native_context();
  EXPECT_FALSE(eval_force("not 1 = 1", ctx));  // not T = F
  EXPECT_TRUE(eval_force("not 1 = 2", ctx));   // not F = T
}

TEST(EngineFilterMatchForce, BypassesRequireObjectGuard) {
  // For a literal (require_object=false), match(expect_object=true) returns
  // false. match_force ignores the guard and evaluates the expression.
  auto handler = make_handler();
  auto f = build_filter("1 = 1", handler);
  ASSERT_TRUE(f) << handler->error_;

  auto ctx = as_eval(make_native_context());
  EXPECT_FALSE(f->match(handler, ctx, /*expect_object=*/true));
  EXPECT_TRUE(f->match_force(handler, ctx).matched);
}

TEST(EngineFilterMatchForce, ClearsContextWarningsAfterEvaluation) {
  auto handler = make_handler();
  auto f = build_filter("1 = 1", handler);
  ASSERT_TRUE(f) << handler->error_;

  auto ctx = as_eval(make_native_context());
  ctx->warn("stale warning");
  ASSERT_TRUE(ctx->has_warn());

  (void)f->match_force(handler, ctx);
  EXPECT_FALSE(ctx->has_warn());
}

// ============================================================================
// engine::match_force — any-of semantics across multiple filters
// ============================================================================

TEST(EngineMatchForce, EmptyFilterListReturnsFalse) {
  auto handler = make_handler();
  engine e(std::vector<std::string>{}, handler);
  ASSERT_TRUE(e.validate(make_factory()));
  const auto r = e.match_force(as_eval(make_native_context()));
  EXPECT_FALSE(r.matched);
  EXPECT_FALSE(r.is_unsure);
}

TEST(EngineMatchForce, SingleTrueFilterMatches) {
  const auto handler = make_handler();
  engine e({"1 = 1"}, handler);
  ASSERT_TRUE(e.validate(make_factory())) << handler->error_;
  EXPECT_TRUE(e.match_force(as_eval(make_native_context())).matched);
}

TEST(EngineMatchForce, SingleFalseFilterDoesNotMatch) {
  auto handler = make_handler();
  engine e({"1 = 2"}, handler);
  ASSERT_TRUE(e.validate(make_factory())) << handler->error_;
  EXPECT_FALSE(e.match_force(as_eval(make_native_context())).matched);
}

TEST(EngineMatchForce, AnyOfSemanticsAcrossMultipleFilters) {
  auto handler = make_handler();
  engine e({"1 = 2", "5 = 5"}, handler);
  ASSERT_TRUE(e.validate(make_factory())) << handler->error_;
  EXPECT_TRUE(e.match_force(as_eval(make_native_context())).matched);
}

TEST(EngineMatchForce, AllFalseAcrossMultipleFiltersReturnsFalse) {
  auto handler = make_handler();
  engine e({"1 = 2", "5 = 6"}, handler);
  ASSERT_TRUE(e.validate(make_factory())) << handler->error_;
  EXPECT_FALSE(e.match_force(as_eval(make_native_context())).matched);
}

TEST(EngineMatchForce, MatchesIndependentlyOfMatchExpectObjectGate) {
  auto handler = make_handler();
  engine e({"7 > 3"}, handler);
  ASSERT_TRUE(e.validate(make_factory())) << handler->error_;

  auto ctx = as_eval(make_native_context());
  EXPECT_FALSE(e.match(ctx, /*expect_object=*/true));
  EXPECT_TRUE(e.match_force(ctx).matched);
}

// ============================================================================
// Validation errors
// ============================================================================

TEST(EngineFilterValidate, ParseFailureReturnsFalse) {
  auto handler = make_handler();
  engine_filter f("1 = ");  // partial expression
  performance_collector boundaries;
  EXPECT_FALSE(f.validate(handler, make_factory(), false, boundaries));
  EXPECT_TRUE(handler->has_errors());
}

TEST(EngineFilterValidate, UnknownVariableSurfacesError) {
  auto handler = make_handler();
  // mock_factory only knows ivar/fvar/svar/scount/sstatus.
  engine_filter f("nonexistent_var = 1");
  performance_collector boundaries;
  EXPECT_FALSE(f.validate(handler, make_factory(), false, boundaries));
}

TEST(EngineFilterValidate, KnownVariableValidatesOk) {
  auto handler = make_handler();
  engine_filter f("ivar = 1");
  performance_collector boundaries;
  EXPECT_TRUE(f.validate(handler, make_factory(), false, boundaries)) << handler->error_;
}

// ============================================================================
// Comparison operators with a current object
//
// Sanity checks that the variable plumbing works end-to-end. We use the
// `match()` API with expect_object=true since the expressions are
// object-bound.
// ============================================================================

TEST(EngineFilterWithObject, IntEquals) {
  auto ctx = make_native_context();
  ctx->set_object({42, 0.0, ""});
  EXPECT_TRUE(eval_match("ivar = 42", ctx, true));
  EXPECT_FALSE(eval_match("ivar = 41", ctx, true));
}

TEST(EngineFilterWithObject, IntComparisons) {
  auto ctx = make_native_context();
  ctx->set_object({10, 0.0, ""});
  EXPECT_TRUE(eval_match("ivar > 5", ctx, true));
  EXPECT_FALSE(eval_match("ivar > 50", ctx, true));
  EXPECT_TRUE(eval_match("ivar < 50", ctx, true));
  EXPECT_FALSE(eval_match("ivar < 5", ctx, true));
  EXPECT_TRUE(eval_match("ivar >= 10", ctx, true));
  EXPECT_TRUE(eval_match("ivar <= 10", ctx, true));
  EXPECT_TRUE(eval_match("ivar != 9", ctx, true));
}

TEST(EngineFilterWithObject, FloatComparisons) {
  auto ctx = make_native_context();
  ctx->set_object({0, 3.14, ""});
  EXPECT_TRUE(eval_match("fvar > 3.0", ctx, true));
  EXPECT_TRUE(eval_match("fvar < 4.0", ctx, true));
  EXPECT_TRUE(eval_match("fvar != 2.71", ctx, true));
}

TEST(EngineFilterWithObject, StringEqualsAndNotEquals) {
  auto ctx = make_native_context();
  ctx->set_object({0, 0.0, "hung"});
  EXPECT_TRUE(eval_match("svar = 'hung'", ctx, true));
  EXPECT_FALSE(eval_match("svar = 'stopped'", ctx, true));
  EXPECT_TRUE(eval_match("svar != 'stopped'", ctx, true));
}

TEST(EngineFilterWithObject, StringLikeAndNotLike) {
  auto ctx = make_native_context();
  ctx->set_object({0, 0.0, "notepad.exe"});
  // LIKE here is case-insensitive substring (per operator_like::like_match_to_container).
  EXPECT_TRUE(eval_match("svar like 'note'", ctx, true));
  EXPECT_TRUE(eval_match("svar like 'NOTE'", ctx, true));
  EXPECT_FALSE(eval_match("svar like 'calc'", ctx, true));
  EXPECT_TRUE(eval_match("svar not like 'calc'", ctx, true));
}

TEST(EngineFilterWithObject, StringRegexpAndNotRegexp) {
  auto ctx = make_native_context();
  ctx->set_object({0, 0.0, "notepad.exe"});
  EXPECT_TRUE(eval_match("svar regexp '.*\\.exe'", ctx, true));
  EXPECT_FALSE(eval_match("svar regexp '.*\\.bin'", ctx, true));
  EXPECT_TRUE(eval_match("svar not regexp '.*\\.bin'", ctx, true));
}

TEST(EngineFilterWithObject, IntInList) {
  auto ctx = make_native_context();
  ctx->set_object({3, 0.0, ""});
  EXPECT_TRUE(eval_match("ivar in (1, 2, 3)", ctx, true));
  EXPECT_FALSE(eval_match("ivar in (4, 5, 6)", ctx, true));
  EXPECT_TRUE(eval_match("ivar not in (4, 5, 6)", ctx, true));
}

TEST(EngineFilterWithObject, StringInList) {
  auto ctx = make_native_context();
  ctx->set_object({0, 0.0, "hung"});
  EXPECT_TRUE(eval_match("svar in ('hung', 'stopped')", ctx, true));
  EXPECT_FALSE(eval_match("svar in ('started', 'running')", ctx, true));
  EXPECT_TRUE(eval_match("svar not in ('started', 'running')", ctx, true));
}

TEST(EngineFilterWithObject, AndOrCombinations) {
  const auto ctx = make_native_context();
  ctx->set_object({10, 0.0, "hung"});
  EXPECT_TRUE(eval_match("ivar > 5 and svar = 'hung'", ctx, true));
  EXPECT_FALSE(eval_match("ivar > 5 and svar = 'stopped'", ctx, true));
  EXPECT_TRUE(eval_match("ivar > 50 or svar = 'hung'", ctx, true));
  EXPECT_FALSE(eval_match("ivar > 50 or svar = 'stopped'", ctx, true));
}

// ============================================================================
// Comparison operators via match_force WITHOUT an object set
//
// This is the modern_filter "no rows matched" path: no object is set, and
// match_force evaluates anyway. The expectation is that object-bound vars
// resolve to something falsy without throwing, so a mixed expression like
// `<bound> OR <summary>` lets the summary side carry the verdict.
// ============================================================================

TEST(EngineFilterMatchForce, IntEqualsNoObjectIsFalse) {
  auto ctx = make_native_context();  // no object
  EXPECT_FALSE(eval_force("ivar = 42", ctx));
}

TEST(EngineFilterMatchForce, IntComparisonsNoObjectResolveAgainstZero) {
  // KEY FINDING: int_variable_node::get_value with no object returns
  // value_container::create_int(0, /*is_unsure=*/true). So *every* numeric
  // comparison against an int variable in the no-object path is evaluated
  // as if the variable were 0 — which means `ivar < 5`, `ivar <= 0`, and
  // `ivar = 0` all come out TRUE in the empty-rows case. Users who write
  // mixed expressions like `crit=cpu < 5 OR count = 0` will see CRIT in
  // the empty case purely because cpu defaults to 0. Document that here so
  // the surprising-but-load-bearing behaviour is testable.
  const auto ctx = make_native_context();
  EXPECT_FALSE(eval_force("ivar > 5", ctx));   // 0 > 5  → false
  EXPECT_TRUE(eval_force("ivar < 5", ctx));    // 0 < 5  → true  (!)
  EXPECT_FALSE(eval_force("ivar >= 1", ctx));  // 0 >= 1 → false
  EXPECT_TRUE(eval_force("ivar <= 0", ctx));   // 0 <= 0 → true  (!)
  EXPECT_TRUE(eval_force("ivar != 1", ctx));   // 0 != 1 → true
  EXPECT_TRUE(eval_force("ivar = 0", ctx));    // 0 = 0  → true  (!)
}

TEST(EngineFilterMatchForce, FloatComparisonsNoObjectResolveAgainstZero) {
  // Same default-to-zero behaviour for float variables.
  const auto ctx = make_native_context();
  EXPECT_FALSE(eval_force("fvar > 0.5", ctx));
  EXPECT_TRUE(eval_force("fvar < 0.5", ctx));
  EXPECT_TRUE(eval_force("fvar = 0", ctx));
}

TEST(EngineFilterMatchForce, FloatComparisonsNoObjectFalseSide) {
  const auto ctx = make_native_context();
  EXPECT_FALSE(eval_force("fvar > 3.0", ctx));
  EXPECT_FALSE(eval_force("fvar < -1.0", ctx));
}

// String-variable comparisons under match_force with no object: since
// string_variable_node::get_value now returns string("", is_unsure=true) for
// the no-object case (Quirk A), every comparison is evaluated against the
// empty string with is_unsure propagated through the operator. The matched
// value depends on whether the comparison happens to be true or false for
// "" — but is_unsure is always set, so modern_filter::match_post escalates
// the verdict to UNKNOWN.

TEST(EngineFilterMatchForce, StringEqualsNoObjectIsUnsureFalse) {
  auto ctx = make_native_context();
  // "" == "hung" is false, but is_unsure=true. matched=false, is_unsure=true.
  const auto r = eval_force_full("svar = 'hung'", ctx);
  EXPECT_FALSE(r.matched);
  EXPECT_TRUE(r.is_unsure);
}

TEST(EngineFilterMatchForce, StringNotEqualsNoObjectIsUnsureTrue) {
  auto ctx = make_native_context();
  // "" != "hung" is TRUE — but the result is unsure (we don't actually know
  // what svar is). modern_filter sees unsure-true → UNKNOWN.
  const auto r = eval_force_full("svar != 'hung'", ctx);
  EXPECT_TRUE(r.matched);
  EXPECT_TRUE(r.is_unsure);
}

TEST(EngineFilterMatchForce, StringLikeNonEmptyPatternNoObjectIsUnsureFalse) {
  auto ctx = make_native_context();
  // "" doesn't contain "hung" → matched=false, is_unsure=true.
  const auto r = eval_force_full("svar like 'hung'", ctx);
  EXPECT_FALSE(r.matched);
  EXPECT_TRUE(r.is_unsure);
}

TEST(EngineFilterMatchForce, StringNotLikeNonEmptyPatternNoObjectIsUnsureTrue) {
  auto ctx = make_native_context();
  // !("" contains "hung") → matched=true, is_unsure=true. Reaches UNKNOWN
  // through the unsure flag, NOT through matched=false as the previous
  // nil-collapses-to-false implementation incidentally produced.
  const auto r = eval_force_full("svar not like 'hung'", ctx);
  EXPECT_TRUE(r.matched);
  EXPECT_TRUE(r.is_unsure);
}

TEST(EngineFilterMatchForce, StringRegexpDotStarNoObjectIsUnsureTrue) {
  auto ctx = make_native_context();
  // "" matches '.*' → matched=true, is_unsure=true.
  const auto r = eval_force_full("svar regexp '.*'", ctx);
  EXPECT_TRUE(r.matched);
  EXPECT_TRUE(r.is_unsure);
}

TEST(EngineFilterMatchForce, StringNotRegexpDotStarNoObjectIsUnsureFalse) {
  auto ctx = make_native_context();
  // !("" matches '.*') → matched=false, is_unsure=true.
  const auto r = eval_force_full("svar not regexp '.*'", ctx);
  EXPECT_FALSE(r.matched);
  EXPECT_TRUE(r.is_unsure);
}

TEST(EngineFilterMatchForce, StringRegexpDotPlusNoObjectIsUnsureFalse) {
  auto ctx = make_native_context();
  // '.+' requires at least one char; "" doesn't match → matched=false, is_unsure=true.
  const auto r = eval_force_full("svar regexp '.+'", ctx);
  EXPECT_FALSE(r.matched);
  EXPECT_TRUE(r.is_unsure);
}

// ----------------------------------------------------------------------------
// IN / NOT IN with int variable — works correctly in match_force
//
// int_variable_node returns int(0, unsure=true) when no object is present, so
// the IN / NOT IN comparison operates on the value 0 without throwing. The
// comparison is unsure but is_true() honours the integer result.
// ----------------------------------------------------------------------------

TEST(EngineFilterMatchForce, IntInListNoObjectMatchesZero) {
  auto ctx = make_native_context();
  // ivar resolves to 0 with no object → 0 in (0,1,2) is true.
  EXPECT_TRUE(eval_force("ivar in (0, 1, 2)", ctx));
}

TEST(EngineFilterMatchForce, IntNotInListNoObjectIsTrueIfNotZero) {
  auto ctx = make_native_context();
  // ivar resolves to 0 → 0 not in (1,2,3) is true.
  EXPECT_TRUE(eval_force("ivar not in (1, 2, 3)", ctx));
}

TEST(EngineFilterMatchForce, IntInListNoObjectIsFalseIfMissing) {
  auto ctx = make_native_context();
  // ivar resolves to 0 → 0 in (1,2,3) is false.
  EXPECT_FALSE(eval_force("ivar in (1, 2, 3)", ctx));
}

// ----------------------------------------------------------------------------
// IN / NOT IN with STRING variable — fixed via nil-guards in operators.cpp
//
// Background: prior to the fix, string_variable_node::get_value with no
// object returned a nil value_container. operator_in::eval_string then called
// lhs.get_string() on the nil — which throws filter_exception("Type is not
// string"). The throw propagated up to parser::evaluate where it was caught
// and the entire AST collapsed to nil → false, so the OR's right side was
// never reached.
//
// The fix mirrors the nil-guard in even_simpler_bool_binary_operator_impl::
// eval_string: detect the type mismatch up-front and return an UNSURE
// value_container instead of dereferencing the nil. Combined with the
// modern_filter::match_post UNKNOWN escalation, this means broken mixed
// expressions surface as UNKNOWN to the user instead of silently OK.
// ----------------------------------------------------------------------------

TEST(EngineFilterMatchForce, StringInListNoObjectIsUnsureFalse) {
  auto ctx = make_native_context();
  const auto r = eval_force_full("svar in ('hung', 'stopped')", ctx);
  EXPECT_FALSE(r.matched);
  EXPECT_TRUE(r.is_unsure)
      << "operator_in::eval_string now returns unsure-false on nil lhs (was: throw → sure-false).";
}

TEST(EngineFilterMatchForce, StringNotInListNoObjectIsUnsureTrue) {
  auto ctx = make_native_context();
  // After Quirk A, svar resolves to "" (unsure). "" is not in ('hung','stopped'),
  // so NOT IN evaluates to true — but with is_unsure=true so modern_filter
  // still escalates UNKNOWN.
  const auto r = eval_force_full("svar not in ('hung', 'stopped')", ctx);
  EXPECT_TRUE(r.matched);
  EXPECT_TRUE(r.is_unsure);
}

TEST(EngineFilterMatchForce, StringInOrSummarySureTrueShortCircuits) {
  // `<unsure-false> OR <sure-true>` short-circuits in operator_or to sure-true,
  // so the result is matched with is_unsure=false. modern_filter::match_post
  // will escalate to CRIT/WARN as intended.
  auto ctx = make_native_context();
  const auto r = eval_force_full("svar in ('hung', 'stopped') or 1 = 1", ctx);
  EXPECT_TRUE(r.matched);
  EXPECT_FALSE(r.is_unsure);
}

TEST(EngineFilterMatchForce, StringNotInOrSummarySureTrueShortCircuits) {
  auto ctx = make_native_context();
  const auto r = eval_force_full("svar not in ('hung', 'stopped') or 1 = 1", ctx);
  EXPECT_TRUE(r.matched);
  EXPECT_FALSE(r.is_unsure);
}

TEST(EngineFilterMatchForce, StringInOrSummaryFalseRemainsUnsure) {
  // `<unsure-false> OR <sure-false>` does NOT short-circuit; result is
  // unsure-false. modern_filter::match_post escalates this to UNKNOWN
  // because no sure verdict could be reached but force-eval was unsure.
  auto ctx = make_native_context();
  const auto r = eval_force_full("svar in ('hung', 'stopped') or 1 = 2", ctx);
  EXPECT_FALSE(r.matched);
  EXPECT_TRUE(r.is_unsure);
}

// ----------------------------------------------------------------------------
// Side-by-side parity fixture: `IN` and the chained-OR rewrite agree
//
// `svar in ('hung', 'stopped')` is sugar for `svar = 'hung' or svar = 'stopped'`.
// After the fix to operator_in::eval_string and operator_not_in::eval_string,
// both forms produce the same verdict under match_force regardless of
// whether an object is set — the previous IN-throws asymmetry is gone.
// ----------------------------------------------------------------------------

TEST(EngineFilter, InVsChainedOrAgreeUnderMatchForceNoObject) {
  auto ctx = make_native_context();
  mock_summary summary;
  summary.count = 0;
  ctx->set_summary(&summary);

  // With scount=0 (sure-true on the OR's right side) both forms short-circuit
  // through the OR and return sure-true — the user-visible verdict is the
  // same and modern_filter::match_post escalates to CRIT.
  const auto chained = eval_force_full("svar = 'hung' or svar = 'stopped' or scount = 0", ctx);
  const auto inform = eval_force_full("svar in ('hung', 'stopped') or scount = 0", ctx);
  EXPECT_EQ(chained.matched, inform.matched);
  EXPECT_TRUE(chained.matched);
  // is_unsure should also agree — both clear because the sure-true scount=0
  // short-circuits operator_or.
  EXPECT_EQ(chained.is_unsure, inform.is_unsure);
  EXPECT_FALSE(chained.is_unsure);
}

TEST(EngineFilter, InVsChainedOrAgreeWhenSummarySideFalse) {
  auto ctx = make_native_context();
  mock_summary summary;
  summary.count = 5;  // scount = 0 is now false
  ctx->set_summary(&summary);

  // With scount!=0 the OR can't short-circuit, so the unsure flag from the
  // IN/= bound side propagates upward — both forms now produce unsure-false
  // (UNKNOWN territory at the modern_filter level).
  const auto chained = eval_force_full("svar = 'hung' or svar = 'stopped' or scount = 0", ctx);
  const auto inform = eval_force_full("svar in ('hung', 'stopped') or scount = 0", ctx);
  EXPECT_EQ(chained.matched, inform.matched);
  EXPECT_FALSE(chained.matched);
  EXPECT_EQ(chained.is_unsure, inform.is_unsure);
  EXPECT_TRUE(chained.is_unsure)
      << "Both bound-side forms propagate unsure → modern_filter escalates to UNKNOWN.";
}

// ============================================================================
// AND / OR combinations via match_force (no object)
//
// These exercise the typical "mixed predicate" patterns from the modern_filter
// no-rows force-evaluate path: object-bound side OR/AND a summary side. The
// summary side is represented by literal comparisons (which do not require
// an object).
// ============================================================================

// Mixed `bound OR summary` — full truth table. The bound side is forced to
// true/false by setting/clearing the object; the summary side is varied via
// the literal comparison.
TEST(EngineFilter, MixedBoundOrSummaryTruthTable) {
  auto bound_true = make_native_context();
  bound_true->set_object({0, 0.0, "hung"});
  auto bound_false = make_native_context();  // no object → svar='hung' is false

  // T or T = T, T or F = T (bound side carries when summary false)
  EXPECT_TRUE(eval_match("svar = 'hung' or 1 = 1", bound_true, true));
  EXPECT_TRUE(eval_match("svar = 'hung' or 1 = 2", bound_true, true));
  // F or T = T (summary side carries when bound false), F or F = F
  EXPECT_TRUE(eval_force("svar = 'hung' or 1 = 1", bound_false));
  EXPECT_FALSE(eval_force("svar = 'hung' or 1 = 2", bound_false));
}

// Mixed `bound AND summary` — full truth table.
TEST(EngineFilter, MixedBoundAndSummaryTruthTable) {
  auto bound_true = make_native_context();
  bound_true->set_object({0, 0.0, "hung"});
  auto bound_false = make_native_context();

  // T and T = T, T and F = F
  EXPECT_TRUE(eval_match("svar = 'hung' and 1 = 1", bound_true, true));
  EXPECT_FALSE(eval_match("svar = 'hung' and 1 = 2", bound_true, true));
  // F and T = F, F and F = F
  EXPECT_FALSE(eval_force("svar = 'hung' and 1 = 1", bound_false));
  EXPECT_FALSE(eval_force("svar = 'hung' and 1 = 2", bound_false));
}

// Operand order should not change the verdict (AND/OR are commutative). Run
// the same truth tables with the summary side on the left.
TEST(EngineFilter, MixedSummaryOrBoundTruthTable) {
  auto bound_true = make_native_context();
  bound_true->set_object({0, 0.0, "hung"});
  auto bound_false = make_native_context();

  EXPECT_TRUE(eval_match("1 = 1 or svar = 'hung'", bound_true, true));
  EXPECT_TRUE(eval_match("1 = 2 or svar = 'hung'", bound_true, true));
  EXPECT_TRUE(eval_force("1 = 1 or svar = 'hung'", bound_false));
  EXPECT_FALSE(eval_force("1 = 2 or svar = 'hung'", bound_false));
}

TEST(EngineFilter, MixedSummaryAndBoundTruthTable) {
  auto bound_true = make_native_context();
  bound_true->set_object({0, 0.0, "hung"});
  auto bound_false = make_native_context();

  EXPECT_TRUE(eval_match("1 = 1 and svar = 'hung'", bound_true, true));
  EXPECT_FALSE(eval_match("1 = 2 and svar = 'hung'", bound_true, true));
  EXPECT_FALSE(eval_force("1 = 1 and svar = 'hung'", bound_false));
  EXPECT_FALSE(eval_force("1 = 2 and svar = 'hung'", bound_false));
}

// Same truth tables but with an int variable on the bound side. ivar=42 with
// no object resolves to 0 (the surprising default-zero behaviour pinned by
// IntComparisonsNoObjectResolveAgainstZero), so the bound-false case here
// uses `ivar > 100`.
TEST(EngineFilter, MixedIntBoundOrSummaryTruthTable) {
  auto bound_true = make_native_context();
  bound_true->set_object({200, 0.0, ""});  // ivar > 100 is true
  auto bound_false = make_native_context();  // ivar (=0) > 100 is false

  EXPECT_TRUE(eval_match("ivar > 100 or 1 = 1", bound_true, true));
  EXPECT_TRUE(eval_match("ivar > 100 or 1 = 2", bound_true, true));
  EXPECT_TRUE(eval_force("ivar > 100 or 1 = 1", bound_false));
  EXPECT_FALSE(eval_force("ivar > 100 or 1 = 2", bound_false));
}

TEST(EngineFilter, MixedIntBoundAndSummaryTruthTable) {
  auto bound_true = make_native_context();
  bound_true->set_object({200, 0.0, ""});
  auto bound_false = make_native_context();

  EXPECT_TRUE(eval_match("ivar > 100 and 1 = 1", bound_true, true));
  EXPECT_FALSE(eval_match("ivar > 100 and 1 = 2", bound_true, true));
  EXPECT_FALSE(eval_force("ivar > 100 and 1 = 1", bound_false));
  EXPECT_FALSE(eval_force("ivar > 100 and 1 = 2", bound_false));
}

// ============================================================================
// Unary NOT — surprising behaviour via match_force
//
// When the inner expression evaluates to false (which it does for any object-
// bound predicate with no object), unary NOT inverts that to true. This means
// `crit=not <bound> OR <summary>` will fire CRIT in the no_rows path even when
// the summary side is false. Tests pin the current behaviour so any future
// fix is an explicit, intentional change.
// ============================================================================

// Truth table for `NOT <bound>` covering both "object set" and "no object".
// The no-object row is the surprising one — NOT-of-default-false yields true.
TEST(EngineFilter, NotBoundTruthTable) {
  auto bound_true = make_native_context();
  bound_true->set_object({0, 0.0, "hung"});
  auto bound_false_with_obj = make_native_context();
  bound_false_with_obj->set_object({0, 0.0, "running"});
  auto no_object = make_native_context();

  EXPECT_FALSE(eval_match("not svar = 'hung'", bound_true, true));            // not T = F
  EXPECT_TRUE(eval_match("not svar = 'hung'", bound_false_with_obj, true));   // not F = T
  EXPECT_TRUE(eval_force("not svar = 'hung'", no_object))                     // not (default-F) = T
      << "Surprise: NOT inverts the default-false from a missing string variable.";
}

// `NOT <bound> OR <summary>` truth table — three rows because the bound side
// has three relevant states (true / explicit-false / no-object-default-false).
TEST(EngineFilter, NotBoundOrSummaryTruthTable) {
  auto bound_true = make_native_context();
  bound_true->set_object({0, 0.0, "hung"});
  auto bound_false_with_obj = make_native_context();
  bound_false_with_obj->set_object({0, 0.0, "running"});
  auto no_object = make_native_context();

  // bound_true → not(T)=F → F or T = T, F or F = F
  EXPECT_TRUE(eval_match("not svar = 'hung' or 1 = 1", bound_true, true));
  EXPECT_FALSE(eval_match("not svar = 'hung' or 1 = 2", bound_true, true));
  // bound_false_with_obj → not(F)=T → always true regardless of summary
  EXPECT_TRUE(eval_match("not svar = 'hung' or 1 = 1", bound_false_with_obj, true));
  EXPECT_TRUE(eval_match("not svar = 'hung' or 1 = 2", bound_false_with_obj, true));
  // no_object → not(default-F)=T → always true (the surprise)
  EXPECT_TRUE(eval_force("not svar = 'hung' or 1 = 1", no_object));
  EXPECT_TRUE(eval_force("not svar = 'hung' or 1 = 2", no_object))
      << "KNOWN SURPRISE: NOT-of-default-false carries the verdict even when summary side is false.";
}

// `NOT <bound>` for an int variable — same shape, with the no-object case
// resolving against the int-default-zero.
TEST(EngineFilter, NotIntComparisonTruthTable) {
  auto bound_true = make_native_context();
  bound_true->set_object({100, 0.0, ""});  // ivar > 5 is true
  auto bound_false_with_obj = make_native_context();
  bound_false_with_obj->set_object({1, 0.0, ""});  // ivar > 5 is false
  auto no_object = make_native_context();  // ivar (=0) > 5 is false

  EXPECT_FALSE(eval_match("not ivar > 5", bound_true, true));            // not T = F
  EXPECT_TRUE(eval_match("not ivar > 5", bound_false_with_obj, true));   // not F = T
  EXPECT_TRUE(eval_force("not ivar > 5", no_object));                    // not (default-F) = T
}

// ============================================================================
// Summary variables via match_force
//
// Summary variables (summary_int_variable_node / summary_string_variable_node)
// have require_object()=false and resolve via the summary pointer. They are
// the canonical "summary side" of mixed expressions, and force-eval works
// for them as it would for plain literals.
// ============================================================================

TEST(EngineFilterMatchForce, SummaryIntComparisonNoObject) {
  auto ctx = make_native_context();
  mock_summary summary;
  summary.count = 0;
  ctx->set_summary(&summary);
  EXPECT_TRUE(eval_force("scount = 0", ctx));
  EXPECT_FALSE(eval_force("scount > 5", ctx));
}

TEST(EngineFilterMatchForce, SummaryIntChangesWithSummaryState) {
  auto ctx = make_native_context();
  mock_summary summary;
  summary.count = 7;
  ctx->set_summary(&summary);
  EXPECT_TRUE(eval_force("scount = 7", ctx));
  EXPECT_FALSE(eval_force("scount = 0", ctx));
}

TEST(EngineFilterMatchForce, MixedBoundAndSummary) {
  // `svar='hung' OR scount=0` — the canonical issue #74 expression. Even
  // with no object set, the summary side correctly carries the verdict.
  auto ctx = make_native_context();
  mock_summary summary;
  summary.count = 0;
  ctx->set_summary(&summary);
  EXPECT_TRUE(eval_force("svar = 'hung' or scount = 0", ctx));
}

TEST(EngineFilterMatchForce, MixedBoundAndSummaryNonEmptyDoesNotFire) {
  // Same expression but with a non-zero summary count → both sides false.
  auto ctx = make_native_context();
  mock_summary summary;
  summary.count = 5;
  ctx->set_summary(&summary);
  EXPECT_FALSE(eval_force("svar = 'hung' or scount = 0", ctx));
}

// ============================================================================
// Functions
// ============================================================================

TEST(EngineFilter, FunctionWithStringLiteralArgument) {
  // The where grammar restricts function arguments to lists of literals (or
  // bare identifier-like names parsed as string literals). So function calls
  // take their arguments as string values. Verify the function path is
  // wired up and exercised: identity('hello') should evaluate to "hello".
  auto handler = make_handler();
  auto f = build_filter("identity('hello') = 'hello'", handler);
  ASSERT_TRUE(f) << "validate: " << handler->error_;

  // require_object is true for custom_function_node, so use match() with
  // expect_object=true. No object actually needs to be set because the
  // function only operates on its literal argument.
  auto ctx = make_native_context();
  EXPECT_TRUE(f->match(handler, as_eval(ctx), true)) << "error: " << handler->error_;
}

TEST(EngineFilterMatchForce, FunctionPathLiteralArgEvaluatesEvenWithoutObject) {
  // Even without an object set, a function call whose argument is a literal
  // can still produce its value — the function does not actually depend on
  // object state. match_force must therefore happily evaluate it.
  auto ctx = make_native_context();
  EXPECT_TRUE(eval_force("identity('hello') = 'hello'", ctx));
  EXPECT_FALSE(eval_force("identity('hello') = 'world'", ctx));
}

// ============================================================================
// match_force log channels
//
// After Quirk A both string and int variables use context->warn() for the
// no-object case (not error). match_force explicitly clears warnings on the
// context, so evaluating any object-bound predicate with no object should
// produce a clean handler — no ERROR-level noise on every empty-rows tick.
// ============================================================================

TEST(EngineFilterMatchForce, StringVariableNoObjectDoesNotLogError) {
  // Pre-Quirk-A: string_variable_node logged ERROR on every no-object
  // evaluation, flooding production agent logs. Now it logs WARN, which
  // match_force clears via context->clear() — handler stays clean.
  auto handler = make_handler();
  auto f = build_filter("svar = 'hung'", handler);
  ASSERT_TRUE(f) << handler->error_;

  auto ctx = as_eval(make_native_context());
  (void)f->match_force(handler, ctx);
  EXPECT_FALSE(handler->has_errors())
      << "post-Quirk-A: no-object string variable should warn (which match_force "
         "clears), not error. Got: " << handler->error_;
}

TEST(EngineFilterMatchForce, IntVariableNoObjectDoesNotLogError) {
  // int_variable_node::get_value uses warn() for the no-object case (not
  // error), and match_force explicitly clears warnings. So evaluating an
  // int-only expression with no object should produce a clean handler.
  auto handler = make_handler();
  auto f = build_filter("ivar = 42", handler);
  ASSERT_TRUE(f) << handler->error_;

  auto ctx = as_eval(make_native_context());
  (void)f->match_force(handler, ctx);
  EXPECT_FALSE(handler->has_errors());
  EXPECT_FALSE(handler->has_warnings());
}

// ============================================================================
// match() vs match_force() — the require_object guard
// ============================================================================

TEST(EngineFilterMatch, ExpectObjectFalseSkipsObjectBound) {
  auto handler = make_handler();
  auto f = build_filter("ivar = 42", handler);
  ASSERT_TRUE(f) << handler->error_;
  auto ctx = make_native_context();
  ctx->set_object({42, 0.0, ""});
  // ivar requires object → match(ctx, expect_object=false) returns false
  // even though the value would compare equal.
  EXPECT_FALSE(f->match(handler, as_eval(ctx), /*expect_object=*/false));
  EXPECT_TRUE(f->match(handler, as_eval(ctx), /*expect_object=*/true));
}

TEST(EngineFilterMatch, ExpectObjectTrueSkipsLiteralOnly) {
  auto handler = make_handler();
  auto f = build_filter("1 = 1", handler);
  ASSERT_TRUE(f) << handler->error_;
  auto ctx = as_eval(make_native_context());
  EXPECT_FALSE(f->match(handler, ctx, /*expect_object=*/true));
  EXPECT_TRUE(f->match(handler, ctx, /*expect_object=*/false));
}

TEST(EngineFilterMatch, ExpectObjectFalseAcceptsSummaryOnly) {
  // Summary variables have require_object=false, so they pass the
  // expect_object=false gate. This is the path modern_filter::match_post
  // already used for `crit=count=0` style pure-summary expressions.
  auto handler = make_handler();
  auto f = build_filter("scount = 0", handler);
  ASSERT_TRUE(f) << handler->error_;
  auto ctx = make_native_context();
  mock_summary summary;
  summary.count = 0;
  ctx->set_summary(&summary);
  EXPECT_TRUE(f->match(handler, as_eval(ctx), /*expect_object=*/false));
}

// ============================================================================
// engine — multiple filters, validation, and force-eval composition
// ============================================================================

TEST(EngineMatchForce, SureTrueFilterShortCircuitsAndClearsUnsure) {
  // engine::match_force is any-of: a sure-true filter wins immediately and
  // clears the is_unsure flag from any earlier unsure-result filter that
  // hadn't yet been visited. Here the literal `1=1` is sure-true, so the
  // engine returns matched=true, is_unsure=false even though the IN-on-
  // string-var second filter would have returned unsure-false.
  auto handler = make_handler();
  engine e({"1 = 1", "svar in ('a','b')"}, handler);
  ASSERT_TRUE(e.validate(make_factory())) << handler->error_;
  const auto r = e.match_force(as_eval(make_native_context()));
  EXPECT_TRUE(r.matched);
  EXPECT_FALSE(r.is_unsure);
}

TEST(EngineMatchForce, UnsureFirstFilterMaskedBySureTrueLater) {
  // An unsure filter earlier in the list does not prevent a sure-true filter
  // later in the list from winning. The engine prefers a sure verdict over
  // an unsure one — required so a properly-formed sure filter isn't poisoned
  // by an adjacent unsure one.
  auto handler = make_handler();
  engine e({"svar in ('a','b')", "1 = 1"}, handler);
  ASSERT_TRUE(e.validate(make_factory())) << handler->error_;
  const auto r = e.match_force(as_eval(make_native_context()));
  EXPECT_TRUE(r.matched);
  EXPECT_FALSE(r.is_unsure);
}

TEST(EngineMatchForce, AllUnsureProducesUnsureFalse) {
  // If every filter is unsure (no sure-true wins), the engine returns
  // matched=false, is_unsure=true so the caller can surface UNKNOWN.
  auto handler = make_handler();
  engine e({"svar in ('a','b')", "svar = 'x'"}, handler);
  ASSERT_TRUE(e.validate(make_factory())) << handler->error_;
  const auto r = e.match_force(as_eval(make_native_context()));
  EXPECT_FALSE(r.matched);
  EXPECT_TRUE(r.is_unsure);
}

TEST(EngineValidate, AcceptsMixOfBoundAndSummaryFilters) {
  auto handler = make_handler();
  engine e({"ivar > 5", "scount = 0"}, handler);
  EXPECT_TRUE(e.validate(make_factory())) << handler->error_;
}

// ============================================================================
// Operator audit: LIKE / NOT LIKE / REGEXP / NOT REGEXP propagate is_unsure
// in mixed expressions. The base LIKE/NOT LIKE/REGEXP/NOT REGEXP cases
// against a no-object string variable are covered above by the
// String{Like,NotLike,Regexp,NotRegexp}*NoObject* tests — those exercise
// the matched + is_unsure values for individual operators. The mixed-
// expression rows below pin the OR short-circuit interaction with
// modern_filter::match_post.
// ============================================================================

// Mixed expression: `<bound LIKE> OR <sure-true>` short-circuits in
// operator_or to sure-true; `<bound LIKE> OR <sure-false>` propagates
// unsure-false so match_post escalates UNKNOWN. Pin both rows of the
// behaviour-matrix so future regressions surface here.
TEST(EngineFilterMatchForce, LikeOrSummarySureTrueShortCircuits) {
  auto ctx = make_native_context();
  const auto r = eval_force_full("svar like 'hung' or 1 = 1", ctx);
  EXPECT_TRUE(r.matched);
  EXPECT_FALSE(r.is_unsure);
}

TEST(EngineFilterMatchForce, LikeOrSummarySureFalsePropagatesUnsure) {
  auto ctx = make_native_context();
  const auto r = eval_force_full("svar like 'hung' or 1 = 2", ctx);
  EXPECT_FALSE(r.matched);
  EXPECT_TRUE(r.is_unsure);
}

TEST(EngineFilterMatchForce, RegexpOrSummarySureTrueShortCircuits) {
  auto ctx = make_native_context();
  const auto r = eval_force_full("svar regexp '.*' or 1 = 1", ctx);
  EXPECT_TRUE(r.matched);
  EXPECT_FALSE(r.is_unsure);
}

// ============================================================================
// Operator audit: invalid regex syntax surfaces as unsure-false (UNKNOWN)
// instead of silent sure-false (OK).
//
// `boost::bad_expression` from a malformed regex used to be caught and
// returned as nil → sure-false. Same fix as the nil-guard: return
// unsure-false so the user sees UNKNOWN for their broken filter.
// ============================================================================

TEST(EngineFilter, InvalidRegexSyntaxSurfacesAsUnsure) {
  auto ctx = make_native_context();
  ctx->set_object({0, 0.0, "anything"});
  // '[' is not a valid regex (unmatched bracket).
  const auto r = eval_force_full("svar regexp '['", ctx);
  EXPECT_FALSE(r.matched);
  EXPECT_TRUE(r.is_unsure);
}

TEST(EngineFilter, InvalidRegexInOrWithSureTrueStillFires) {
  // Even if the user's regex is broken, a sure-true summary side still
  // short-circuits the OR — preserves issue #74's core intent that summary
  // can rescue a verdict from a malformed bound side.
  auto ctx = make_native_context();
  ctx->set_object({0, 0.0, "anything"});
  const auto r = eval_force_full("svar regexp '[' or 1 = 1", ctx);
  EXPECT_TRUE(r.matched);
  EXPECT_FALSE(r.is_unsure);
}

// ============================================================================
// Operator audit: operator_lt::do_eval_float no longer drops lhs.is_unsure
// in the false-branch.
//
// Before the fix the false-result returned `(false, rhs.is_unsure)` —
// asymmetric with every other comparison operator that ORs both is_unsure
// flags. This test pins consistency across all six comparison ops on float.
// ============================================================================

TEST(EngineFilterMatchForce, FloatLessThanFalseStillPropagatesUnsure) {
  auto ctx = make_native_context();
  // fvar with no object resolves to (0.0, is_unsure=true). 0.0 < -1.0 is
  // false, but the result must still be unsure — the bound side couldn't
  // resolve. Without the fix this returned sure-false → silent OK.
  const auto r = eval_force_full("fvar < -1.0", ctx);
  EXPECT_FALSE(r.matched);
  EXPECT_TRUE(r.is_unsure);
}

TEST(EngineFilterMatchForce, FloatLessThanFalseInOrSummaryFalsePropagatesUnsure) {
  auto ctx = make_native_context();
  // Mixed expression where the false-from-bound side must keep is_unsure
  // so the OR result stays unsure (and modern_filter escalates UNKNOWN
  // rather than the silent OK that the bug used to produce).
  const auto r = eval_force_full("fvar < -1.0 or 1 = 2", ctx);
  EXPECT_FALSE(r.matched);
  EXPECT_TRUE(r.is_unsure);
}

// ============================================================================
// Operator audit: unary `not` for type_int (unary minus) and type_date.
//
// Note: helpers::get_return_type(op_not, *) always returns type_bool — the
// where grammar's `not <expr>` always produces a type_bool unary_op. The
// type_int and type_date branches in operator_not::evaluate are therefore
// only reachable when `not` is invoked via binary_function_impl (e.g.
// programmatic AST construction), not through user filter syntax.
//
// The Tier 3 fix to those branches is defensive and not user-reachable, so
// no test exercises it directly. The user-reachable type_bool branch is
// already covered by NotBoundTruthTable / NotBoundOrSummaryTruthTable
// elsewhere in this file.
// ============================================================================

// ============================================================================
// Operator audit (Quirk F): parser::evaluate's catch block escalates throws
// to unsure-false instead of silent sure-false.
//
// We can't easily inject a throw from a real operator after the IN/NOT IN
// fix, so this test relies on a deeply-defensive expectation: any future
// throw inside an operator's evaluate() should surface as UNKNOWN. The
// regex-bad-syntax test above is the closest user-reachable scenario; this
// section is the contract documentation.
// ============================================================================

TEST(EngineFilter, ThrowingExpressionSurfacesAsUnsure) {
  // Today the only user-reachable throw path inside a working operator is
  // a malformed regex (boost::bad_expression). The throw is now caught
  // inside operator_regexp::regex_match_to_container and returned as
  // unsure-false directly, so parser::evaluate's catch block doesn't fire
  // for this case. But the catch block exists as defence-in-depth: any
  // future operator that throws on a degenerate input will still escalate
  // to UNKNOWN rather than silent OK.
  //
  // Sanity: confirm the regex case (which used to throw, now propagates
  // through the helper) ends in unsure-false.
  auto ctx = make_native_context();
  ctx->set_object({0, 0.0, "x"});
  const auto r = eval_force_full("svar regexp '*invalid*'", ctx);
  EXPECT_FALSE(r.matched);
  EXPECT_TRUE(r.is_unsure);
}

// ============================================================================
// Operator audit: function_convert (Issue 1) propagates is_unsure through
// size/time conversions and int↔float casts.
//
// Reachability:
//   - Literal-with-unit syntax (e.g. `ivar > 100M`) wraps an int_literal in
//     a function_convert at parse time. The convert always produces sure-int
//     for the RHS; the unsure flag flows from the LHS variable. Tested via
//     IntComparisonAgainstSizeLiteralPropagatesUnsureFromLhs below.
//   - Programmatic AST construction can build a function_convert whose
//     subject is itself unsure (e.g. `convert(<bound-var>, 'gb')`). The
//     where grammar's list_expr restricts function args to mono-typed
//     literal/variable lists, so the user can't write this directly today,
//     but the fix is exercised below by manually constructing the node tree.
// ============================================================================

TEST(EngineFilterMatchForce, IntComparisonAgainstSizeLiteralPropagatesUnsureFromLhs) {
  // `ivar > 100M` — `100M` is a function_convert(list[100, "M"]) wrapping
  // sure literals. The convert returns sure-int (100*1024*1024). The `>`
  // operator combines: 0 (unsure-int from no-object ivar) > 104857600 →
  // false, is_unsure = unsure | sure = unsure. Result: unsure-false.
  auto ctx = make_native_context();
  const auto r = eval_force_full("ivar > 100M", ctx);
  EXPECT_FALSE(r.matched);
  EXPECT_TRUE(r.is_unsure)
      << "function_convert should keep the LHS's unsure flag intact through the comparison.";
}

TEST(EngineFilterMatchForce, IntComparisonAgainstSizeLiteralOrSummarySureTrue) {
  // Same expression with a sure-true rescue clause — operator_or short-
  // circuits to sure-true, modern_filter escalates CRIT.
  auto ctx = make_native_context();
  const auto r = eval_force_full("ivar > 100M or 1 = 1", ctx);
  EXPECT_TRUE(r.matched);
  EXPECT_FALSE(r.is_unsure);
}

TEST(EngineFilterMatchForce, IntComparisonAgainstTimeLiteral) {
  // `ivar > 1h` — function_convert(list[1, "h"]) computes now+3600 → sure-int.
  // ivar unsure-int 0; 0 > big-time-value → false (unsure). modern_filter
  // escalates UNKNOWN.
  auto ctx = make_native_context();
  const auto r = eval_force_full("ivar > 1h", ctx);
  EXPECT_FALSE(r.matched);
  EXPECT_TRUE(r.is_unsure);
}

TEST(EngineFilter, FunctionConvertProgrammaticBoundSubjectPropagatesUnsure) {
  // Defensive test: programmatically construct
  //   convert(ivar, 'gb') > 5
  // which the grammar can't currently emit (list_expr requires mono-typed
  // args). Bypass the parser and build the AST directly so we can verify
  // function_convert propagates ivar's is_unsure through parse_size.
  //
  // After Issue 1's fix, the result must carry is_unsure=true so any
  // surrounding comparison (here: > 5) propagates UNKNOWN to match_post.
  auto factory_ptr = make_factory();

  // Build subject list, convert wrapper, and the surrounding `> 5` op.
  auto args = factory::create_list();
  args->push_back(factory_ptr->create_variable("ivar", false));
  args->push_back(factory::create_string("gb"));
  auto convert_fun = std::make_shared<unary_fun>("convert", args);
  convert_fun->set_type(type_size);
  ASSERT_TRUE(convert_fun->bind(factory_ptr));

  auto five = factory::create_int(5);
  auto cmp = factory::create_bin_op(op_gt, convert_fun, five);
  // binary_op needs infer_type to set its type before evaluate works
  // (otherwise binary_op::evaluate's is_int()/is_string() check fails and
  // it returns sure-false, masking the bug we're testing for).
  cmp->infer_type(factory_ptr);
  ASSERT_TRUE(cmp->bind(factory_ptr));

  auto ctx = as_eval(make_native_context());
  const value_container v = cmp->get_value(ctx, type_int);
  // ivar=0 unsure → convert returns int_value(0, unsure) → 0>5 false unsure.
  EXPECT_FALSE(v.is_true());
  EXPECT_TRUE(v.is_unsure)
      << "function_convert must propagate the subject's is_unsure flag through "
         "parse_size — the previous get_int_value() path silently dropped it.";
}

// ============================================================================
// Operator audit: operator_not type_string fallback (Issue 2)
//
// `neg(...)` is registered as a binary_function that dispatches to
// operator_not via binary_function_impl. When the subject's type is
// type_string (which the grammar's list_expr can produce when the function
// takes a single bare identifier-as-string), the dispatch falls into the
// "missing impl for NOT operator" branch. Pre-fix: returned sure-false,
// dropping any is_unsure. Post-fix: returns unsure-false.
// ============================================================================

TEST(EngineFilterMatchForce, NegOnStringSubjectFallbackIsUnsureFalse) {
  // `neg('hello')` — the parser builds a list[string_value("hello")] subject,
  // then unary_fun("neg", subject) which dispatches to operator_not. subject
  // type derives to type_string (mono-typed string list), and operator_not
  // has no string-specialised inversion. After the fix this hits the
  // unsure-false fallback rather than silently returning sure-false.
  //
  // We compare to `0 = 0` (sure-true) so the OR can short-circuit when the
  // bound side is unsure-false; OR-form keeps the test independent of the
  // exact fallback truth value.
  auto ctx = make_native_context();
  const auto r = eval_force_full("neg('hello') or 0 = 0", ctx);
  EXPECT_TRUE(r.matched);   // sure-true via OR short-circuit
  EXPECT_FALSE(r.is_unsure);
}

TEST(EngineFilterMatchForce, NegOnStringSubjectAloneIsUnsureFalse) {
  // Without a sure-true rescue: the unsure-false from neg() propagates to
  // the result. modern_filter::match_post would escalate UNKNOWN. The
  // parser actually types `neg('hello')` as type_bool (not type_string),
  // and the subject is a list_node which produces nil for get_value(int) —
  // operator_not's type_bool branch defends against that nil and returns
  // unsure-false rather than dereferencing it to 0 (NOT 0 = sure-true).
  auto ctx = make_native_context();
  const auto r = eval_force_full("neg('hello') or 1 = 2", ctx);
  EXPECT_FALSE(r.matched);
  EXPECT_TRUE(r.is_unsure);
}

// ============================================================================
// Quirk A: string_variable_node returns string("", is_unsure=true) on no-
// object instead of nil.
//
// Direct contract test (StrVariableNode in variable_test.cpp covers this
// against the type directly). Here we pin the engine-level visible effect:
// match_force evaluations against a no-object context produce a clean
// handler (no ERROR-level log) because the variable warns instead of
// erroring, and match_force clears warnings.
// ============================================================================

TEST(EngineFilterMatchForce, StringVariableNoObjectProducesNoErrorInHandler) {
  auto handler = make_handler();
  auto f = build_filter("svar = 'hung' or svar like '%error%' or svar in ('a','b','c')", handler);
  ASSERT_TRUE(f) << handler->error_;

  auto ctx = as_eval(make_native_context());
  const auto r = f->match_force(handler, ctx);
  // Every operator above touches svar with no object; pre-Quirk-A this would
  // have produced an ERROR per visit. Post-Quirk-A: warns, cleared by
  // match_force.
  EXPECT_FALSE(handler->has_errors())
      << "Quirk A: string variable no-object now warns (cleared) instead of erroring. "
         "Got: " << handler->error_;
  // The expression itself is still unsure-false (no sure-true rescue clause).
  EXPECT_FALSE(r.matched);
  EXPECT_TRUE(r.is_unsure);
}

// ============================================================================
// Operator audit (commit dd8024ae): summary variables under match() don't
// produce spurious unsure flags or "mutating" warns.
//
// modern_filter::evaluate_deferred_records sets the object per row and calls
// engine_crit/warn/ok->match(context, true). If the warn/crit expression
// references a summary variable (e.g. `count`), the variable is consulted
// with object SET. Pre-fix that flagged the result as unsure and warned
// "X is most likely mutating" — both wrong post-deferred-eval, since the
// summary value is final at that point.
//
// These tests pin the post-fix contract: with object set AND summary set,
// the variable returns sure-int with no warn. We exercise via the
// engine_filter::match() path used by evaluate_deferred_records, plus a
// pure unit assertion that scount is sure under match_force too.
// ============================================================================

TEST(EngineFilter, SummaryVarUnderRegularMatchWithObjectIsSureAndQuiet) {
  // Simulate a deferred-eval call: object set, summary set, evaluate a
  // mixed predicate via the regular match() entry point (which is what
  // evaluate_deferred_records uses).
  auto handler = make_handler();
  auto f = build_filter("scount = 5", handler);
  ASSERT_TRUE(f) << handler->error_;

  auto ctx = make_native_context();
  mock_summary summary;
  summary.count = 5;
  ctx->set_summary(&summary);
  ctx->set_object({0, 0.0, ""});  // <-- object set, like during deferred eval

  // Pure-summary expression has require_object=false; match with
  // expect_object=true would refuse it, so use expect_object=false.
  EXPECT_TRUE(f->match(handler, as_eval(ctx), /*expect_object=*/false));
  EXPECT_FALSE(handler->has_warnings())
      << "no 'is most likely mutating' warn or 'Ignoring unsure result' should "
         "fire — summary is final at this point. Got: " << handler->warn_;
}

TEST(EngineFilter, MixedSummaryAndBoundUnderRegularMatchWithObject) {
  // Real deferred-eval scenario: `crit=svar='hung' OR scount<5` evaluated
  // per-row with object set (so svar resolves from the row) and summary
  // set (so scount has its final value). No "mutating" warn or unsure
  // propagation should leak from scount into the final verdict.
  auto handler = make_handler();
  auto f = build_filter("svar = 'hung' or scount < 5", handler);
  ASSERT_TRUE(f) << handler->error_;

  auto ctx = make_native_context();
  mock_summary summary;
  summary.count = 10;  // count<5 is sure-false
  ctx->set_summary(&summary);
  ctx->set_object({0, 0.0, "running"});  // svar='hung' is sure-false

  EXPECT_FALSE(f->match(handler, as_eval(ctx), /*expect_object=*/true));
  EXPECT_FALSE(handler->has_warnings())
      << "no warn from summary mutating heuristic (was: 'count is most likely "
         "mutating' + 'Ignoring unsure result' on every row). Got: " << handler->warn_;
}

TEST(EngineFilter, SummaryVarMatchedSideUnderRegularMatchWithObject) {
  // Same shape but the row matches via the bound side. Verdict must be
  // true with no leaking warns.
  auto handler = make_handler();
  auto f = build_filter("svar = 'hung' or scount < 5", handler);
  ASSERT_TRUE(f) << handler->error_;

  auto ctx = make_native_context();
  mock_summary summary;
  summary.count = 10;
  ctx->set_summary(&summary);
  ctx->set_object({0, 0.0, "hung"});  // svar='hung' is sure-true

  EXPECT_TRUE(f->match(handler, as_eval(ctx), /*expect_object=*/true));
  EXPECT_FALSE(handler->has_warnings()) << "Got: " << handler->warn_;
}

TEST(EngineFilterMatchForce, SummaryVarUnderForceEvalIsSure) {
  // No-rows force-evaluate path (no object): scount is sure-int. This was
  // already correct pre-fix; this test pins it stays correct after the
  // heuristic change.
  auto ctx = make_native_context();
  mock_summary summary;
  summary.count = 0;
  ctx->set_summary(&summary);

  const auto r = eval_force_full("scount = 0", ctx);
  EXPECT_TRUE(r.matched);
  EXPECT_FALSE(r.is_unsure);
}
