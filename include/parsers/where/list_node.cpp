#include <boost/foreach.hpp>

#include <parsers/where/list_node.hpp>

namespace parsers {
	namespace where {

		std::string list_node::to_string() const {
			std::string ret;
			BOOST_FOREACH(const node_type n, value_) {
				if (!ret.empty())
					ret += ", ";
				ret += n->to_string();
			}
			return ret;
		}

		long long list_node::get_int_value(evaluation_context errors) const {
			long long ret = 0;
			BOOST_FOREACH(const node_type n, value_) {
				ret += n->get_int_value(errors);
			}
			return ret;
		}
		std::string list_node::get_string_value(evaluation_context errors) const {
			std::string ret;
			BOOST_FOREACH(const node_type n, value_) {
				if (!ret.empty())
					ret += ", ";
				ret += n->get_string_value(errors);
			}
			return ret;
		}

		node_type list_node::evaluate(evaluation_context errors) const {
			BOOST_FOREACH(const node_type n, value_) {
				n->evaluate(errors);
			}
			return factory::create_false();
		}
		bool list_node::bind(object_converter errors) {
			bool ret = true;
			BOOST_FOREACH(const node_type n, value_) {
				if (!n->bind(errors))
					ret = false;
			}
			return ret;
		}
		value_type list_node::infer_type(object_converter converter, value_type suggestion) {
			BOOST_FOREACH(const node_type n, value_) {
				n->infer_type(converter, suggestion);
			}
			return type_tbd;
		}
		value_type list_node::infer_type(object_converter converter) {
			bool first = true;
			value_type types = type_tbd;

			BOOST_FOREACH(const node_type n, value_) {
				if (first) {
					types = n->infer_type(converter);
					first = false;
				} else if (types != n->infer_type(converter)) {
					if (types != type_tbd) {
						types = type_tbd;
					}
				}
			}
			if (types != type_tbd)
				set_type(types);
			return types;
		}
		bool list_node::find_performance_data(evaluation_context context, performance_collector &collector) {
			BOOST_FOREACH(const node_type n, value_) {
				n->find_performance_data(context, collector);
			}
			return false;
		}
		bool list_node::static_evaluate(evaluation_context errors) const {
			BOOST_FOREACH(const node_type n, value_) {
				n->static_evaluate(errors);
			}
			return true;
		}
		bool list_node::require_object(evaluation_context errors) const {
			BOOST_FOREACH(const node_type n, value_) {
				n->require_object(errors);
			}
			return true;
		}

	}
}

