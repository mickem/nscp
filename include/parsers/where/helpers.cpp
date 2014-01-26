#include <strEx.h>

#include <parsers/where/helpers.hpp>

namespace parsers {
	namespace where {
		namespace helpers {

			std::string type_to_string(value_type type) {
				if (type == type_bool)
					return "bool";
				if (type == type_string)
					return "string";
				if (type == type_int)
					return "int";
				if (type == type_date)
					return "date";
				if (type == type_size)
					return "size";
				if (type == type_invalid)
					return "invalid";
				if (type == type_tbd)
					return "tbd";
				if (type >= type_custom)
					return "u:" + strEx::s::xtos(type-type_custom);
				if (type >= type_custom_string)
					return "us:" + strEx::s::xtos(type-type_custom_string);
				if (type >= type_custom_int)
					return "ui:" + strEx::s::xtos(type-type_custom_int);
				return "unknown:" + strEx::s::xtos(type);
			}

			bool type_is_int(value_type type) {
				return type == type_int || type == type_bool || type == type_date || type == type_size || (type >= type_custom_int && type < type_custom_int_end);
			}
			bool type_is_string(value_type type) {
				return type == type_string || (type >= type_custom_string && type < type_custom_string_end);
			}

			value_type get_return_type(operators op, value_type type) {
				if (op == op_inv)
					return type;
				if (op == op_binor)
					return type;
				if (op == op_binand)
					return type;
				return type_bool;
			}
			std::string operator_to_string(operators const& identifier) {
				if (identifier == op_and)
					return "and";
				if (identifier == op_or)
					return "or";
				if (identifier == op_eq)
					return "=";
				if (identifier == op_gt)
					return ">";
				if (identifier == op_lt)
					return "<";
				if (identifier == op_ge)
					return ">=";
				if (identifier == op_le)
					return "<=";
				if (identifier == op_in)
					return "in";
				if (identifier == op_nin)
					return "not in";
				if (identifier == op_binand)
					return "&";
				if (identifier == op_binor)
					return "|";
				if (identifier == op_like)
					return "like";
				return "?";
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
				if (src == type_int && dst == type_bool)
					return true;
				if (src == type_string && dst == type_int)
					return true;
				if (src == type_bool && dst == type_int)
					return true;
				if (src >= type_custom_int && src < type_custom_int_end && dst == type_int)
					return true;
				if (src >= type_custom_string && src < type_custom_string_end && dst == type_int)
					return true;
				return false;
			}

			node_type add_convert_node(node_type subject, value_type newtype) {
				value_type type = subject->get_type();
				if (type == newtype)
					return subject;
				if (type != newtype && type != type_tbd) {
					node_type subnode = factory::create_conversion(subject);
					subnode->set_type(newtype);
					return subnode;
				}
				// Not same type, and new is tbd (so just digest the new type)
				subject->set_type(newtype);
				return subject;
			}

			value_type infer_binary_type(object_converter factory, node_type &left, node_type &right) {
				value_type rt = right->infer_type(factory);
				value_type lt = left->infer_type(factory);
				if (lt == type_multi || rt == type_multi) {
					if (lt == rt)
						return type_tbd;
					if (lt == type_multi)
						lt = left->infer_type(factory, rt);
					else 
						rt = right->infer_type(factory, lt);
				}
				if (lt == rt)
					return lt;
				if (rt == type_invalid || lt == type_invalid)
					return type_invalid;
				if (rt == type_tbd && lt == type_tbd)
					return type_tbd;
				if (factory->can_convert(rt, lt)) {
					right = add_convert_node(right, lt);
					return lt;
				}
				if (factory->can_convert(lt, rt)) {
					left = add_convert_node(left, rt);
					return rt;
				}
				if (can_convert(rt, lt)) {
					right = add_convert_node(right, lt);
					return lt;
				}
				if (can_convert(lt, rt)) {
					left = add_convert_node(left, rt);
					return rt;
				}
				factory->error("Cannot compare " + left->to_string() + " to " + right->to_string() + " (" + type_to_string(lt) + " to " + type_to_string(rt) + ")");
				return type_invalid;
			}

			bool is_lower(operators op) {
				return op == op_le || op == op_lt;
			}
			bool is_upper(operators op) {
				return op == op_ge || op == op_gt;
			}
		}
	}
}

