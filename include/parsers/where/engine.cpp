#include <parsers/where/engine.hpp>

namespace parsers {
	namespace where {
		engine::engine(std::string filter, error_handler error) : filter_string(filter), error(error) {}

		engine::boundries_type engine::fetch_performance_data() {
			return boundries.get_candidates();
		}

		bool engine::require_object(execution_context_type context) {
			if (requires_object)
				return *requires_object;
			requires_object = ast_parser.require_object(context);
			return *requires_object;
		}


		void engine::enabled_performance_collection() {
			perf_collection = true;
		}

		bool engine::validate(object_factory context) {
			if (error->is_debug())
				error->log_debug("Parsing: " + filter_string);

			if (!ast_parser.parse(context, filter_string)) {
				error->log_error("Parsing failed of '" + filter_string + "' at: " + ast_parser.rest);
				return false;
			}
			if (error->is_debug())
				error->log_debug("Parsing succeeded: " + ast_parser.result_as_tree());

			if (!ast_parser.derive_types(context) || context->has_error()) {
				error->log_error("Invalid types: " + context->get_error());
				return false;
			}
			if (error->is_debug())
				error->log_debug("Type resolution succeeded: " + ast_parser.result_as_tree());

			if (!ast_parser.bind(context) || context->has_error()) {
				error->log_error("Variable and function binding failed: " + context->get_error());
				return false;
			}
			if (error->is_debug())
				error->log_debug("Binding succeeded: " + ast_parser.result_as_tree());

			if (!ast_parser.static_eval(context) || context->has_error()) {
				error->log_error("Static evaluation failed: " + context->get_error());
				return false;
			}
			if (error->is_debug())
				error->log_debug("Static evaluation succeeded: " + ast_parser.result_as_tree());

			if (perf_collection) {
				if (!ast_parser.collect_perfkeys(context, boundries) || context->has_error()) {
					error->log_error("Collection of perfkeys failed: " + context->get_error());
					return false;
				}
			}
			return true;
		}

		bool engine::match(execution_context_type context) {
			value_container v = ast_parser.evaluate(context);
			if (context->has_error()) {
				error->log_error(context->get_error() + ": " + ast_parser.result_as_tree(context));
			}
			if (context->has_warn()) {
				error->log_warning(context->get_warn() + ": " + ast_parser.result_as_tree(context));
			}
			if (context->has_debug()) {
				error->log_debug(context->get_debug() + ": " + ast_parser.result_as_tree(context));
			}
			context->clear();
			if (v.is_unsure) {
				error->log_warning("Ignoring unsure result: " + ast_parser.result_as_tree(context));
			}
			return v.is_true();
		}
	}
}