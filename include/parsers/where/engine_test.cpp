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
#include <parsers/where/engine.hpp>
#include <parsers/where/node.hpp>
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

TEST(EngineFilterMatchForce, StringEqualsNoObjectIsFalse) {
  // string_variable_node::get_value with no object returns nil and logs an
  // error. eval_string sees !lhs.is(type_string), returns nil, factory::
  // create_num(nil) yields int_value(0). is_true → false.
  auto ctx = make_native_context();
  EXPECT_FALSE(eval_force("svar = 'hung'", ctx));
}

TEST(EngineFilterMatchForce, StringNotEqualsNoObjectIsFalse) {
  // Same nil-collapses-to-int(0) path applies — even though semantically
  // "missing != 'hung'" might be expected to be true, the engine normalises
  // it to false.
  auto ctx = make_native_context();
  EXPECT_FALSE(eval_force("svar != 'hung'", ctx));
}

TEST(EngineFilterMatchForce, StringLikeAndNotLikeNoObjectAreFalse) {
  auto ctx = make_native_context();
  EXPECT_FALSE(eval_force("svar like 'hung'", ctx));
  EXPECT_FALSE(eval_force("svar not like 'hung'", ctx));
}

TEST(EngineFilterMatchForce, StringRegexpAndNotRegexpNoObjectAreFalse) {
  auto ctx = make_native_context();
  EXPECT_FALSE(eval_force("svar regexp '.*'", ctx));
  EXPECT_FALSE(eval_force("svar not regexp '.*'", ctx));
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

TEST(EngineFilterMatchForce, StringNotInListNoObjectIsUnsureFalse) {
  auto ctx = make_native_context();
  const auto r = eval_force_full("svar not in ('hung', 'stopped')", ctx);
  EXPECT_FALSE(r.matched);
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
// Errors are still surfaced from match_force
//
// match_force suppresses warnings (per its comment) but NOT errors —
// missing-object errors from string variables, for example, propagate to the
// error_handler. Tests pin this behaviour because it is the source of the
// "noisy logs on every empty-result tick" symptom.
// ============================================================================

TEST(EngineFilterMatchForce, StringVariableNoObjectLogsError) {
  auto handler = make_handler();
  auto f = build_filter("svar = 'hung'", handler);
  ASSERT_TRUE(f) << handler->error_;

  auto ctx = as_eval(make_native_context());
  (void)f->match_force(handler, ctx);
  EXPECT_TRUE(handler->has_errors())
      << "string_variable_node::get_value calls context->error() when no "
         "object is set; that error must surface to the caller's handler.";
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
