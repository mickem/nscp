#pragma once

namespace parsers {
	namespace where {

		///////////////////////////////////////////////////////////////////////////
		//  Walk the tree
		///////////////////////////////////////////////////////////////////////////
		template<typename THandler>
		struct ast_type_inference {
			typedef value_type result_type;

			varible_type_handler & handler;
			ast_type_inference(varible_type_handler & handler) : handler(handler) {}

			value_type operator()(expression_ast<THandler> & ast) {
				//std::wcout << _T(">>>Setting type: ") << ast.to_string() << _T(" to: ") << ast.get_type() << std::endl;
				value_type type = ast.get_type();
				//std::wcout << _T("!!!Setting type: ") << ast.to_string() << _T(" to: ") << type << std::endl;
				if (type != type_tbd)
					return type;
				type = boost::apply_visitor(*this, ast.expr);
				ast.set_type(type);
				//std::wcout << _T("<<<Setting type: ") << ast.to_string() << _T(" to: ") << ast.get_type() << std::endl;
				return type;
			}
			bool can_convert(value_type src, value_type dst) {
				if (src == type_invalid || dst == type_invalid)
					return false;
				if (dst == type_tbd)
					return false;
				if (src == type_tbd)
					return true;
				if (src == type_int && dst == type_string)
					return true;
				if (src == type_string && dst == type_int)
					return true;
				if (src >= type_custom_int && src < type_custom_int_end && dst == type_int)
					return true;
				if (src >= type_custom_string && src < type_custom_string_end && dst == type_int)
					return true;
				return false;
			}
			value_type infer_binary_type(expression_ast<THandler> & left, expression_ast<THandler> & right) {
				value_type rt = operator()(right);
				value_type lt = operator()(left);
				if (lt == rt)
					return lt;
				if (rt == type_invalid || lt == type_invalid)
					return type_invalid;
				if (rt == type_tbd && lt == type_tbd)
					return type_tbd;
				if (handler.can_convert(rt, lt)) {
					//std::wcout << _T("FORCE 001") << std::endl;
					right.force_type(lt);
					return lt;
				}
				if (handler.can_convert(lt, rt)) {
					//std::wcout << _T("FORCE 002") << std::endl;
					left.force_type(rt);
					return rt;
				}
				if (can_convert(rt, lt)) {
					//std::wcout << _T("FORCE 003") << std::endl;
					right.force_type(lt);
					return rt;
				}
				if (can_convert(lt, rt)) {
					//std::wcout << _T("FORCE 004") << std::endl;
					left.force_type(rt);
					return lt;
				}
				handler.error(_T("Invalid type detected for nodes: ") + left.to_string() + _T(" and " )+ right.to_string());
				return type_invalid;
			}

			value_type operator()(binary_op<THandler> & expr) {
				value_type type = infer_binary_type(expr.left, expr.right);
				if (type == type_invalid)
					return type;
				return get_return_type(expr.op, type);
			}
			value_type operator()(unary_op<THandler> & expr) {
				value_type type = operator()(expr.subject);
				return get_return_type(expr.op, type);
			}

			value_type operator()(unary_fun<THandler> & expr) {
				return type_tbd;
			}

			value_type operator()(list_value<THandler> & expr) {
				BOOST_FOREACH(expression_ast<THandler> &e, expr.list) {
					operator()(e);
				}
				return type_tbd;
			}

			value_type operator()(string_value & expr) {
				return type_string;
			}
			value_type operator()(int_value & expr) {
				return type_int;
			}
			value_type operator()(variable<THandler> & expr) {
				if (!handler.has_variable(expr.name)) {
					handler.error(_T("Variable not found: ") + expr.name);
					return type_invalid;
				}
				return handler.get_type(expr.name);
			}

			value_type operator()(nil & expr) {
				handler.error(_T("NULL node encountered"));
				return type_invalid;
			}
		};
	}
}