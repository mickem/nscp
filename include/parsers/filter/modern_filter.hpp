#pragma once

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <boost/foreach.hpp>
#include <boost/unordered_set.hpp>

#include <parsers/expression/expression.hpp>
#include <parsers/perfconfig/perfconfig.hpp>
#include <parsers/where/engine_impl.hpp>

#include <NSCAPI.h>
#include <nscapi/nscapi_helper.hpp>

namespace parsers {
	namespace where {
		template<class Tobject>
		struct generic_summary;
	}
}


	struct perf_writer_interface {
		//virtual void write(::Plugin::QueryResponseMessage::Response::Line *line, const parsers::where::performance_data &data) = 0;
		virtual void write(const parsers::where::performance_data &value) = 0;
	};




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

		bool empty() const {
			return entries.empty();
		}
		bool parse(boost::shared_ptr<Tfactory> context, const std::string str, std::string &error) {
			if (str.empty())
				return true;
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
		std::string render(boost::shared_ptr<Tfactory> context) const {
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

	template<class Tfactory>
	struct perf_config_parser {
		typedef std::map<std::string,std::string> values_type;
		struct config_entry {
			std::string key;
			values_type values;
			config_entry() {}
			config_entry(const std::string &key, const std::vector<parsers::perfconfig::perf_option> &ops) : key(key) {
				BOOST_FOREACH(const parsers::perfconfig::perf_option &op, ops) {
					values[op.key] = op.value;
				}
			}
		};

		typedef std::list<config_entry> entry_list;
		entry_list entries;
		std::list<std::string> extra_perf;
		perf_config_parser() {}

		bool parse(boost::shared_ptr<Tfactory> context, const std::string str, std::string &error) {
			parsers::perfconfig::result_type keys;
			parsers::perfconfig parser;
			if (!parser.parse(str, keys)) {
				error = "Failed to parse: " + str;
				return false;
			}
			BOOST_FOREACH(const parsers::perfconfig::perf_rule &r, keys) {
				if (r.name == "extra") {
					BOOST_FOREACH(const parsers::perfconfig::perf_option &o, r.options) {
						extra_perf.push_back(o.key);
					}
				} else {
					std::map<std::string,std::string> options;
					BOOST_FOREACH(const parsers::perfconfig::perf_option &o, r.options) {
						options[o.key] = o.value;
					}
					context->add_perf_config(r.name, options);
				}
			}
			return true;
		}
		boost::optional<values_type> lookup(std::string key) {
			BOOST_FOREACH(const config_entry &e, entries) {
				if (e.key == key)
					return e.values;
			}
			return boost::optional<values_type>();
		}
		std::list<std::string> get_extra_perf() const {
			return extra_perf;
		}
	};

	struct match_result {
		bool matched_filter;
		bool matched_bound;
		bool is_done;

		match_result(bool matched_filter, bool matched_bound) 
			: matched_filter(matched_filter)
			, matched_bound(matched_bound)
			, is_done(false)
		{
		}
	};

	class error_handler_impl : public parsers::where::error_handler_interface {
		std::string error;
		bool debug_;
		error_handler_impl() {}
	public:
		error_handler_impl(bool debug) : debug_(debug) {}
		void log_error(const std::string error);
		void log_warning(const std::string error);
		void log_debug(const std::string error);
		bool is_debug() const;
		void set_debug(bool debug_);
		bool has_errors() const;
		std::string get_errors() const;
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
		filter_text_renderer<Tfactory> renderer_unqiue;
		filter_text_renderer<Tfactory> renderer_ok;
		filter_text_renderer<Tfactory> renderer_empty;
		filter_engine engine_filter;
		filter_engine engine_warn;
		filter_engine engine_crit;
		filter_engine engine_ok;
		perf_config_parser<Tfactory> perf_config;
		parsers::where::generic_summary<object_type> summary;
		boost::unordered_set<std::string> unique_index;
		bool has_matched;
		boost::shared_ptr<Tfactory> context;
		bool has_unique_index;
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


		modern_filters() : context(new Tfactory()), has_unique_index(false) {
			context->set_summary(&summary);
		}

		std::string get_filter_syntax() const {
			return context->get_filter_syntax() + summary.get_filter_syntax();
		}
		std::string get_format_syntax() const {
			return context->get_format_syntax() + summary.get_format_syntax();
		}
		bool build_index(const std::string &unqie, std::string &gerror) {
			std::string lerror;
			if (!renderer_unqiue.parse(context, unqie, lerror)) {
				gerror = "Invalid unique-syntax: " + lerror;
				return false;
			}
			has_unique_index = true;
			return true;
		}
		bool build_syntax(const std::string &top, const std::string &detail, const std::string &perf, const std::string &perf_config_data, const std::string &ok_syntax, const std::string &empty_syntax, std::string &gerror) {
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
			if (!perf_config.parse(context, perf_config_data, lerror)) {
				gerror = "Invalid syntax: " + lerror;
				return false;
			}
			if (!renderer_ok.parse(context, ok_syntax, lerror)) {
				gerror = "Invalid syntax: " + lerror;
				return false;
			}
			if (!renderer_empty.parse(context, empty_syntax, lerror)) {
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
			if (debug)
				set_debug(true);

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
			BOOST_FOREACH(const std::string &p, perf_config.get_extra_perf()) {
				add_manual_perf(p);
			}
			return true;
		}
		void set_debug(bool debug) {
			context->enable_debug(debug);
		}
		bool has_errors() const {
			if (error_handler)
				return error_handler->has_errors();
			return false;
		}
		std::string get_errors() const {
			if (error_handler)
				return error_handler->get_errors();
			return "unknown";
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

		bool has_filter() const {
			return engine_filter;
		}
		void start_match() {
			summary.returnCode = NSCAPI::query_return_codes::returnOK;
			has_matched = false;
			summary.reset();
		}
		match_result match(object_type record) {
			context->set_object(record);
			bool matched_filter = false;
			bool matched_bound = false;
			// done should be set if we want to bail out after the first hit!
			// I.e. mode==first (mode==all)
			summary.count();
			if (!engine_filter || engine_filter->match(context, false)) {
				matched_filter = true;
				std::string current = renderer_detail.render(context);
				std::string perf_alias = renderer_perf.render(context);
				bool second_unique_match = false;
				if (has_unique_index) {
					std::string tmp = renderer_unqiue.render(context);
					second_unique_match = unique_index.find(tmp) != unique_index.end();
					if (!second_unique_match)
						unique_index.emplace(tmp);
				}

				BOOST_FOREACH(const typename leaf_performance_entry_type::value_type &entry, leaf_performance_data) {
					parsers::where::perf_list_type perf = entry.second.current_value->get_performance_data(context, perf_alias, entry.second.warn_value, entry.second.crit_value, entry.second.minimum_value, entry.second.maximum_value);
					if (perf.size() > 0)
						performance_instance_data.insert(performance_instance_data.end(), perf.begin(), perf.end());
				}
				if (second_unique_match)
					summary.matched_unique();
				else
					summary.matched(current);
				if (engine_crit && engine_crit->match(context, false)) {
					if (error_handler && error_handler->is_debug())
						error_handler->log_debug("Crit match: " + current);
					if (second_unique_match)
						summary.matched_crit_unique();
					else
						summary.matched_crit(current);
					nscapi::plugin_helper::escalteReturnCodeToCRIT(summary.returnCode);
					matched_bound = true;
				} else if (engine_warn && engine_warn->match(context, false)) {
					if (error_handler && error_handler->is_debug())
						error_handler->log_debug("Warn match: " + current);
					if (second_unique_match)
						summary.matched_warn_unique();
					else
						summary.matched_warn(current);
					nscapi::plugin_helper::escalteReturnCodeToWARN(summary.returnCode);
					matched_bound = true;
				} else if (engine_ok && engine_ok->match(context, false)) {
					if (error_handler && error_handler->is_debug())
						error_handler->log_debug("Ok match: " + current);
					// TODO: Unsure of this, should this not re-set matched?
					// What is matched for?
					if (second_unique_match)
						summary.matched_ok_unique();
					else
						summary.matched_ok(current);
					matched_bound = true;
				} else {
					if (error_handler && error_handler->is_debug())
						error_handler->log_debug("Crit/warn/ok did not match: " + current);
					if (second_unique_match)
						summary.matched_ok_unique();
					else
						summary.matched_ok(current);
				}
				if (matched_bound) {
					has_matched = true;
				}
			} else if (error_handler && error_handler->is_debug()) {
				error_handler->log_debug("Filter did not match: " + renderer_detail.render(context));
			}
			if (error_handler && error_handler->is_debug()) {
				if (context->has_debug()) {
					error_handler->log_debug(context->get_debug());
					context->clear_debug();
				}
			}
			return match_result(matched_filter, matched_bound);
		}


		bool match_post() {
			context->remove_object();
			bool matched = summary.has_matched();
			BOOST_FOREACH(const typename leaf_performance_entry_type::value_type &entry, leaf_performance_data) {
				parsers::where::perf_list_type perf = entry.second.current_value->get_performance_data(context, "TODO", entry.second.warn_value, entry.second.crit_value, entry.second.minimum_value, entry.second.maximum_value);
				if (perf.size() > 0)
					performance_instance_data.insert(performance_instance_data.end(), perf.begin(), perf.end());
			}
			if (engine_crit && !engine_crit->require_object(context) && engine_crit->match(context, true)) {
				nscapi::plugin_helper::escalteReturnCodeToCRIT(summary.returnCode);
				matched = true;
			} else if (engine_warn && !engine_warn->require_object(context) && engine_warn->match(context, true)) {
				nscapi::plugin_helper::escalteReturnCodeToWARN(summary.returnCode);
				matched = true;
			} else if (engine_ok && !engine_ok->require_object(context) && engine_ok->match(context, true)) {
				// TODO: Unsure of this, should this not re-set matched?
				// What is matched for?
				matched = true;
			} else if (error_handler && error_handler->is_debug()) {
				error_handler->log_debug("Crit/warn/ok did not match: <END>");
				if (context->has_debug()) {
					error_handler->log_debug(context->get_debug());
					context->clear_debug();
				}
			}
			return matched;
		}

		void end_match() {
			context->remove_object();
			BOOST_FOREACH(const typename leaf_performance_entry_type::value_type &entry, leaf_performance_data) {
				parsers::where::perf_list_type perf = entry.second.current_value->get_performance_data(context, "TODO", entry.second.warn_value, entry.second.crit_value, entry.second.minimum_value, entry.second.maximum_value);
				if (perf.size() > 0)
					performance_instance_data.insert(performance_instance_data.end(), perf.begin(), perf.end());
			}
		}
		void fetch_perf(perf_writer_interface* writer) {
			BOOST_FOREACH(const parsers::where::perf_list_type::value_type &entry, performance_instance_data) {
				writer->write(entry);
			}
		}
		std::string get_message() {
			if (!summary.has_matched() && !renderer_empty.empty())
				return renderer_empty.render(context);
			if (summary.returnCode == NSCAPI::query_return_codes::returnOK && !renderer_ok.empty())
				return renderer_ok.render(context);
			return renderer_top.render(context);
		}
	};
}
