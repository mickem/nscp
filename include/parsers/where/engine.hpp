#pragma once

#include <parsers/where/node.hpp>
#include <parsers/where.hpp>

namespace parsers {
	namespace where {

		class error_handler_interface {
		public:
			virtual void log_error(const std::string error) = 0;
			virtual void log_warning(const std::string error) = 0;
			virtual void log_debug(const std::string error) = 0;
			virtual bool is_debug() const = 0;
			virtual void set_debug(bool debug) = 0;
		};

		struct engine {
			typedef boost::shared_ptr<error_handler_interface> error_handler;
			typedef parsers::where::evaluation_context execution_context_type;

			parsers::where::parser ast_parser;
			std::string filter_string;
			bool perf_collection;
			typedef parsers::where::performance_collector::boundries_type boundries_type;
			parsers::where::performance_collector boundries;
			error_handler error;

			engine(std::string filter, error_handler error) : filter_string(filter), error(error) {}

			boundries_type fetch_performance_data() {
				return boundries.get_candidates();
			}

			void enabled_performance_collection() {
				perf_collection = true;
			}

			bool validate(object_factory context) {
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

			bool match(execution_context_type context) {
				bool ret = ast_parser.evaluate(context);
				if (context->has_error()) {
					error->log_error("Error: " + context->get_error());
				}
				context->clear_errors();
				return ret;
			}

			std::string get_subject() { return filter_string; }
		};
	}
}