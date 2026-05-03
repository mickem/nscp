/*
 * Copyright (C) 2004-2016 Michael Medin
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

#include <boost/algorithm/string.hpp>
#include <boost/optional.hpp>
#include <boost/regex.hpp>
#include <cmath>  // Required on linux
#include <iostream>
#include <parsers/helpers.hpp>
#include <parsers/operators.hpp>
#include <parsers/where/helpers.hpp>
#include <parsers/where/value_node.hpp>
#include <str/xtos.hpp>

#ifdef _WIN32
#pragma warning(disable : 4100)
#endif

namespace parsers {
namespace where {
namespace operator_impl {

struct simple_bool_binary_operator_impl : binary_operator_impl {
  explicit simple_bool_binary_operator_impl(std::string desc) : binary_operator_impl(std::move(desc)) {}
  node_type evaluate(const evaluation_context context, const node_type left, const node_type right) const override {
    const value_type ltype = left->get_type();
    const value_type rtype = right->get_type();

    if (helpers::type_is_int(ltype) && helpers::type_is_int(rtype)) return factory::create_num(eval_int(ltype, context, left, right));

    if (helpers::type_is_float(ltype) && helpers::type_is_float(rtype)) return factory::create_num(eval_float(ltype, context, left, right));

    if (ltype != rtype && rtype != type_tbd) {
      context->error("Invalid types (not same) for binary operator");
      return factory::create_false();
    }
    const value_type type = left->get_type();
    if (helpers::type_is_int(type)) return factory::create_num(eval_int(type, context, left, right));
    if (helpers::type_is_float(type)) return factory::create_num(eval_float(type, context, left, right));
    if (type == type_string) return factory::create_num(eval_string(type, context, left, right));
    context->error("missing impl for simple bool binary operator");
    return factory::create_false();
  }

  virtual value_container eval_int(value_type, evaluation_context context, node_type left, node_type right) const = 0;
  virtual value_container eval_float(value_type, evaluation_context context, node_type left, node_type right) const = 0;
  virtual value_container eval_string(value_type, evaluation_context context, node_type left, node_type right) const = 0;
};

struct even_simpler_bool_binary_operator_impl : simple_bool_binary_operator_impl {
  explicit even_simpler_bool_binary_operator_impl(std::string desc) : simple_bool_binary_operator_impl(std::move(desc)) {}
  // Type-mismatch in the lhs/rhs typically means "object-bound variable was
  // evaluated with no current object" (modern_filter no-rows force-evaluate
  // path). Returning sure-false (via nil → factory::create_num(nil)) used to
  // hide this from the caller — modern_filter::match_post couldn't tell a
  // genuine sure-false from "I couldn't fully evaluate this." Returning
  // unsure-false here lets match_post escalate to UNKNOWN when no other
  // sure verdict was reached, matching the contract of operator_in /
  // operator_not_in.
  value_container eval_int(const value_type type, const evaluation_context context, const node_type left, const node_type right) const override {
    const value_container lhs = left->get_value(context, type_int);
    const value_container rhs = right->get_value(context, type_int);
    if (!lhs.is(type_int) || !rhs.is(type_int)) {
      context->error("invalid type");
      return value_container::create_int(false, /*is_unsure=*/true);
    }
    return do_eval_int(type, context, lhs, rhs);
  };
  value_container eval_float(const value_type type, const evaluation_context context, const node_type left, const node_type right) const override {
    const value_container lhs = left->get_value(context, type_float);
    const value_container rhs = right->get_value(context, type_float);
    if (!lhs.is(type_float) || !rhs.is(type_float)) {
      context->error("invalid type");
      return value_container::create_int(false, /*is_unsure=*/true);
    }
    return do_eval_float(type, context, lhs, rhs);
  };
  value_container eval_string(const value_type type, const evaluation_context context, const node_type left, const node_type right) const override {
    const value_container lhs = left->get_value(context, type_string);
    const value_container rhs = right->get_value(context, type_string);
    if (!lhs.is(type_string) || !rhs.is(type_string)) {
      context->error("invalid type");
      return value_container::create_int(false, /*is_unsure=*/true);
    }
    return do_eval_string(type, context, lhs, rhs);
  };

  virtual value_container do_eval_float(value_type type, evaluation_context context, value_container left, value_container right) const = 0;
  virtual value_container do_eval_int(value_type type, evaluation_context context, value_container left, value_container right) const = 0;
  virtual value_container do_eval_string(value_type type, evaluation_context context, value_container left, value_container right) const = 0;
};

struct eval_helper {
  const node_type left;
  const node_type right;
  evaluation_context context;
  const value_type ltype;
  const value_type rtype;
  boost::optional<value_container> lhs;
  boost::optional<value_container> rhs;

  eval_helper(const evaluation_context &context, const node_type &left, const node_type &right)
      : left(left), right(right), context(context), ltype(left->get_type()), rtype(right->get_type()) {}
  value_container get_lhs() {
    if (lhs) return *lhs;
    lhs.reset(left->get_value(context, type_int));
    return *lhs;
  }
  value_container get_rhs() {
    if (rhs) return *rhs;
    rhs.reset(right->get_value(context, type_int));
    return *rhs;
  }
};
struct simple_int_binary_operator_impl : binary_operator_impl {
  explicit simple_int_binary_operator_impl(std::string desc) : binary_operator_impl(std::move(desc)) {}

  node_type evaluate(const evaluation_context context_, const node_type left_, const node_type right_) const override {
    eval_helper helper(context_, left_, right_);

    if (helpers::type_is_int(helper.ltype) && helpers::type_is_int(helper.rtype)) {
      return factory::create_num(eval_int(helper));
    }
    if ((helper.ltype != helper.rtype) && (helper.rtype != type_tbd)) {
      helper.context->error("Incompatible types in binary operator: " + desc_);
      return factory::create_false();
    }
    helper.context->error("Invalid types in binary operator " + desc_);
    return factory::create_false();
  }
  virtual value_container eval_int(eval_helper &helper) const = 0;
};

struct operator_eq : even_simpler_bool_binary_operator_impl {
  explicit operator_eq(std::string desc) : even_simpler_bool_binary_operator_impl(std::move(desc)) {}
  value_container do_eval_int(value_type, evaluation_context context, const value_container lhs, const value_container rhs) const override {
    return value_container::create_int(lhs.get_int() == rhs.get_int(), lhs.is_unsure | rhs.is_unsure);
  }
  value_container do_eval_float(value_type, evaluation_context context, const value_container lhs, const value_container rhs) const override {
    return value_container::create_int(lhs.get_float() == rhs.get_float(), lhs.is_unsure | rhs.is_unsure);
  }
  value_container do_eval_string(value_type, evaluation_context context, const value_container lhs, const value_container rhs) const override {
    return value_container::create_int(lhs.get_string() == rhs.get_string(), lhs.is_unsure | rhs.is_unsure);
  };
};
struct operator_ne : even_simpler_bool_binary_operator_impl {
  explicit operator_ne(std::string desc) : even_simpler_bool_binary_operator_impl(std::move(desc)) {}
  value_container do_eval_int(value_type, evaluation_context context, const value_container lhs, const value_container rhs) const override {
    return value_container::create_int(lhs.get_int() != rhs.get_int(), lhs.is_unsure | rhs.is_unsure);
  }
  value_container do_eval_float(value_type, evaluation_context context, const value_container lhs, const value_container rhs) const override {
    return value_container::create_int(lhs.get_float() != rhs.get_float(), lhs.is_unsure | rhs.is_unsure);
  }
  value_container do_eval_string(value_type, evaluation_context context, const value_container lhs, const value_container rhs) const override {
    return value_container::create_int(lhs.get_string() != rhs.get_string(), lhs.is_unsure | rhs.is_unsure);
  };
};
struct operator_gt : even_simpler_bool_binary_operator_impl {
  explicit operator_gt(std::string desc) : even_simpler_bool_binary_operator_impl(std::move(desc)) {}
  value_container do_eval_int(value_type, evaluation_context context, const value_container lhs, const value_container rhs) const override {
    return value_container::create_int(lhs.get_int() > rhs.get_int(), lhs.is_unsure | rhs.is_unsure);
  }
  value_container do_eval_float(value_type, evaluation_context context, const value_container lhs, const value_container rhs) const override {
    return value_container::create_int(lhs.get_float() > rhs.get_float(), lhs.is_unsure | rhs.is_unsure);
  }
  value_container do_eval_string(value_type, evaluation_context context, const value_container lhs, const value_container rhs) const override {
    return value_container::create_int(lhs.get_string() > rhs.get_string(), lhs.is_unsure | rhs.is_unsure);
  };
};
struct operator_lt : even_simpler_bool_binary_operator_impl {
  explicit operator_lt(std::string desc) : even_simpler_bool_binary_operator_impl(std::move(desc)) {}
  value_container do_eval_int(value_type, evaluation_context context, const value_container lhs, const value_container rhs) const override {
    return value_container::create_int(lhs.get_int() < rhs.get_int(), lhs.is_unsure | rhs.is_unsure);
  }
  value_container do_eval_float(value_type, evaluation_context context, const value_container lhs, const value_container rhs) const override {
    if (lhs.get_float() < rhs.get_float()) return value_container::create_int(true, lhs.is_unsure | rhs.is_unsure);
    return value_container::create_int(false, rhs.is_unsure);
  }
  value_container do_eval_string(value_type, evaluation_context context, const value_container lhs, const value_container rhs) const override {
    return value_container::create_int(lhs.get_string() < rhs.get_string(), lhs.is_unsure | rhs.is_unsure);
  };
};
struct operator_le : even_simpler_bool_binary_operator_impl {
  explicit operator_le(std::string desc) : even_simpler_bool_binary_operator_impl(std::move(desc)) {}
  value_container do_eval_int(value_type, evaluation_context context, const value_container lhs, const value_container rhs) const override {
    return value_container::create_int(lhs.get_int() <= rhs.get_int(), lhs.is_unsure | rhs.is_unsure);
  }
  value_container do_eval_float(value_type, evaluation_context context, const value_container lhs, const value_container rhs) const override {
    return value_container::create_int(lhs.get_float() <= rhs.get_float(), lhs.is_unsure | rhs.is_unsure);
  }
  value_container do_eval_string(value_type, evaluation_context context, const value_container lhs, const value_container rhs) const override {
    return value_container::create_int(lhs.get_string() <= rhs.get_string(), lhs.is_unsure | rhs.is_unsure);
  };
};
struct operator_ge : even_simpler_bool_binary_operator_impl {
  explicit operator_ge(std::string desc) : even_simpler_bool_binary_operator_impl(std::move(desc)) {}
  value_container do_eval_int(value_type, evaluation_context context, const value_container lhs, const value_container rhs) const override {
    return value_container::create_int(lhs.get_int() >= rhs.get_int(), lhs.is_unsure | rhs.is_unsure);
  }
  value_container do_eval_float(value_type, evaluation_context context, const value_container lhs, const value_container rhs) const override {
    return value_container::create_int(lhs.get_float() >= rhs.get_float(), lhs.is_unsure | rhs.is_unsure);
  }
  value_container do_eval_string(value_type, evaluation_context context, const value_container lhs, const value_container rhs) const override {
    return value_container::create_int(lhs.get_string() >= rhs.get_string(), lhs.is_unsure | rhs.is_unsure);
  };
};

struct operator_and : simple_int_binary_operator_impl {
  explicit operator_and(std::string desc) : simple_int_binary_operator_impl(std::move(desc)) {}

  value_container eval_int(eval_helper &helper) const override {
    const long long lhsi = helper.get_lhs().get_int();
    if (!lhsi && !helper.get_lhs().is_unsure) return value_container::create_bool(false, false);
    const long long rhsi = helper.get_rhs().get_int();
    if (!rhsi && !helper.get_rhs().is_unsure) return value_container::create_bool(false, false);
    return value_container::create_int(lhsi && rhsi, helper.get_lhs().is_unsure | helper.get_rhs().is_unsure);
  }
};
struct operator_or : simple_int_binary_operator_impl {
  explicit operator_or(std::string desc) : simple_int_binary_operator_impl(std::move(desc)) {}

  value_container eval_int(eval_helper &helper) const override {
    const long long lhsi = helper.get_lhs().get_int();
    if (lhsi && !helper.get_lhs().is_unsure) return value_container::create_bool(true, false);
    const long long rhsi = helper.get_rhs().get_int();
    if (rhsi && !helper.get_rhs().is_unsure) return value_container::create_bool(true, false);
    return value_container::create_int(lhsi || rhsi, helper.get_lhs().is_unsure | helper.get_rhs().is_unsure);
  }
};

struct pattern_binary_operator_impl : binary_operator_impl {
  explicit pattern_binary_operator_impl(std::string desc) : binary_operator_impl(std::move(desc)) {}
  node_type evaluate(const evaluation_context context, const node_type left, const node_type right) const override {
    const value_type ltype = left->get_type();
    if (helpers::type_is_int(ltype)) return factory::create_num(eval_int(ltype, context, left, right));
    if (helpers::type_is_float(ltype)) return factory::create_num(eval_float(ltype, context, left, right));
    if (ltype == type_string) return factory::create_num(eval_string(ltype, context, left, right));
    context->error("missing impl for pattern binary operator: " + desc_);
    return factory::create_false();
  }
  virtual value_container eval_int(value_type, evaluation_context context, node_type left, node_type right) const = 0;
  virtual value_container eval_float(value_type, evaluation_context context, node_type left, node_type right) const = 0;
  virtual value_container eval_string(value_type, evaluation_context context, node_type left, node_type right) const = 0;
};

struct operator_like : pattern_binary_operator_impl {
  explicit operator_like(std::string desc) : pattern_binary_operator_impl(std::move(desc)) {}
  value_container eval_int(value_type, const evaluation_context context, const node_type left, const node_type right) const override {
    // Match the integer's textual representation, so e.g. 'cpu like "80"'
    // matches the value 80 / 180 / 800.
    const value_container lhs = left->get_value(context, type_int);
    const value_container rhs = right->get_value(context, type_string);
    return like_match_to_container(context, str::xtos(lhs.get_int()), rhs, false, lhs.is_unsure || rhs.is_unsure);
  };
  value_container eval_float(value_type, const evaluation_context context, const node_type left, const node_type right) const override {
    const value_container lhs = left->get_value(context, type_float);
    const value_container rhs = right->get_value(context, type_string);
    return like_match_to_container(context, str::xtos(lhs.get_float()), rhs, false, lhs.is_unsure || rhs.is_unsure);
  };
  value_container eval_string(value_type, const evaluation_context context, const node_type left, const node_type right) const override {
    const value_container lhs = left->get_value(context, type_string);
    const value_container rhs = right->get_value(context, type_string);
    if (!lhs.is(type_string) || !rhs.is(type_string)) {
      context->error("invalid type");
      return value_container::create_nil();
    }
    return like_match_to_container(context, lhs.get_string(), rhs, false, lhs.is_unsure || rhs.is_unsure);
  }

  // Helper shared with operator_not_like; static so it can be reused
  // without friending or duplicating the substring-match logic.
  //
  // Semantics match the original string `like` operator: case-insensitive
  // "the smaller string is a substring of the larger one". Empty inputs are
  // handled the same way the previous code did (both empty => match,
  // exactly one empty => no match).
  static value_container like_match_to_container(const evaluation_context context, const std::string &subject, const value_container &pattern,
                                                 const bool negate, const bool is_unsure) {
    if (!pattern.is(type_string)) {
      context->error("invalid type");
      return value_container::create_nil();
    }
    const std::string s1 = boost::algorithm::to_lower_copy(subject);
    const std::string s2 = boost::algorithm::to_lower_copy(pattern.get_string());

    bool matched;
    if (s1.empty() && s2.empty()) {
      matched = true;
    } else if (s1.empty() || s2.empty()) {
      matched = false;
    } else if (s1.size() > s2.size()) {
      matched = s1.find(s2) != std::string::npos;
    } else {
      matched = s2.find(s1) != std::string::npos;
    }
    return value_container::create_int(negate ? !matched : matched, is_unsure);
  }
};
struct operator_regexp : pattern_binary_operator_impl {
  explicit operator_regexp(std::string desc) : pattern_binary_operator_impl(std::move(desc)) {}
  value_container eval_int(value_type, const evaluation_context context, const node_type left, const node_type right) const override {
    // Match the integer's textual representation, so e.g. "80" matches "8.*".
    const value_container lhs = left->get_value(context, type_int);
    const value_container rhs = right->get_value(context, type_string);
    return regex_match_to_container(context, str::xtos(lhs.get_int()), rhs, false, lhs.is_unsure || rhs.is_unsure);
  };
  value_container eval_float(value_type, const evaluation_context context, const node_type left, const node_type right) const override {
    const value_container lhs = left->get_value(context, type_float);
    const value_container rhs = right->get_value(context, type_string);
    return regex_match_to_container(context, str::xtos(lhs.get_float()), rhs, false, lhs.is_unsure || rhs.is_unsure);
  };
  value_container eval_string(value_type, const evaluation_context context, const node_type left, const node_type right) const override {
    const value_container lhs = left->get_value(context, type_string);
    const value_container rhs = right->get_value(context, type_string);
    if (!lhs.is(type_string) || !rhs.is(type_string)) {
      context->error("invalid type");
      return value_container::create_nil();
    }
    return regex_match_to_container(context, lhs.get_string(), rhs, false, lhs.is_unsure || rhs.is_unsure);
  }

  // Helpers shared with operator_not_regexp; static so they can be reused
  // without friending or duplicating the boost::regex try/catch logic.
  static value_container regex_match_to_container(const evaluation_context context, const std::string &subject, const value_container &pattern,
                                                  const bool negate, const bool is_unsure) {
    if (!pattern.is(type_string)) {
      context->error("invalid type");
      return value_container::create_nil();
    }
    const std::string regexp = pattern.get_string();
    try {
      const boost::regex re(regexp);
      const bool matched = boost::regex_match(subject, re);
      return value_container::create_int(negate ? !matched : matched, is_unsure);
    } catch (const boost::bad_expression &e) {
      context->error("Invalid syntax in regular expression:" + regexp + " error: " + e.what());
      return value_container::create_nil();
    } catch (...) {
      context->error("Invalid syntax in regular expression:" + regexp);
      return value_container::create_nil();
    }
  }
};
struct operator_not_regexp : pattern_binary_operator_impl {
  explicit operator_not_regexp(std::string desc) : pattern_binary_operator_impl(std::move(desc)) {}
  value_container eval_int(value_type, const evaluation_context context, const node_type left, const node_type right) const override {
    const value_container lhs = left->get_value(context, type_int);
    const value_container rhs = right->get_value(context, type_string);
    return operator_regexp::regex_match_to_container(context, str::xtos(lhs.get_int()), rhs, true, lhs.is_unsure || rhs.is_unsure);
  };
  value_container eval_float(value_type, const evaluation_context context, const node_type left, const node_type right) const override {
    const value_container lhs = left->get_value(context, type_float);
    const value_container rhs = right->get_value(context, type_string);
    return operator_regexp::regex_match_to_container(context, str::xtos(lhs.get_float()), rhs, true, lhs.is_unsure || rhs.is_unsure);
  };
  value_container eval_string(value_type, const evaluation_context context, const node_type left, const node_type right) const override {
    const value_container lhs = left->get_value(context, type_string);
    const value_container rhs = right->get_value(context, type_string);
    if (!lhs.is(type_string) || !rhs.is(type_string)) {
      context->error("invalid type");
      return value_container::create_nil();
    }
    return operator_regexp::regex_match_to_container(context, lhs.get_string(), rhs, true, lhs.is_unsure || rhs.is_unsure);
  }
};
struct operator_not_like : pattern_binary_operator_impl {
  explicit operator_not_like(std::string desc) : pattern_binary_operator_impl(std::move(desc)) {}
  value_container eval_int(value_type, const evaluation_context context, const node_type left, const node_type right) const override {
    const value_container lhs = left->get_value(context, type_int);
    const value_container rhs = right->get_value(context, type_string);
    return operator_like::like_match_to_container(context, str::xtos(lhs.get_int()), rhs, true, lhs.is_unsure || rhs.is_unsure);
  };
  value_container eval_float(value_type, const evaluation_context context, const node_type left, const node_type right) const override {
    const value_container lhs = left->get_value(context, type_float);
    const value_container rhs = right->get_value(context, type_string);
    return operator_like::like_match_to_container(context, str::xtos(lhs.get_float()), rhs, true, lhs.is_unsure || rhs.is_unsure);
  };
  value_container eval_string(value_type, const evaluation_context context, const node_type left, const node_type right) const override {
    const value_container lhs = left->get_value(context, type_string);
    const value_container rhs = right->get_value(context, type_string);
    if (!lhs.is(type_string) || !rhs.is(type_string)) {
      context->error("invalid type");
      return value_container::create_nil();
    }
    return operator_like::like_match_to_container(context, lhs.get_string(), rhs, true, lhs.is_unsure || rhs.is_unsure);
  }
};
// Nil-safety guard shared by IN / NOT IN. The lhs of a `<var> in (...)` /
// `<var> not in (...)` expression can be nil when the variable is an object-
// bound string evaluated with no current object (the modern_filter no-rows
// force-evaluate path). The eval_string path used to dereference such a nil
// via lhs.get_string() / lhs.get_int(), which throws filter_exception. The
// throw propagated up through binary_op::evaluate and was caught in
// parser::evaluate, collapsing the entire AST to nil → false. That made
// `svar in ('hung','stopped') or count=0` silently return false for the
// no-rows case where the equivalent chained-OR rewrite returned true.
//
// Mirror even_simpler_bool_binary_operator_impl::eval_string: detect the
// type mismatch up-front and return an unsure result so the caller (and
// modern_filter::match_post) can distinguish "object-bound subterm couldn't
// resolve" from a sure verdict and surface UNKNOWN where appropriate.
namespace {
inline bool nil_lhs_unsure(const value_container& lhs, value_type wanted, const evaluation_context& context) {
  if (lhs.is(wanted)) return false;
  context->error("invalid type");
  return true;
}
}  // namespace

struct operator_not_in : simple_bool_binary_operator_impl {
  explicit operator_not_in(std::string desc) : simple_bool_binary_operator_impl(std::move(desc)) {}
  value_container eval_int(value_type type, const evaluation_context context, const node_type left, const node_type right) const override {
    const value_container lhs = left->get_value(context, type_int);
    if (nil_lhs_unsure(lhs, type_int, context)) return value_container::create_int(false, /*is_unsure=*/true);
    const long long val = lhs.get_int();
    for (const node_type itm : right->get_list_value(context)) {
      if (itm->get_int_value(context) == val) return value_container::create_int(false, lhs.is_unsure);
    }
    return value_container::create_int(true, lhs.is_unsure);
  }
  value_container eval_float(value_type, const evaluation_context context, const node_type left, const node_type right) const override {
    const value_container lhs = left->get_value(context, type_float);
    if (nil_lhs_unsure(lhs, type_float, context)) return value_container::create_int(false, /*is_unsure=*/true);
    const double val = lhs.get_float();
    for (const node_type itm : right->get_list_value(context)) {
      if (itm->get_float_value(context) == val) return value_container::create_int(false, lhs.is_unsure);
    }
    return value_container::create_int(true, lhs.is_unsure);
  }
  value_container eval_string(value_type, const evaluation_context context, const node_type left, const node_type right) const override {
    const value_container lhs = left->get_value(context, type_string);
    if (nil_lhs_unsure(lhs, type_string, context)) return value_container::create_int(false, /*is_unsure=*/true);
    const std::string val = lhs.get_string();
    for (const node_type itm : right->get_list_value(context)) {
      if (itm->get_string_value(context) == val) return value_container::create_int(false, lhs.is_unsure);
    }
    return value_container::create_int(true, lhs.is_unsure);
  };
};
struct operator_in : simple_bool_binary_operator_impl {
  explicit operator_in(std::string desc) : simple_bool_binary_operator_impl(std::move(desc)) {}
  value_container eval_int(value_type type, const evaluation_context context, const node_type left, const node_type right) const override {
    const value_container lhs = left->get_value(context, type_int);
    if (nil_lhs_unsure(lhs, type_int, context)) return value_container::create_int(false, /*is_unsure=*/true);
    const long long val = lhs.get_int();
    for (const node_type& itm : right->get_list_value(context)) {
      const long long cmp = itm->get_int_value(context);
      if (cmp == val) return value_container::create_int(true, lhs.is_unsure);
    }
    return value_container::create_int(false, lhs.is_unsure);
  }
  value_container eval_float(value_type, const evaluation_context context, const node_type left, const node_type right) const override {
    const value_container lhs = left->get_value(context, type_float);
    if (nil_lhs_unsure(lhs, type_float, context)) return value_container::create_int(false, /*is_unsure=*/true);
    const double val = lhs.get_float();
    for (const node_type& itm : right->get_list_value(context)) {
      if (itm->get_float_value(context) == val) return value_container::create_int(true, lhs.is_unsure);
    }
    return value_container::create_int(false, lhs.is_unsure);
  }
  value_container eval_string(value_type, const evaluation_context context, const node_type left, const node_type right) const override {
    const value_container lhs = left->get_value(context, type_string);
    if (nil_lhs_unsure(lhs, type_string, context)) return value_container::create_int(false, /*is_unsure=*/true);
    const std::string val = lhs.get_string();
    for (const node_type& itm : right->get_list_value(context)) {
      if (itm->get_string_value(context) == val) return value_container::create_int(true, lhs.is_unsure);
    }
    return value_container::create_int(false, lhs.is_unsure);
  };
};
struct operator_false : binary_operator_impl, unary_operator_impl, binary_function_impl {
  explicit operator_false(std::string desc) : binary_operator_impl(std::move(desc)) {}
  node_type evaluate(const evaluation_context context, const node_type, const node_type) const override {
    context->error("missing impl for FALSE");
    return factory::create_false();
  }
  node_type evaluate(const evaluation_context context, const node_type) const override {
    context->error("missing impl for FALSE");
    return factory::create_false();
  }
  node_type evaluate(value_type, const evaluation_context context, const node_type) const override {
    context->error("missing impl for FALSE");
    return factory::create_false();
  }
};

struct function_convert : binary_function_impl {
  boost::optional<node_type> value;
  boost::optional<node_type> unit;
  function_convert(const evaluation_context &context, const node_type &subject) {
    std::list<node_type> args = subject->get_list_value(context);
    std::list<node_type>::const_iterator item = args.begin();
    if (args.size() > 0) {
      value = *item;
    }
    if (args.size() > 1) {
      std::advance(item, 1);
      unit = *item;
    }
  }

  node_type evaluate(const value_type type, const evaluation_context context, const node_type subject) const override {
    if (!value) {
      context->error("no arguments for convert(): " + subject->to_string());
      return factory::create_false();
    }
    node_type v = *value;
    if (unit) {
      const node_type u = *unit;
      if (type == type_date) {
        return factory::create_int(parse_time(v->get_int_value(context), u->get_string_value(context)));
      }
      if (type == type_size) {
        return factory::create_int(parse_size(v->get_int_value(context), u->get_string_value(context)));
      }
      context->error("could not convert to " + helpers::type_to_string(type) + " from " + v->to_string() + ", " + u->to_string());
      return factory::create_false();
    }
    if (helpers::type_is_int(type)) {
      if (v->is_float()) return factory::create_int(llround(v->get_float_value(context)));
    }
    if (helpers::type_is_float(type)) {
      if (v->is_int()) return factory::create_float(static_cast<double>(v->get_int_value(context)));
    }
    return v;
  }

  static long long parse_time(const long long new_value, const std::string &new_unit) {
    const long long now = constants::get_now();
    if (new_unit.empty()) return now + new_value;
    if ((new_unit == "s") || (new_unit == "S")) return now + (new_value);
    if ((new_unit == "m") || (new_unit == "M")) return now + (new_value * 60);
    if ((new_unit == "h") || (new_unit == "H")) return now + (new_value * 60 * 60);
    if ((new_unit == "d") || (new_unit == "D")) return now + (new_value * 24 * 60 * 60);
    if ((new_unit == "w") || (new_unit == "W")) return now + (new_value * 7 * 24 * 60 * 60);
    return now + new_value;
  }

  static long long parse_size(const long long new_value, const std::string &new_unit) {
    if (new_unit.empty()) return new_value;
    if ((new_unit == "b") || (new_unit == "B")) return new_value;
    if ((new_unit == "k") || (new_unit == "K")) return new_value * 1024;
    if ((new_unit == "m") || (new_unit == "M")) return new_value * 1024 * 1024;
    if ((new_unit == "g") || (new_unit == "G")) return new_value * 1024 * 1024 * 1024;
    if ((new_unit == "t") || (new_unit == "T")) return new_value * 1024 * 1024 * 1024 * 1024;
    return new_value;
  }
};

struct simple_bool_unary_operator_impl : unary_operator_impl {
  node_type evaluate(const evaluation_context context, const node_type subject) const override {
    const value_type type = subject->get_type();
    if (helpers::type_is_int(type)) return eval_int(type, context, subject) ? factory::create_true() : factory::create_false();
    if (type == type_string) return eval_string(type, context, subject) ? factory::create_true() : factory::create_false();
    context->error("missing impl for bool unary operator");
    return factory::create_false();
  }
  virtual bool eval_int(value_type type, evaluation_context context, node_type subject) const = 0;
  virtual bool eval_string(value_type type, evaluation_context context, node_type subject) const = 0;
};

struct operator_not : unary_operator_impl, binary_function_impl {
  explicit operator_not(const node_type &) {}
  operator_not() {}
  node_type evaluate(const evaluation_context context, const node_type subject) const override { return evaluate(subject->get_type(), context, subject); }
  node_type evaluate(const value_type type, const evaluation_context context, const node_type subject) const override {
    if (type == type_bool) {
      // Propagate value_container::is_unsure through the inversion. Without
      // this, `not <object-bound predicate>` evaluated with no current
      // object discards the unsure flag and returns a sure-true (since the
      // bound side defaults to false) — which causes match_force callers to
      // escalate spurious CRIT/WARN verdicts in the no-rows path. See
      // engine_filter::match_force / modern_filter::match_post.
      const value_container v = subject->get_value(context, type_int);
      return std::make_shared<int_value>(v.get_int(0) ? 0 : 1, v.is_unsure);
    }
    if (type == type_int) return factory::create_int(-subject->get_int_value(context));
    if (type == type_date) {
      const long long now = constants::get_now();
      const long long val = now - (subject->get_int_value(context) - now);
      return factory::create_int(val);
    }
    context->error("missing impl for NOT operator");
    return factory::create_false();
  }
};
}  // namespace operator_impl

op_factory::bin_op_type op_factory::get_binary_operator(const operators op, const node_type &left, const node_type &right) {
  std::string desc = left->to_string() + " " + helpers::operator_to_string(op) + " " + right->to_string();
  // op_in, op_nin
  if (op == op_eq) return std::make_shared<operator_impl::operator_eq>(desc);
  if (op == op_gt) return std::make_shared<operator_impl::operator_gt>(desc);
  if (op == op_lt) return std::make_shared<operator_impl::operator_lt>(desc);
  if (op == op_le) return std::make_shared<operator_impl::operator_le>(desc);
  if (op == op_ge) return std::make_shared<operator_impl::operator_ge>(desc);
  if (op == op_ne) return std::make_shared<operator_impl::operator_ne>(desc);
  if (op == op_like) return std::make_shared<operator_impl::operator_like>(desc);
  if (op == op_not_like) return std::make_shared<operator_impl::operator_not_like>(desc);
  if (op == op_regexp) return std::make_shared<operator_impl::operator_regexp>(desc);
  if (op == op_not_regexp) return std::make_shared<operator_impl::operator_not_regexp>(desc);

  if (op == op_and) return std::make_shared<operator_impl::operator_and>(desc);
  if (op == op_or) return std::make_shared<operator_impl::operator_or>(desc);
  if (op == op_in) return std::make_shared<operator_impl::operator_in>(desc);
  if (op == op_nin) return std::make_shared<operator_impl::operator_not_in>(desc);

  if (op == op_binand) return std::make_shared<operator_impl::operator_and>(desc);
  if (op == op_binor) return std::make_shared<operator_impl::operator_or>(desc);

  return std::make_shared<operator_impl::operator_false>(desc);
}

bool op_factory::is_binary_function(const std::string &name) {
  if (name == "convert" || name == "auto_convert") return true;
  if (name == "neg") return true;
  return false;
}
op_factory::bin_fun_type op_factory::get_binary_function(evaluation_context context, const std::string &name, const node_type &subject) {
  if (name == "convert" || name == "auto_convert") return std::make_shared<operator_impl::function_convert>(context, subject);
  if (name == "neg") return std::make_shared<operator_impl::operator_not>(subject);
  std::cout << "======== UNDEFINED FUNCTION: " << name << std::endl;
  return std::make_shared<operator_impl::operator_false>("TODO");
}
op_factory::un_op_type op_factory::get_unary_operator(const operators op) {
  // op_inv, op_not
  if (op == op_not) return std::make_shared<operator_impl::operator_not>();
  std::cout << "======== UNHANDLED OPERATOR\n";
  return std::make_shared<operator_impl::operator_false>("TODO");
}
}  // namespace where
}  // namespace parsers