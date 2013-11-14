#pragma once

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

#include <parsers/expression/expression.hpp>
#include <parsers/where/engine_impl.hpp>

#include <NSCAPI.h>
#include <nscapi/nscapi_helper.hpp>

namespace parsers {
	namespace where {
		template<class Tobject>
		struct generic_summary;
	}
}

namespace modern_filter {

	template<class Tfactory>
	struct filter_text_renderer {
		struct my_entry {
			parsers::simple_expression::entry origin;
			parsers::where::node_type node;
			my_entry(const parsers::simple_expression::entry &origin) : origin(origin) {}
			my_entry(const my_entry &other) : origin(other.origin), node(other.node) {}
			const my_entry& operator= (const my_entry &other) {
				origin = other.origin;
				node = other.node;
				return *this;
			}
		};

		typedef std::list<my_entry> entry_list;
		entry_list entries;
		filter_text_renderer() {}

		bool parse(boost::shared_ptr<Tfactory> context, const std::string str, std::string &error) {
			parsers::simple_expression::result_type keys;
			parsers::simple_expression expr;
			if (!expr.parse(str, keys)) {
				error = "Failed to parse: " + str;
				return false;
			}
			BOOST_FOREACH(const parsers::simple_expression::entry &e, keys) {
				my_entry my_e(e);
				if (e.is_variable) {
					std::string tag = e.name;
					if (context->has_variable(tag)) {
						my_e.node = context->create_variable(tag, true);
					} else if (context->has_function(tag)) {
						my_e.node = context->create_text_function(tag);
					} else {
						error = "Invalid variable: " + e.name;
						return false;
					}
				}
				entries.push_back(my_e);
			}
			return true;
		}
		std::string render(boost::shared_ptr<Tfactory> context) {
			std::string ret;
			BOOST_FOREACH(const my_entry &e, entries) {
				if (!e.origin.is_variable)
					ret += e.origin.name;
				else
					ret += e.node->get_string_value(context);
			}
			return ret;
		}
	};


	class error_handler_impl : public parsers::where::error_handler_interface {
		std::string error;
		bool debug;
		error_handler_impl() {}
	public:
		error_handler_impl(bool debug) : debug(debug) {}
		void log_error(const std::string error);
		void log_warning(const std::string error);
		void log_debug(const std::string error);
		bool is_debug() const;
		void set_debug(bool debug_);
		bool has_errors() const;
	};

	template<class Tobject, class Tfactory>
	struct modern_filters {
		typedef boost::shared_ptr<error_handler_impl> error_type;
		typedef boost::shared_ptr<parsers::where::engine> filter_engine;
		typedef parsers::where::performance_collector::boundries_type boundries_type;
		typedef boost::shared_ptr<Tobject> object_type;

		filter_text_renderer<Tfactory> renderer_top;
		filter_text_renderer<Tfactory> renderer_detail;
		filter_text_renderer<Tfactory> renderer_perf;
		filter_engine engine_filter;
		filter_engine engine_warn;
		filter_engine engine_crit;
		filter_engine engine_ok;
		parsers::where::generic_summary<object_type> summary;
		bool has_matched;
		//std::string message;
		boost::shared_ptr<Tfactory> context;
		error_type error_handler;

		struct perf_entry {
			std::string label;
			parsers::where::node_type current_value;
			parsers::where::node_type crit_value;
			parsers::where::node_type warn_value;
			parsers::where::node_type maximum_value;
			parsers::where::node_type minimum_value;
		};


		parsers::where::perf_list_type performance_instance_data;

		typedef std::map<std::string,perf_entry> leaf_performance_entry_type;
		leaf_performance_entry_type leaf_performance_data;


		modern_filters() : context(new Tfactory()) {
			context->set_summary(&summary);
		}

		std::string get_filter_syntax() const {
			return context->get_filter_syntax() + summary.get_filter_syntax();
		}
		std::string get_format_syntax() const {
			return context->get_filter_syntax() + summary.get_format_syntax();
		}
		bool build_syntax(const std::string &top, const std::string &detail, const std::string &perf, std::string &gerror) {
			std::string lerror;
			if (!renderer_top.parse(context, top, lerror)) {
				gerror = "Invalid top-syntax: " + lerror;
				return false;
			}
			if (!renderer_detail.parse(context, detail, lerror)) {
				gerror = "Invalid syntax: " + lerror;
				return false;
			}
			if (!renderer_perf.parse(context, perf, lerror)) {
				gerror = "Invalid syntax: " + lerror;
				return false;
			}
			return true;
		}
		bool build_engines(const bool debug, const std::string &filter, const std::string &ok, const std::string &warn, const std::string &crit) {
			if (!error_handler)
				error_handler.reset(new error_handler_impl(debug));
			else
				error_handler->set_debug(debug);

			if (!filter.empty()) engine_filter.reset(new parsers::where::engine(filter, error_handler));
			if (!ok.empty()) engine_ok.reset(new parsers::where::engine(ok, error_handler));
			if (!warn.empty()) engine_warn.reset(new parsers::where::engine(warn, error_handler));
			if (!crit.empty()) engine_crit.reset(new parsers::where::engine(crit, error_handler));

			if (engine_warn) engine_warn->enabled_performance_collection();
			if (engine_crit) engine_crit->enabled_performance_collection();
			return true;
		}

		bool validate() {
			if (engine_filter && !engine_filter->validate(context)) {
				return false;
			}
			if (engine_warn && !engine_warn->validate(context)) {
				return false;
			}
			if (engine_warn) {
				BOOST_FOREACH(const boundries_type::value_type &v, engine_warn->fetch_performance_data()) {
					register_leaf_performance_data(v.second, false);
				}
			}
			if (engine_crit && !engine_crit->validate(context)) {
				return false;
			}
			if (engine_crit) {
				BOOST_FOREACH(const boundries_type::value_type &v, engine_crit->fetch_performance_data()) {
					register_leaf_performance_data(v.second, true);
				}
			}
			if (engine_ok && !engine_ok->validate(context)) {
				return false;
			}
			return true;
		}
		bool has_errors() const {
			if (error_handler)
				return error_handler->has_errors();
			return false;
		}

		void register_leaf_performance_data(const parsers::where::performance_node &node, const bool is_crit) {
			if (!context->has_variable(node.variable)) {
				error_handler->log_error("Failed to register for performance data");
				return;
			}
			typename leaf_performance_entry_type::iterator it = leaf_performance_data.find(node.variable);
			if (it != leaf_performance_data.end()) {
				if (is_crit)
					it->second.crit_value = node.value;
				else
					it->second.warn_value = node.value;
			} else {
				perf_entry entry;
				entry.current_value = context->create_variable(node.variable, false);
				entry.label = node.variable;
				if (is_crit)
					entry.crit_value = node.value;
				else
					entry.warn_value = node.value;
				leaf_performance_data[node.variable] = entry;
			}
		}
		void add_manual_perf(std::string key) {
			if (!context->has_variable(key)) {
				error_handler->log_error("Failed to register for performance data");
				return;
			}
			perf_entry entry;
			entry.current_value = context->create_variable(key, false);
			entry.label = key;
			leaf_performance_data[key] = entry;
		}

		void start_match() {
			summary.returnCode = NSCAPI::returnOK;
			has_matched = false;
			summary.reset();
		}
		boost::tuple<bool,bool> match(object_type record) {
			context->set_object(record);
			bool matched = false;
			bool done = false;
			// done should be set if we want to bail out after the first hit!
			// I.e. mode==first (mode==all)
			summary.count();
			if (!engine_filter || engine_filter->match(context)) {
				std::string current = renderer_detail.render(context);
				std::string perf_alias = renderer_perf.render(context);
				BOOST_FOREACH(const typename leaf_performance_entry_type::value_type &entry, leaf_performance_data) {
					parsers::where::perf_list_type perf = entry.second.current_value->get_performance_data(context, perf_alias, entry.second.warn_value, entry.second.crit_value, entry.second.minimum_value, entry.second.maximum_value);
					if (perf.size() > 0)
						performance_instance_data.insert(performance_instance_data.end(), perf.begin(), perf.end());
				}
				summary.matched(current);
				if (engine_crit && engine_crit->match(context)) {
					summary.matched_crit(current);
					nscapi::plugin_helper::escalteReturnCodeToCRIT(summary.returnCode);
					matched = true;
				} else if (engine_warn && engine_warn->match(context)) {
					summary.matched_warn(current);
					nscapi::plugin_helper::escalteReturnCodeToWARN(summary.returnCode);
					matched = true;
				} else if (engine_ok && engine_ok->match(context)) {
					// TODO: Unsure of this, should this not re-set matched?
					// What is matched for?
					summary.matched_ok(current);
					matched = true;
				} else if (error_handler && error_handler->is_debug()) {
					error_handler->log_debug("Crit/warn/ok did not match: " + current);
				}
				if (matched) {
					has_matched = true;
				}
			} else if (error_handler && error_handler->is_debug()) {
				error_handler->log_debug("Did not match: " + renderer_detail.render(context));
			}
			return boost::make_tuple(has_matched, done);
		}
		void end_match() {
			context->remove_object();
			BOOST_FOREACH(const typename leaf_performance_entry_type::value_type &entry, leaf_performance_data) {
				parsers::where::perf_list_type perf = entry.second.current_value->get_performance_data(context, "TODO", entry.second.warn_value, entry.second.crit_value, entry.second.minimum_value, entry.second.maximum_value);
				if (perf.size() > 0)
					performance_instance_data.insert(performance_instance_data.end(), perf.begin(), perf.end());
			}
		}
		void fetch_perf(parsers::where::perf_writer_interface* writer) {
			BOOST_FOREACH(const parsers::where::perf_list_type::value_type &entry, performance_instance_data) {
				writer->write(entry);
			}
		}
		std::string get_message() {
			return renderer_top.render(context);
		}
	};
}
