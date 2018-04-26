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

#include <iostream>

#include <boost/regex.hpp>
#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>

#include <parsers/operators.hpp>
#include <parsers/helpers.hpp>
#include <parsers/where/helpers.hpp>

#ifdef _WIN32
#pragma warning( disable : 4100)
#endif

namespace parsers {
	namespace where {
		namespace operator_impl {
			struct simple_bool_binary_operator_impl : public binary_operator_impl {
				node_type evaluate(evaluation_context errors, const node_type left, const node_type right) const {
					value_type ltype = left->get_type();
					value_type rtype = right->get_type();

					if (helpers::type_is_int(ltype) && helpers::type_is_int(rtype))
						return factory::create_num(eval_int(ltype, errors, left, right));

					if (helpers::type_is_float(ltype) && helpers::type_is_float(rtype))
						return factory::create_num(eval_float(ltype, errors, left, right));

					if ((ltype != rtype) && (rtype != type_tbd)) {
						errors->error("Invalid types (not same) for binary operator");
						return factory::create_false();
					}
					value_type type = left->get_type();
					if (helpers::type_is_int(type))
						return factory::create_num(eval_int(type, errors, left, right));
					if (helpers::type_is_float(type))
						return factory::create_num(eval_float(type, errors, left, right));
					if (type == type_string)
						return factory::create_num(eval_string(type, errors, left, right));
					errors->error("missing impl for simple bool binary operator");
					return factory::create_false();
				}

				virtual value_container eval_int(value_type, evaluation_context errors, const node_type left, const node_type right) const = 0;
				virtual value_container eval_float(value_type, evaluation_context errors, const node_type left, const node_type right) const = 0;
				virtual value_container eval_string(value_type, evaluation_context errors, const node_type left, const node_type right) const = 0;
			};

			struct even_simpler_bool_binary_operator_impl : public simple_bool_binary_operator_impl {
				value_container eval_int(value_type type, evaluation_context errors, const node_type left, const node_type right) const {
					value_container lhs = left->get_value(errors, type_int);
					value_container rhs = right->get_value(errors, type_int);
					if (!lhs.is(type_int) || !rhs.is(type_int)) {
						errors->error("invalid type");
						return value_container::create_nil();
					}
					return do_eval_int(type, errors, lhs, rhs);
				};
				value_container eval_float(value_type type, evaluation_context errors, const node_type left, const node_type right) const {
					value_container lhs = left->get_value(errors, type_float);
					value_container rhs = right->get_value(errors, type_float);
					if (!lhs.is(type_float) || !rhs.is(type_float)) {
						errors->error("invalid type");
						return value_container::create_nil();
					}
					return do_eval_float(type, errors, lhs, rhs);
				};
				value_container eval_string(value_type type, evaluation_context errors, const node_type left, const node_type right) const {
					value_container lhs = left->get_value(errors, type_string);
					value_container rhs = right->get_value(errors, type_string);
					if (!lhs.is(type_string) || !rhs.is(type_string)) {
						errors->error("invalid type");
						return value_container::create_nil();
					}
					return do_eval_string(type, errors, lhs, rhs);
				};

				virtual value_container do_eval_float(value_type type, evaluation_context errors, const value_container left, const value_container right) const = 0;
				virtual value_container do_eval_int(value_type type, evaluation_context errors, const value_container left, const value_container right) const = 0;
				virtual value_container do_eval_string(value_type type, evaluation_context errors, const value_container left, const value_container right) const = 0;
			};

			struct eval_helper {
				const node_type left;
				const node_type right;
				evaluation_context errors;
				const value_type ltype;
				const value_type rtype;
				boost::optional<value_container> lhs;
				boost::optional<value_container> rhs;
				boost::optional<value_container> result_;

				eval_helper(evaluation_context errors, const node_type left, const node_type right)
					: left(left)
					, right(right)
					, errors(errors) 
					, ltype(left->get_type())
					, rtype(right->get_type())
				{}
				value_container get_lhs() {
					if (lhs)
						return *lhs;
					lhs.reset(left->get_value(errors, type_int));
					return *lhs;
				}
				value_container get_rhs() {
					if (rhs)
						return *rhs;
					rhs.reset(right->get_value(errors, type_int));
					return *rhs;
				}
			};
			struct simple_int_binary_operator_impl : public binary_operator_impl {

				node_type evaluate(evaluation_context errors_, const node_type left_, const node_type right_) const {
					eval_helper helper(errors_, left_, right_);

					if (helpers::type_is_int(helper.ltype) && helpers::type_is_int(helper.rtype)) {
						return factory::create_num(eval_int(helper));
					}
					if ((helper.ltype != helper.rtype) && (helper.rtype != type_tbd)) {
						helper.errors->error("Incompatible types in binary operator");
						return factory::create_false();
					}
					helper.errors->error("Invalid types in binary operator");
					return factory::create_false();
				}
				virtual value_container eval_int(eval_helper &helper) const = 0;
			};

			struct operator_eq : public even_simpler_bool_binary_operator_impl {
				value_container do_eval_int(value_type, evaluation_context errors, const value_container lhs, const value_container rhs) const {
					return value_container::create_int(lhs.get_int() == rhs.get_int(), lhs.is_unsure | rhs.is_unsure);
				}
				value_container do_eval_float(value_type, evaluation_context errors, const value_container lhs, const value_container rhs) const {
					return value_container::create_int(lhs.get_float() == rhs.get_float(), lhs.is_unsure | rhs.is_unsure);
				}
				value_container do_eval_string(value_type, evaluation_context errors, const value_container lhs, const value_container rhs) const {
					return value_container::create_int(lhs.get_string() == rhs.get_string(), lhs.is_unsure | rhs.is_unsure);
				};
			};
			struct operator_ne : public even_simpler_bool_binary_operator_impl {
				value_container do_eval_int(value_type, evaluation_context errors, const value_container lhs, const value_container rhs) const {
					return value_container::create_int(lhs.get_int() != rhs.get_int(), lhs.is_unsure | rhs.is_unsure);
				}
				value_container do_eval_float(value_type, evaluation_context errors, const value_container lhs, const value_container rhs) const {
					return value_container::create_int(lhs.get_float() != rhs.get_float(), lhs.is_unsure | rhs.is_unsure);
				}
				value_container do_eval_string(value_type, evaluation_context errors, const value_container lhs, const value_container rhs) const {
					return value_container::create_int(lhs.get_string() != rhs.get_string(), lhs.is_unsure | rhs.is_unsure);
				};
			};
			struct operator_gt : public even_simpler_bool_binary_operator_impl {
				value_container do_eval_int(value_type, evaluation_context errors, const value_container lhs, const value_container rhs) const {
					return value_container::create_int(lhs.get_int() > rhs.get_int(), lhs.is_unsure | rhs.is_unsure);
				}
				value_container do_eval_float(value_type, evaluation_context errors, const value_container lhs, const value_container rhs) const {
					return value_container::create_int(lhs.get_float() > rhs.get_float(), lhs.is_unsure | rhs.is_unsure);
				}
				value_container do_eval_string(value_type, evaluation_context errors, const value_container lhs, const value_container rhs) const {
					return value_container::create_int(lhs.get_string() > rhs.get_string(), lhs.is_unsure | rhs.is_unsure);
				};
			};
			struct operator_lt : public even_simpler_bool_binary_operator_impl {
				value_container do_eval_int(value_type, evaluation_context errors, const value_container lhs, const value_container rhs) const {
					return value_container::create_int(lhs.get_int() < rhs.get_int(), lhs.is_unsure | rhs.is_unsure);
				}
				value_container do_eval_float(value_type, evaluation_context errors, const value_container lhs, const value_container rhs) const {
					if (lhs.get_float() < rhs.get_float())
						return value_container::create_int(true, lhs.is_unsure | rhs.is_unsure);
					return value_container::create_int(false, rhs.is_unsure);
				}
				value_container do_eval_string(value_type, evaluation_context errors, const value_container lhs, const value_container rhs) const {
					return value_container::create_int(lhs.get_string() < rhs.get_string(), lhs.is_unsure | rhs.is_unsure);
				};
			};
			struct operator_le : public even_simpler_bool_binary_operator_impl {
				value_container do_eval_int(value_type, evaluation_context errors, const value_container lhs, const value_container rhs) const {
					return value_container::create_int(lhs.get_int() <= rhs.get_int(), lhs.is_unsure | rhs.is_unsure);
				}
				value_container do_eval_float(value_type, evaluation_context errors, const value_container lhs, const value_container rhs) const {
					return value_container::create_int(lhs.get_float() <= rhs.get_float(), lhs.is_unsure | rhs.is_unsure);
				}
				value_container do_eval_string(value_type, evaluation_context errors, const value_container lhs, const value_container rhs) const {
					return value_container::create_int(lhs.get_string() <= rhs.get_string(), lhs.is_unsure | rhs.is_unsure);
				};
			};
			struct operator_ge : public even_simpler_bool_binary_operator_impl {
				value_container do_eval_int(value_type, evaluation_context errors, const value_container lhs, const value_container rhs) const {
					return value_container::create_int(lhs.get_int() >= rhs.get_int(), lhs.is_unsure | rhs.is_unsure);
				}
				value_container do_eval_float(value_type, evaluation_context errors, const value_container lhs, const value_container rhs) const {
					return value_container::create_int(lhs.get_float() >= rhs.get_float(), lhs.is_unsure | rhs.is_unsure);
				}
				value_container do_eval_string(value_type, evaluation_context errors, const value_container lhs, const value_container rhs) const {
					return value_container::create_int(lhs.get_string() >= rhs.get_string(), lhs.is_unsure | rhs.is_unsure);
				};
			};

			struct operator_and : public simple_int_binary_operator_impl {
				value_container eval_int(eval_helper &helper) const {
					long long lhsi = helper.get_lhs().get_int();
					if (!lhsi && !helper.get_lhs().is_unsure)
						return value_container::create_bool(false, false);
					long long rhsi = helper.get_rhs().get_int();
					if (!rhsi && !helper.get_rhs().is_unsure)
						return value_container::create_bool(false, false);
					return value_container::create_int(lhsi && rhsi, helper.get_lhs().is_unsure | helper.get_rhs().is_unsure);
				}
			};
			struct operator_or : public simple_int_binary_operator_impl {
				value_container eval_int(eval_helper &helper) const {
					long long lhsi = helper.get_lhs().get_int();
					if (lhsi && !helper.get_lhs().is_unsure)
						return value_container::create_bool(true, false);
					long long rhsi = helper.get_rhs().get_int();
					if (rhsi && !helper.get_rhs().is_unsure)
						return value_container::create_bool(true, false);
					return value_container::create_int(lhsi || rhsi, helper.get_lhs().is_unsure | helper.get_rhs().is_unsure);
				}
			};

			struct operator_like : public simple_bool_binary_operator_impl {
				value_container eval_int(value_type type, evaluation_context errors, const node_type left, const node_type right) const {
					errors->error("Like not supported on numbers...");
					return value_container::create_nil();
				};
				value_container eval_float(value_type type, evaluation_context errors, const node_type left, const node_type right) const {
					errors->error("Like not supported on numbers...");
					return value_container::create_nil();
				};
				value_container eval_string(value_type type, evaluation_context errors, const node_type left, const node_type right) const {
					value_container lhs = left->get_value(errors, type_string);
					value_container rhs = right->get_value(errors, type_string);
					if (!lhs.is(type_string) || !rhs.is(type_string)) {
						errors->error("invalid type");
						return value_container::create_nil();
					}
					std::string s1 = boost::algorithm::to_lower_copy(lhs.get_string());
					std::string s2 = boost::algorithm::to_lower_copy(rhs.get_string());
					if (s1.size() == 0 && s2.size() == 0)
						return value_container::create_int(1, lhs.is_unsure || rhs.is_unsure);
					if (s1.size() == 0 || s2.size() == 0)
						return value_container::create_int(0, lhs.is_unsure || rhs.is_unsure);
					if (s1.size() > s2.size() && s2.size() > 0)
						return value_container::create_int(s1.find(s2) != std::string::npos, lhs.is_unsure || rhs.is_unsure);
					return value_container::create_int(s2.find(s1) != std::string::npos, lhs.is_unsure || rhs.is_unsure);
				}
			};
			struct operator_regexp : public simple_bool_binary_operator_impl {
				value_container eval_int(value_type type, evaluation_context errors, const node_type left, const node_type right) const {
					errors->error("Like not supported on numbers...");
					return value_container::create_nil();
				};
				value_container eval_float(value_type type, evaluation_context errors, const node_type left, const node_type right) const {
					errors->error("Like not supported on numbers...");
					return value_container::create_nil();
				};
				value_container eval_string(value_type type, evaluation_context errors, const node_type left, const node_type right) const {
					value_container lhs = left->get_value(errors, type_string);
					value_container rhs = right->get_value(errors, type_string);
					if (!lhs.is(type_string) || !rhs.is(type_string)) {
						errors->error("invalid type");
						return value_container::create_nil();
					}
					std::string str = lhs.get_string();
					std::string regexp = rhs.get_string();
					try {
						boost::regex re(regexp);
						return value_container::create_int(boost::regex_match(str, re), lhs.is_unsure || rhs.is_unsure);
					} catch (const boost::bad_expression e) {
						errors->error("Invalid syntax in regular expression:" + regexp);
						return value_container::create_nil();
					} catch (...) {
						errors->error("Invalid syntax in regular expression:" + regexp);
						return value_container::create_nil();
					}
				}
			};
			struct operator_not_regexp : public simple_bool_binary_operator_impl {
				value_container eval_int(value_type type, evaluation_context errors, const node_type left, const node_type right) const {
					errors->error("Like not supported on numbers...");
					return value_container::create_nil();
				};
				value_container eval_float(value_type type, evaluation_context errors, const node_type left, const node_type right) const {
					errors->error("Like not supported on numbers...");
					return value_container::create_nil();
				};
				value_container eval_string(value_type type, evaluation_context errors, const node_type left, const node_type right) const {
					value_container lhs = left->get_value(errors, type_string);
					value_container rhs = right->get_value(errors, type_string);
					if (!lhs.is(type_string) || !rhs.is(type_string)) {
						errors->error("invalid type");
						return value_container::create_nil();
					}
					std::string str = lhs.get_string();
					std::string regexp = rhs.get_string();
					try {
						boost::regex re(regexp);
						return value_container::create_int(!boost::regex_match(str, re), lhs.is_unsure || rhs.is_unsure);
					} catch (const boost::bad_expression e) {
						errors->error("Invalid syntax in regular expression:" + regexp);
						return value_container::create_nil();
					} catch (...) {
						errors->error("Invalid syntax in regular expression:" + regexp);
						return value_container::create_nil();
					}
				}
			};
			struct operator_not_like : public simple_bool_binary_operator_impl {
				value_container eval_int(value_type type, evaluation_context errors, const node_type left, const node_type right) const {
					errors->error("Like not supported on numbers...");
					return value_container::create_nil();
				};
				value_container eval_float(value_type type, evaluation_context errors, const node_type left, const node_type right) const {
					errors->error("Like not supported on numbers...");
					return value_container::create_nil();
				};
				value_container eval_string(value_type type, evaluation_context errors, const node_type left, const node_type right) const {
					value_container lhs = left->get_value(errors, type_string);
					value_container rhs = right->get_value(errors, type_string);
					if (!lhs.is(type_string) || !rhs.is(type_string)) {
						errors->error("invalid type");
						return value_container::create_nil();
					}
					std::string s1 = boost::algorithm::to_lower_copy(lhs.get_string());
					std::string s2 = boost::algorithm::to_lower_copy(rhs.get_string());
					if (s1.size() == 0 && s2.size() == 0)
						return value_container::create_int(0, lhs.is_unsure || rhs.is_unsure);
					if (s1.size() == 0 || s2.size() == 0)
						return value_container::create_int(1, lhs.is_unsure || rhs.is_unsure);
					if (s1.size() > s2.size() && s2.size() > 0)
						return value_container::create_int(s1.find(s2) == std::string::npos, lhs.is_unsure || rhs.is_unsure);
					return value_container::create_int(s2.find(s1) == std::string::npos, lhs.is_unsure || rhs.is_unsure);
				}
			};
			struct operator_not_in : public simple_bool_binary_operator_impl {
				value_container eval_int(value_type type, evaluation_context errors, const node_type left, const node_type right) const {
					value_container lhs = left->get_value(errors, type_int);
					long long val = lhs.get_int();
					BOOST_FOREACH(node_type itm, right->get_list_value(errors)) {
						if (itm->get_int_value(errors) == val)
							return value_container::create_int(false, lhs.is_unsure);
					}
					return value_container::create_int(true, lhs.is_unsure);
				}
				value_container eval_float(value_type, evaluation_context errors, const node_type left, const node_type right) const {
					value_container lhs = left->get_value(errors, type_float);
					double val = lhs.get_float();
					BOOST_FOREACH(node_type itm, right->get_list_value(errors)) {
						if (itm->get_float_value(errors) == val)
							return value_container::create_int(false, lhs.is_unsure);
					}
					return value_container::create_int(true, lhs.is_unsure);
				}
				value_container eval_string(value_type, evaluation_context errors, const node_type left, const node_type right) const {
					value_container lhs = left->get_value(errors, type_string);
					std::string val = lhs.get_string();
					BOOST_FOREACH(node_type itm, right->get_list_value(errors)) {
						if (itm->get_string_value(errors) == val)
							return value_container::create_int(false, lhs.is_unsure);
					}
					return value_container::create_int(true, lhs.is_unsure);
				};
			};
			struct operator_in : public simple_bool_binary_operator_impl {
				value_container eval_int(value_type type, evaluation_context errors, const node_type left, const node_type right) const {
					value_container lhs = left->get_value(errors, type_int);
					long long val = lhs.get_int();
					BOOST_FOREACH(node_type itm, right->get_list_value(errors)) {
						long long cmp = itm->get_int_value(errors);
						if (cmp == val)
							return value_container::create_int(true, lhs.is_unsure);
					}
					return value_container::create_int(false, lhs.is_unsure);
				}
				value_container eval_float(value_type, evaluation_context errors, const node_type left, const node_type right) const {
					value_container lhs = left->get_value(errors, type_float);
					double val = lhs.get_float();
					BOOST_FOREACH(node_type itm, right->get_list_value(errors)) {
						if (itm->get_float_value(errors) == val)
							return value_container::create_int(true, lhs.is_unsure);
					}
					return value_container::create_int(false, lhs.is_unsure);
				}
				value_container eval_string(value_type, evaluation_context errors, const node_type left, const node_type right) const {
					value_container lhs = left->get_value(errors, type_string);
					std::string val = lhs.get_string();
					BOOST_FOREACH(node_type itm, right->get_list_value(errors)) {
						if (itm->get_string_value(errors) == val)
							return value_container::create_int(true, lhs.is_unsure);
					}
					return value_container::create_int(false, lhs.is_unsure);
				};
			};
			struct operator_false : public binary_operator_impl, unary_operator_impl, binary_function_impl {
				node_type evaluate(evaluation_context errors, const node_type, const node_type) const {
					errors->error("missing impl for FALSE");
					return factory::create_false();
				}
				node_type evaluate(evaluation_context errors, const node_type) const {
					errors->error("missing impl for FALSE");
					return factory::create_false();
				}
				node_type evaluate(parsers::where::value_type, evaluation_context errors, const node_type) const {
					errors->error("missing impl for FALSE");
					return factory::create_false();
				}
			};

			struct function_convert : public binary_function_impl {
				boost::optional<node_type> value;
				boost::optional<node_type> unit;
				function_convert(evaluation_context errors, const node_type subject) {
					std::list<node_type> args = subject->get_list_value(errors);
					std::list<node_type>::const_iterator item = args.begin();
					if (args.size() > 0) {
						value = *item;
					}
					if (args.size() > 1) {
						std::advance(item, 1);
						unit = *item;
					}
				}

				node_type evaluate(value_type type, evaluation_context errors, const node_type subject) const {
					if (!value) {
						errors->error("no arguments for convert(): " + subject->to_string());
						return factory::create_false();
					}
					node_type v = *value;
					if (unit) {
						node_type u = *unit;
						if (type == type_date) {
							return factory::create_int(parse_time(v->get_int_value(errors), u->get_string_value(errors)));
						}
						if (type == type_size) {
							return factory::create_int(parse_size(v->get_int_value(errors), u->get_string_value(errors)));
						}
						errors->error("could not convert to " + helpers::type_to_string(type) + " from " + v->to_string() + ", " + u->to_string());
						return factory::create_false();
					} else {
						if (helpers::type_is_int(type)) {
							if (v->is_float())
								return factory::create_int(v->get_float_value(errors));
						}
						if (helpers::type_is_float(type)) {
							if (v->is_int())
								return factory::create_float(v->get_int_value(errors));
						}
						return v;
					}
				}

				inline long long parse_time(long long value, std::string unit) const {
					long long now = constants::get_now();
					if (unit.empty())
						return now + value;
					else if ((unit == "s") || (unit == "S"))
						return now + (value);
					else if ((unit == "m") || (unit == "M"))
						return now + (value * 60);
					else if ((unit == "h") || (unit == "H"))
						return now + (value * 60 * 60);
					else if ((unit == "d") || (unit == "D"))
						return now + (value * 24 * 60 * 60);
					else if ((unit == "w") || (unit == "W"))
						return now + (value * 7 * 24 * 60 * 60);
					return now + value;
				}

				inline long long parse_size(long long value, std::string unit) const {
					if (unit.empty())
						return value;
					else if ((unit == "b") || (unit == "B"))
						return value;
					else if ((unit == "k") || (unit == "k"))
						return value * 1024;
					else if ((unit == "m") || (unit == "M"))
						return value * 1024 * 1024;
					else if ((unit == "g") || (unit == "G"))
						return value * 1024 * 1024 * 1024;
					else if ((unit == "t") || (unit == "T"))
						return value * 1024 * 1024 * 1024 * 1024;
					return value;
				}
			};

			struct simple_bool_unary_operator_impl : public unary_operator_impl {
				node_type evaluate(evaluation_context errors, const node_type subject) const {
					value_type type = subject->get_type();
					if (helpers::type_is_int(type))
						return eval_int(type, errors, subject) ? factory::create_true() : factory::create_false();
					if (type == type_string)
						return eval_string(type, errors, subject) ? factory::create_true() : factory::create_false();
					errors->error("missing impl for bool unary operator");
					return factory::create_false();
				}
				virtual bool eval_int(value_type type, evaluation_context errors, const node_type subject) const = 0;
				virtual bool eval_string(value_type type, evaluation_context errors, const node_type subject) const = 0;
			};

			struct operator_not : public unary_operator_impl, binary_function_impl {
				operator_not(const node_type) {}
				operator_not() {}
				node_type evaluate(evaluation_context errors, const node_type subject) const {
					return evaluate(subject->get_type(), errors, subject);
				}
				node_type evaluate(value_type type, evaluation_context errors, const node_type subject) const {
					if (type == type_bool)
						return subject->get_int_value(errors) ? factory::create_false() : factory::create_true();
					if (type == type_int)
						return  factory::create_int(-subject->get_int_value(errors));
					if (type == type_date) {
						long long now = constants::get_now();
						long long val = now - (subject->get_int_value(errors) - now);
						return factory::create_int(val);
					}
					errors->error("missing impl for NOT operator");
					return factory::create_false();
				}
			};
		}

		op_factory::bin_op_type op_factory::get_binary_operator(operators op, const node_type, const node_type) {
			// op_in, op_nin
			if (op == op_eq)
				return op_factory::bin_op_type(new operator_impl::operator_eq());
			if (op == op_gt)
				return op_factory::bin_op_type(new operator_impl::operator_gt());
			if (op == op_lt)
				return op_factory::bin_op_type(new operator_impl::operator_lt());
			if (op == op_le)
				return op_factory::bin_op_type(new operator_impl::operator_le());
			if (op == op_ge)
				return op_factory::bin_op_type(new operator_impl::operator_ge());
			if (op == op_ne)
				return op_factory::bin_op_type(new operator_impl::operator_ne());
			if (op == op_like)
				return op_factory::bin_op_type(new operator_impl::operator_like());
			if (op == op_not_like)
				return op_factory::bin_op_type(new operator_impl::operator_not_like());
			if (op == op_regexp)
				return op_factory::bin_op_type(new operator_impl::operator_regexp());
			if (op == op_not_regexp)
				return op_factory::bin_op_type(new operator_impl::operator_not_regexp());

			if (op == op_and)
				return op_factory::bin_op_type(new operator_impl::operator_and());
			if (op == op_or)
				return op_factory::bin_op_type(new operator_impl::operator_or());
			if (op == op_in)
				return op_factory::bin_op_type(new operator_impl::operator_in());
			if (op == op_nin)
				return op_factory::bin_op_type(new operator_impl::operator_not_in());

			if (op == op_binand)
				return op_factory::bin_op_type(new operator_impl::operator_and());
			if (op == op_binor)
				return op_factory::bin_op_type(new operator_impl::operator_or());

			std::cout << "======== UNHANDLED OPERATOR\n";
			return op_factory::bin_op_type(new operator_impl::operator_false());
		}

		bool op_factory::is_binary_function(std::string name) {
			if (name == "convert" || name == "auto_convert")
				return true;
			if (name == "neg")
				return true;
			return false;
		}
		op_factory::bin_fun_type op_factory::get_binary_function(evaluation_context errors, std::string name, const node_type subject) {
			if (name == "convert" || name == "auto_convert")
				return op_factory::bin_fun_type(new operator_impl::function_convert(errors, subject));
			if (name == "neg")
				return op_factory::bin_fun_type(new operator_impl::operator_not(subject));
			std::cout << "======== UNDEFINED FUNCTION: " << name << std::endl;
			return op_factory::bin_fun_type(new operator_impl::operator_false());
		}
		op_factory::un_op_type op_factory::get_unary_operator(operators op) {
			// op_inv, op_not
			if (op == op_not)
				return op_factory::un_op_type(new operator_impl::operator_not());
			std::cout << "======== UNHANDLED OPERATOR\n";
			return op_factory::un_op_type(new operator_impl::operator_false());
		}
	}
}