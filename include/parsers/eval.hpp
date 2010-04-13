#pragma once

namespace parsers {
	namespace where {
		namespace operator_impl {
			template<typename THandler>
			struct simple_bool_binary_operator_impl : public binary_operator_impl<THandler> {
				expression_ast<THandler> evaluate(THandler &handler, const expression_ast<THandler> &left, const expression_ast<THandler> & right) const {
					if (left.get_type() != right.get_type()) {
						handler.error(_T("Invalid types (not same) for binary operator"));
						return expression_ast<THandler>(int_value(FALSE));
					}
					value_type type = left.get_type();
					if (type_is_int(type))
						return eval_int(type, handler, left, right)?expression_ast<THandler>(int_value(TRUE)):expression_ast<THandler>(int_value(FALSE));
					if (type == type_string)
						return eval_string(type, handler, left, right)?expression_ast<THandler>(int_value(TRUE)):expression_ast<THandler>(int_value(FALSE));
					handler.error(_T("missing impl for simple bool binary operator"));
					return expression_ast<THandler>(int_value(FALSE));
				}
				virtual bool eval_int(value_type type, THandler &handler, const expression_ast<THandler> &left, const expression_ast<THandler> & right) const = 0;
				virtual bool eval_string(value_type type, THandler &handler, const expression_ast<THandler> &left, const expression_ast<THandler> & right) const = 0;
			};

			template<typename THandler>
			struct operator_and : public simple_bool_binary_operator_impl<THandler> {
				bool eval_int(value_type type, THandler &handler, const expression_ast<THandler> &left, const expression_ast<THandler> & right) const {
					return left.get_int(handler) && right.get_int(handler);
				}
				bool eval_string(value_type type, THandler &handler, const expression_ast<THandler> &left, const expression_ast<THandler> & right) const {
					handler.error(_T("missing impl for and binary operator"));
					// TODO convert strings
					return false;
				};
			};
			template<typename THandler>
			struct operator_or : public simple_bool_binary_operator_impl<THandler> {
				bool eval_int(value_type type, THandler &handler, const expression_ast<THandler> &left, const expression_ast<THandler> & right) const {
					return left.get_int(handler) || right.get_int(handler);
				}
				bool eval_string(value_type type, THandler &handler, const expression_ast<THandler> &left, const expression_ast<THandler> & right) const {
					handler.error(_T("missing impl for or binary operator"));
					// TODO convert strings
					return false;
				};
			};
			template<typename THandler>
			struct operator_eq : public simple_bool_binary_operator_impl<THandler> {
				bool eval_int(value_type type, THandler &handler, const expression_ast<THandler> &left, const expression_ast<THandler> & right) const {
					return left.get_int(handler) == right.get_int(handler);
				}
				bool eval_string(value_type type, THandler &handler, const expression_ast<THandler> &left, const expression_ast<THandler> & right) const { 
					return left.get_string(handler) == right.get_string(handler);
				};
			};
			template<typename THandler>
			struct operator_ne : public simple_bool_binary_operator_impl<THandler> {
				bool eval_int(value_type type, THandler &handler, const expression_ast<THandler> &left, const expression_ast<THandler> & right) const {
					return left.get_int(handler) != right.get_int(handler);
				}
				bool eval_string(value_type type, THandler &handler, const expression_ast<THandler> &left, const expression_ast<THandler> & right) const { 
					return left.get_string(handler) != right.get_string(handler);
				};
			};
			template<typename THandler>
			struct operator_gt : public simple_bool_binary_operator_impl<THandler> {
				bool eval_int(value_type type, THandler &handler, const expression_ast<THandler> &left, const expression_ast<THandler> & right) const {
					return left.get_int(handler) > right.get_int(handler);
				}
				bool eval_string(value_type type, THandler &handler, const expression_ast<THandler> &left, const expression_ast<THandler> & right) const { 
					return left.get_string(handler) > right.get_string(handler);
				};
			};
			template<typename THandler>
			struct operator_lt : public simple_bool_binary_operator_impl<THandler> {
				bool eval_int(value_type type, THandler &handler, const expression_ast<THandler> &left, const expression_ast<THandler> & right) const {
					return left.get_int(handler) < right.get_int(handler);
				}
				bool eval_string(value_type type, THandler &handler, const expression_ast<THandler> &left, const expression_ast<THandler> & right) const { 
					return left.get_string(handler) < right.get_string(handler);
				};
			};
			template<typename THandler>
			struct operator_le : public simple_bool_binary_operator_impl<THandler> {
				bool eval_int(value_type type, THandler &handler, const expression_ast<THandler> &left, const expression_ast<THandler> & right) const {
					return left.get_int(handler) <= right.get_int(handler);
				}
				bool eval_string(value_type type, THandler &handler, const expression_ast<THandler> &left, const expression_ast<THandler> & right) const { 
					return left.get_string(handler) <= right.get_string(handler);
				};
			};
			template<typename THandler>
			struct operator_ge : public simple_bool_binary_operator_impl<THandler> {
				bool eval_int(value_type type, THandler &handler, const expression_ast<THandler> &left, const expression_ast<THandler> & right) const {
					return left.get_int(handler) >= right.get_int(handler);
				}
				bool eval_string(value_type type, THandler &handler, const expression_ast<THandler> &left, const expression_ast<THandler> & right) const { 
					return left.get_string(handler) >= right.get_string(handler);
				};
			};
			template<typename THandler>
			struct operator_like : public simple_bool_binary_operator_impl<THandler> {
				bool eval_int(value_type type, THandler &handler, const expression_ast<THandler> &left, const expression_ast<THandler> & right) const {
					return false;
				}
				bool eval_string(value_type type, THandler &handler, const expression_ast<THandler> &left, const expression_ast<THandler> & right) const { 
					std::wstring s1 = left.get_string(handler);
					std::wstring s2 = right.get_string(handler);
					bool res;
					if (s1.size() > s2.size() && s2.size() > 0)
						return s1.find(s2) != std::wstring::npos;
					return s2.find(s1) != std::wstring::npos;
					//if (res)
					//	std::wcout << _T("Found: ") << s1 << _T(" in ") << s2 << std::endl;
					return res;
				};
			};
			template<typename THandler>
			struct operator_false : public binary_operator_impl<THandler>, unary_operator_impl<THandler>, binary_function_impl<THandler> {
				expression_ast<THandler> evaluate(THandler &handler, const expression_ast<THandler> &left, const expression_ast<THandler> & right) const {
					handler.error(_T("missing impl for FALSE"));
					return expression_ast<THandler>(int_value(FALSE));
				}
				expression_ast<THandler> evaluate(THandler &handler, const expression_ast<THandler> &subject) const {
					handler.error(_T("missing impl for FALSE"));
					return expression_ast<THandler>(int_value(FALSE));
				}
				expression_ast<THandler> evaluate(parsers::where::value_type type,THandler &handler, const expression_ast<THandler> &subject) const {
					handler.error(_T("missing impl for FALSE"));
					return expression_ast<THandler>(int_value(FALSE));
				}
			};

			template<typename THandler>
			struct function_convert : public binary_function_impl<THandler> {
				typename expression_ast<THandler> list_entry;
				typedef typename expression_ast<THandler>::list_type list_type;
				typename expression_ast<THandler>::list_type list;
				bool single_item;
				function_convert(const expression_ast<THandler> &subject) : list(subject.get_list()), single_item(list.size()==1) {}
				expression_ast<THandler> evaluate(value_type type, THandler &handler, const expression_ast<THandler> &subject) const {
					if (single_item) {
						if (type_is_int(type)) {
							return expression_ast<THandler>(int_value(list.front().get_int(handler)));
						}
						if (type == type_string) {
							return expression_ast<THandler>(string_value(list.front().get_string(handler)));
						}
						handler.error(_T("1:Failed to handle type: ") + to_string(type));
						return expression_ast<THandler>(int_value(FALSE));
					}
					if (list.size()==2) {
						list_type::const_iterator item = list.begin();
						list_type::const_iterator unit = item;
						std::advance(unit, 1);
						if (type == type_date) {
							return expression_ast<THandler>(int_value(parse_time((*item).get_int(handler), (*unit).get_string(handler))));
						}
						handler.error(_T("m:Failed to handle type: ") + to_string(type) + _T(" ") + (*item).to_string() + _T(", ") + (*unit).to_string());
						return expression_ast<THandler>(int_value(FALSE));
					}
					std::wcout << _T("----------------------------------------------\n");
					std::wcout << list.size() << _T("\n");
					std::wcout << subject.to_string() << _T("\n");
					std::wcout << _T("----------------------------------------------\n");
					handler.error(_T("Missing implementation for convert function"));
					return expression_ast<THandler>(int_value(FALSE));
				}

				inline long long parse_time(unsigned int value, std::wstring unit) const {
					long long now = constants::get_now();
					if (unit.empty())
						return now + value;
					else if ( (unit == _T("s")) || (unit == _T("S")) )
						return now + (value);
					else if ( (unit == _T("m")) || (unit == _T("M")) )
						return now + (value * 60);
					else if ( (unit == _T("h")) || (unit == _T("H")) )
						return now + (value * 60 * 60);
					else if ( (unit == _T("d")) || (unit == _T("D")) )
						return now + (value * 24 * 60 * 60);
					else if ( (unit == _T("w")) || (unit == _T("W")) )
						return now + (value * 7 * 24 * 60 * 60);
					return now + value;
				}

			};


			template<typename THandler>
			struct simple_bool_unary_operator_impl : public unary_operator_impl<THandler> {
				expression_ast<THandler> evaluate(THandler &handler, const expression_ast<THandler> &subject) const {
					value_type type = subject.get_type();
					if (type_is_int(type))
						return eval_int(type, handler, subject)?expression_ast<THandler>(int_value(TRUE)):expression_ast<THandler>(int_value(FALSE));
					if (type == type_string)
						return eval_string(type, handler, subject)?expression_ast<THandler>(int_value(TRUE)):expression_ast<THandler>(int_value(FALSE));
					handler.error(_T("missing impl for bool unary operator"));
					return expression_ast<THandler>(int_value(FALSE));
				}
				virtual bool eval_int(value_type type, THandler &handler, const expression_ast<THandler> &subject) const = 0;
				virtual bool eval_string(value_type type, THandler &handler, const expression_ast<THandler> &subject) const = 0;
			};

			template<typename THandler>
			struct operator_not : public unary_operator_impl<THandler>, binary_function_impl<THandler> {
				operator_not(const expression_ast<THandler> &subject) {}
				operator_not() {}
				expression_ast<THandler> evaluate(THandler &handler, const expression_ast<THandler> &subject) const {
					return evaluate(subject.get_type(), handler, subject);
				}
				expression_ast<THandler> evaluate(value_type type, THandler &handler, const expression_ast<THandler> &subject) const {
					if (type == type_bool)
						return subject.get_int(handler)?expression_ast<THandler>(int_value(TRUE)):expression_ast<THandler>(int_value(FALSE));
					if (type == type_int)
						return  expression_ast<THandler>(int_value(-subject.get_int(handler)));
					if (type == type_date) {
						long long now = constants::get_now();
						long long val = now - (subject.get_int(handler) - now);
						return expression_ast<THandler>(int_value(val));
					}
					handler.error(_T("missing impl for NOT operator"));
					return expression_ast<THandler>(int_value(FALSE));
				}
			};
		}
		template<typename THandler>
		typename factory<THandler>::bin_op_type factory<THandler>::get_binary_operator(operators op) {
			// op_in, op_nin
			if (op == op_eq)
				return bin_op_type(new operator_impl::operator_eq<THandler>());
			if (op == op_gt)
				return bin_op_type(new operator_impl::operator_gt<THandler>());
			if (op == op_lt)
				return bin_op_type(new operator_impl::operator_lt<THandler>());
			if (op == op_le)
				return bin_op_type(new operator_impl::operator_le<THandler>());
			if (op == op_ge)
				return bin_op_type(new operator_impl::operator_ge<THandler>());
			if (op == op_ne)
				return bin_op_type(new operator_impl::operator_ne<THandler>());
			if (op == op_like)
				return bin_op_type(new operator_impl::operator_like<THandler>());

			if (op == op_and)
				return bin_op_type(new operator_impl::operator_and<THandler>());
			if (op == op_or)
				return bin_op_type(new operator_impl::operator_or<THandler>());
			std::cout << "======== UNHANDLED OPERATOR\n";
			return bin_op_type(new operator_impl::operator_false<THandler>());
		}

		template<typename THandler>
		typename factory<THandler>::bin_fun_type factory<THandler>::get_binary_function(std::wstring name, const expression_ast<THandler> &subject) {
			if (name == _T("convert"))
				return bin_fun_type(new operator_impl::function_convert<THandler>(subject));
			if (name == _T("auto_convert"))
				return bin_fun_type(new operator_impl::function_convert<THandler>(subject));
			if (name == _T("neg"))
				return bin_fun_type(new operator_impl::operator_not<THandler>(subject));
			std::wcout << _T("======== UNDEFINED FUNCTION: ") << name << std::endl;
			return bin_fun_type(new operator_impl::operator_false<THandler>());
		}
		template<typename THandler>
		typename factory<THandler>::un_op_type factory<THandler>::get_unary_operator(operators op) {
			// op_inv, op_not
			if (op == op_not)
				return un_op_type(new operator_impl::operator_not<THandler>());
			std::cout << "======== UNHANDLED OPERATOR\n";
			return un_op_type(new operator_impl::operator_false<THandler>());
		}
	}
}
