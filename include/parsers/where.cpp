// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <parsers/helpers.hpp>
#include <parsers/where.hpp>
#include <parsers/where/grammar/grammar.hpp>
#include <parsers/where/node.hpp>
#include <str/xtos.hpp>

namespace parsers {
namespace where {
namespace {
// Upper bounds on a single where-expression, enforced before the recursive
// descent parser and the recursive AST evaluator ever see the string.
//
// Both stages recurse with a depth that grows with the input: the Boost.Spirit
// grammar re-enters the `expression` rule for every nested `(...)` / `fn(...)`,
// and AST evaluation recurses one frame per operator down a left-leaning
// and/or chain (binary_op::evaluate -> get_value -> evaluate). An unbounded
// expression can therefore exhaust the stack and crash the whole nsclient
// process — a denial of service reachable wherever a filter/warning/critical
// string or a `%(...)` detail-syntax placeholder is attacker-influenced
// (authenticated REST arguments, or NRPE with "allow arguments = true"). A
// real where-filter is a short threshold string; these limits sit an order of
// magnitude above any legitimate one.
constexpr std::size_t max_expression_length = 1024;
constexpr int max_expression_depth = 64;

// Deepest `(` nesting in expr, skipping single-quoted string literals so a `(`
// inside a literal (`x = '(('`) does not count as structure — the where-grammar
// has no escapes inside `'...'`. Returns as soon as the limit is exceeded; the
// exact depth past that point does not matter.
int max_paren_depth(const std::string &expr) {
  int depth = 0;
  int max_depth = 0;
  for (std::size_t i = 0; i < expr.size(); ++i) {
    const char c = expr[i];
    if (c == '\'') {
      ++i;
      while (i < expr.size() && expr[i] != '\'') ++i;
      continue;  // i is on the closing ' (or end); the loop's ++i steps past it
    }
    if (c == '(') {
      if (++depth > max_depth) {
        max_depth = depth;
        if (max_depth > max_expression_depth) return max_depth;
      }
    } else if (c == ')') {
      if (depth > 0) --depth;
    }
  }
  return max_depth;
}
}  // namespace

bool parser::parse(object_factory factory, std::string expr) {
  constants::reset();

  // Reject pathologically large or deeply nested expressions up front so
  // neither the recursive parser nor the recursive evaluator can be driven to
  // a stack-exhaustion crash. rest carries the reason so validate() logs it.
  if (expr.size() > max_expression_length) {
    rest = "expression exceeds the maximum length of " + str::xtos(max_expression_length) + " characters";
    return false;
  }
  if (max_paren_depth(expr) > max_expression_depth) {
    rest = "expression nesting exceeds the maximum depth of " + str::xtos(max_expression_depth);
    return false;
  }

  where_grammar calc(factory);

  where_grammar::iterator_type iter = expr.begin();
  where_grammar::iterator_type end = expr.end();
  if (phrase_parse(iter, end, calc, charset::space, resulting_tree)) {
    rest = std::string(iter, end);
    return rest.empty();
  }
  rest = std::string(iter, end);
  return false;
}

bool parser::derive_types(object_converter converter) {
  try {
    resulting_tree->infer_type(converter);
    return true;
  } catch (...) {
    converter->error("Unhandled exception resolving types: " + result_as_tree());
    return false;
  }
}

bool parser::static_eval(evaluation_context context) {
  try {
    resulting_tree->static_evaluate(context);
    return true;
  } catch (const std::exception &e) {
    context->error(std::string("Unhandled exception static eval: ") + e.what());
    return false;
  } catch (...) {
    context->error("Unhandled exception static eval: " + result_as_tree());
    return false;
  }
}
bool parser::collect_perfkeys(evaluation_context context, performance_collector &boundries) {
  try {
    resulting_tree->find_performance_data(context, boundries);
    return true;
  } catch (...) {
    context->error("Unhandled exception collecting performance data eval: " + result_as_tree());
    return false;
  }
}

bool parser::bind(object_converter context) {
  try {
    resulting_tree->bind(context);
    return true;
  } catch (const std::exception &e) {
    context->error(std::string("Bind exception: ") + e.what());
    return false;
  } catch (...) {
    context->error("Bind exception: " + result_as_tree());
    return false;
  }
}

bool parser::require_object(evaluation_context context) const { return resulting_tree->require_object(context); }

value_container parser::evaluate(evaluation_context context) {
  try {
    node_type result = resulting_tree->evaluate(context);
    return result->get_value(context, type_int);
  } catch (const std::exception &e) {
    // Mark the result as unsure so callers (engine_filter::match_force →
    // modern_filter::match_post) escalate the verdict to UNKNOWN rather
    // than letting a thrown subterm silently collapse the entire AST to a
    // sure-false OK. Returning create_nil() used to drop the is_unsure
    // signal at factory::create_num(nil) → int_value(0).
    context->error(std::string("Evaluate exception: ") + e.what());
    return value_container::create_int(false, /*is_unsure=*/true);
  } catch (...) {
    context->error("Evaluate exception: " + result_as_tree());
    return value_container::create_int(false, /*is_unsure=*/true);
  }
}

std::string parser::result_as_tree() const { return resulting_tree->to_string(); }
std::string parser::result_as_tree(evaluation_context context) const { return resulting_tree->to_string(context); }
}  // namespace where
}  // namespace parsers