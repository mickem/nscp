#include <iostream>

#include <boost/regex.hpp>
#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/make_shared.hpp>

#include <parsers/operators.hpp>
#include <parsers/helpers.hpp>
#include <parsers/where/helpers.hpp>

namespace parsers {
	namespace where {
		static bool debug_enabled = false;
		static int debug_level = 15;

		namespace operator_impl {

			struct simple_bool_binary_operator_impl : public binary_operator_impl {
				node_type evaluate(evaluation_context errors, const node_type left, const node_type right) const {
					value_type ltype = left->get_type();
					value_type rtype = right->get_type();

					if ( (ltype != rtype) && (rtype != type_tbd) ) {
						errors->error("Invalid types (not same) for binary operator");
						return factory::create_false();
					}
					value_type type = left->get_type();
					if (helpers::type_is_int(type))
						return eval_int(type, errors,  left, right)?factory::create_true():factory::create_false();
					if (type == type_string)
						return eval_string(type, errors,  left, right)?factory::create_true():factory::create_false();
					errors->error("missing impl for simple bool binary operator");
					return factory::create_false();
				}
				virtual bool eval_int(value_type type, evaluation_context errors, const node_type left, const node_type right) const = 0;
				virtual bool eval_string(value_type type, evaluation_context errors, const node_type left, const node_type right) const = 0;
			};

			struct simple_int_binary_operator_impl : public binary_operator_impl {
				node_type evaluate(evaluation_context errors, const node_type left, const node_type right) const {
					value_type ltype = left->get_type();
					value_type rtype = right->get_type();

					if ( (ltype != rtype) && (rtype != type_tbd) ) {
						errors->error("Invalid types (not same) for binary operator");
						return factory::create_false();
					}
					value_type type = left->get_type();
					if (helpers::type_is_int(type))
						return factory::create_int(eval_int(type, errors,  left, right));
					if (type == type_string)
						return factory::create_int(eval_string(type, errors,  left, right));
					errors->error("missing impl for simple bool binary operator");
					return factory::create_false();
				}
				virtual long long eval_int(value_type type, evaluation_context errors, const node_type left, const node_type right) const = 0;
				virtual long long eval_string(value_type type, evaluation_context errors, const node_type left, const node_type right) const = 0;
			};

			struct operator_and : public simple_bool_binary_operator_impl {
				bool eval_int(value_type, evaluation_context errors, const node_type left, const node_type right) const {
					return left->get_int_value(errors) && right->get_int_value(errors);
				}
				bool eval_string(value_type, evaluation_context errors, const node_type, const node_type) const {
					errors->error("missing impl for and binary operator");
					return false;
				};
			};
			struct operator_or : public simple_bool_binary_operator_impl {
				bool eval_int(value_type, evaluation_context errors, const node_type left, const node_type right) const {
					return left->get_int_value(errors) || right->get_int_value(errors);
				}
				bool eval_string(value_type, evaluation_context errors, const node_type, const node_type) const {
					errors->error("missing impl for or binary operator");
					return false;
				};
			};
			struct operator_eq : public simple_bool_binary_operator_impl {
				bool eval_int(value_type, evaluation_context errors, const node_type left, const node_type right) const {
					return left->get_int_value(errors) == right->get_int_value(errors);
				}
				bool eval_string(value_type, evaluation_context errors, const node_type left, const node_type right) const {
					if (debug_enabled && debug_level > 10) {
						std::string lhs = left->get_string_value(errors);
						std::string rhs = right->get_string_value(errors);
						std::cout << "(op_gt) " << lhs << " > " << rhs << std::endl;
					}
					return left->get_string_value(errors) == right->get_string_value(errors);
				};
			};
			struct operator_ne : public simple_bool_binary_operator_impl {
				bool eval_int(value_type, evaluation_context errors, const node_type left, const node_type right) const {
					return left->get_int_value(errors) != right->get_int_value(errors);
				}
				bool eval_string(value_type, evaluation_context errors, const node_type left, const node_type right) const { 
					return left->get_string_value(errors) != right->get_string_value(errors);
				};
			};
			struct operator_gt : public simple_bool_binary_operator_impl {
				bool eval_int(value_type, evaluation_context errors, const node_type left, const node_type right) const {
					if (debug_enabled && debug_level > 10) {
						long long lhs = left->get_int_value(errors);
						long long rhs = right->get_int_value(errors);
						std::cout << "(op_gt) " << lhs << " > " << rhs << std::endl;
					}
					return left->get_int_value(errors) > right->get_int_value(errors);
				}
				bool eval_string(value_type, evaluation_context errors, const node_type left, const node_type right) const { 
					return left->get_string_value(errors) > right->get_string_value(errors);
				};
			};
			struct operator_lt : public simple_bool_binary_operator_impl {
				bool eval_int(value_type, evaluation_context errors, const node_type left, const node_type right) const {
					if (debug_enabled && debug_level > 10) {
						long long lhs = left->get_int_value(errors);
						long long rhs = right->get_int_value(errors);
						std::cout << "(op_lt) " << lhs << " < " << rhs << std::endl;
					}
					return left->get_int_value(errors) < right->get_int_value(errors);
				}
				bool eval_string(value_type, evaluation_context errors, const node_type left, const node_type right) const { 
					return left->get_string_value(errors) < right->get_string_value(errors);
				};
			};
			struct operator_le : public simple_bool_binary_operator_impl {
				bool eval_int(value_type, evaluation_context errors, const node_type left, const node_type right) const {
					return left->get_int_value(errors) <= right->get_int_value(errors);
				}
				bool eval_string(value_type, evaluation_context errors, const node_type left, const node_type right) const { 
					return left->get_string_value(errors) <= right->get_string_value(errors);
				};
			};
			struct operator_ge : public simple_bool_binary_operator_impl {
				bool eval_int(value_type, evaluation_context errors, const node_type left, const node_type right) const {
					return left->get_int_value(errors) >= right->get_int_value(errors);
				}
				bool eval_string(value_type, evaluation_context errors, const node_type left, const node_type right) const { 
					return left->get_string_value(errors) >= right->get_string_value(errors);
				};
			};


			struct operator_bin_and : public simple_int_binary_operator_impl {
				long long eval_int(value_type, evaluation_context errors, const node_type left, const node_type right) const {
					return left->get_int_value(errors) & right->get_int_value(errors);
				}
				long long eval_string(value_type, evaluation_context,  const node_type, const node_type) const { 
					return 0;
				};
			};
			struct operator_bin_or : public simple_int_binary_operator_impl {
				long long eval_int(value_type, evaluation_context errors, const node_type left, const node_type right) const {
					return left->get_int_value(errors) | right->get_int_value(errors);
				}
				long long eval_string(value_type, evaluation_context,  const node_type, const node_type) const { 
					return 0;
				};
			};

			struct operator_like : public simple_bool_binary_operator_impl {
				bool eval_int(value_type, evaluation_context errors, const node_type, const node_type) const {
					errors->error("Like not supported on numbers...");
					return false;
				}
				bool eval_string(value_type, evaluation_context errors, const node_type left, const node_type right) const { 
					std::string s1 = left->get_string_value(errors);
					std::string s2 = right->get_string_value(errors);
					if (s1.size() == 0 && s2.size() == 0)
						return true;
					if (s1.size() == 0 || s2.size() == 0)
						return false;
					if (s1.size() > s2.size() && s2.size() > 0)
						return s1.find(s2) != std::string::npos;
					return s2.find(s1) != std::string::npos;
				};
			};
			struct operator_regexp : public simple_bool_binary_operator_impl {
				bool eval_int(value_type, evaluation_context errors, const node_type, const node_type) const {
					errors->error("Regular expression not supported on numbers...");
					return false;
				}
				bool eval_string(value_type, evaluation_context errors, const node_type left, const node_type right) const { 
					std::string str = left->get_string_value(errors);
					std::string regexp = right->get_string_value(errors);
					if (debug_enabled)
						std::cout << "(op_regexp) " << str << " regexp " << regexp << std::endl;
					try {
						boost::regex re(regexp);
						return boost::regex_match(str, re);
					} catch (const boost::bad_expression e) {
						errors->error("Invalid syntax in regular expression:" + regexp);
						return false;
					} catch (...) {
						errors->error("Invalid syntax in regular expression:" + regexp);
						return false;
					}
				};
			};
			struct operator_not_regexp : public simple_bool_binary_operator_impl {
				bool eval_int(value_type, evaluation_context errors, const node_type, const node_type) const {
					errors->error("Regular expression not supported on numbers...");
					return false;
				}
				bool eval_string(value_type, evaluation_context errors, const node_type left, const node_type right) const { 
					std::string str = left->get_string_value(errors);
					std::string regexp = right->get_string_value(errors);
					if (debug_enabled)
						std::cout << "(op_regexp) " << str << " regexp " << regexp << std::endl;
					try {
						boost::regex re(regexp);
						return !boost::regex_match(str, re);
					} catch (const boost::bad_expression e) {
						errors->error("Invalid syntax in regular expression:" + regexp);
						return false;
					} catch (...) {
						errors->error("Invalid syntax in regular expression:" + regexp);
						return false;
					}
				};
			};
			struct operator_not_like : public simple_bool_binary_operator_impl {
				bool eval_int(value_type, evaluation_context errors, const node_type, const node_type) const {
					errors->error("Not like not supported on numbers...");
					return false;
				}
				bool eval_string(value_type, evaluation_context errors, const node_type left, const node_type right) const { 
					std::string s1 = left->get_string_value(errors);
					std::string s2 = right->get_string_value(errors);
					if (s1.size() > s2.size() && s2.size() > 0)
						return s1.find(s2) == std::string::npos;
					return s2.find(s1) == std::string::npos;
				};
			};
			struct operator_not_in : public simple_bool_binary_operator_impl {
				bool eval_int(value_type, evaluation_context errors, const node_type left, const node_type right) const {
					long long val = left->get_int_value(errors);
					BOOST_FOREACH(node_type itm, right->get_list_value(errors)) {
						if (itm->get_int_value(errors) == val)
							return false;
					}
					return true;
				}
				bool eval_string(value_type, evaluation_context errors, const node_type left, const node_type right) const {
					std::string val = left->get_string_value(errors);
					BOOST_FOREACH(node_type itm, right->get_list_value(errors)) {
						if (itm->get_string_value(errors) == val)
							return false;
					}
					return true;
				};
			};
			struct operator_in : public simple_bool_binary_operator_impl {
				bool eval_int(value_type, evaluation_context errors, const node_type left, const node_type right) const {
					long long val = left->get_int_value(errors);
					BOOST_FOREACH(node_type itm, right->get_list_value(errors)) {
						if (itm->get_int_value(errors) == val)
							return true;
					}
					return false;
				}
				bool eval_string(value_type, evaluation_context errors, const node_type left, const node_type right) const {
					std::string val = left->get_string_value(errors);
					BOOST_FOREACH(node_type itm, right->get_list_value(errors)) {
						if (itm->get_string_value(errors) == val)
							return true;
					}
					return false;
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
						errors->error("Convert requires arguments");
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
						errors->error("m:Failed to handle type: " + helpers::type_to_string(type) + " " + v->to_string() + ", " + u->to_string());
						return factory::create_false();
					} else {
						return v;
					}
				}

				inline long long parse_time(long long value, std::string unit) const {
					long long now = constants::get_now();
					if (unit.empty())
						return now + value;
					else if ( (unit == "s") || (unit == "S") )
						return now + (value);
					else if ( (unit == "m") || (unit == "M") )
						return now + (value * 60);
					else if ( (unit == "h") || (unit == "H") )
						return now + (value * 60 * 60);
					else if ( (unit == "d") || (unit == "D") )
						return now + (value * 24 * 60 * 60);
					else if ( (unit == "w") || (unit == "W") )
						return now + (value * 7 * 24 * 60 * 60);
					return now + value;
				}

				inline long long parse_size(long long value, std::string unit) const {
					if (unit.empty())
						return value;
					else if ( (unit == "b") || (unit == "B") )
						return value;
					else if ( (unit == "k") || (unit == "k") )
						return value * 1024;
					else if ( (unit == "m") || (unit == "M") )
						return value * 1024 * 1024;
					else if ( (unit == "g") || (unit == "G") )
						return value * 1024 * 1024 * 1024;
					else if ( (unit == "t") || (unit == "T") )
						return value * 1024 * 1024 * 1024 * 1024;
					return value;
				}
				
			};


			struct simple_bool_unary_operator_impl : public unary_operator_impl {
				node_type evaluate(evaluation_context errors, const node_type subject) const {
					value_type type = subject->get_type();
					if (helpers::type_is_int(type))
						return eval_int(type, errors,  subject)?factory::create_true():factory::create_false();
					if (type == type_string)
						return eval_string(type, errors,  subject)?factory::create_true():factory::create_false();
					errors->error("missing impl for bool unary operator");
					return factory::create_false();
				}
				virtual bool eval_int(value_type type, evaluation_context errors, const node_type subject) const = 0;
				virtual bool eval_string(value_type type, evaluation_context errors, const node_type subject) const = 0;
			};

			struct operator_not : public unary_operator_impl, binary_function_impl {
				operator_not(const node_type subject) {subject;}
				operator_not() {}
				node_type evaluate(evaluation_context errors, const node_type subject) const {
					return evaluate(subject->get_type(), errors,  subject);
				}
				node_type evaluate(value_type type, evaluation_context errors, const node_type subject) const {
					if (type == type_bool)
						return subject->get_int_value(errors)?factory::create_false():factory::create_true();
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

		op_factory::bin_op_type op_factory::get_binary_operator(operators op, const node_type left, const node_type right) {
			left;
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
			if (op == op_regexp)
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
				return op_factory::bin_op_type(new operator_impl::operator_bin_and());
			if (op == op_binor)
				return op_factory::bin_op_type(new operator_impl::operator_bin_or());

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
