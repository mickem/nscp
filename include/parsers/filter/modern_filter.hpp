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

#pragma once

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <boost/foreach.hpp>
#include <boost/unordered_set.hpp>

#include <parsers/expression/expression.hpp>
#include <parsers/perfconfig/perfconfig.hpp>
#include <parsers/where/engine_impl.hpp>

#include <NSCAPI.h>
#include <str/utils.hpp>
#include <str/xtos.hpp>
#include <nscapi/nscapi_helper.hpp>

#ifdef WIN32
#pragma warning(push)
#pragma warning(disable:4456)
#endif

namespace parsers {
	namespace where {
		template<class Tobject>
		struct generic_summary;
	}
}

struct perf_writer_interface {
	virtual void write(const parsers::where::performance_data &value) = 0;
};

namespace modern_filter {
	template<class Tfactory>
	struct filter_text_renderer {
		typedef boost::shared_ptr<parsers::where::error_handler_interface> error_handler;
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
		bool parse(boost::shared_ptr<Tfactory> context, const std::string str, error_handler error) {
			if (str.empty() || str == "none")
				return true;
			parsers::simple_expression::result_type keys;
			if (error->is_debug()) {
				error->log_debug("Parsing: " + str);
			}
			parsers::simple_expression expr;
			if (!expr.parse(str, keys)) {
				error->log_error("Failed to parse: " + str);
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
						error->log_error("Invalid variable: " + e.name);
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
				else if (e.node->is_int())
					ret += str::xtos_non_sci(e.node->get_int_value(context));
				else if (e.node->is_float())
					ret += str::xtos(e.node->get_float_value(context));
				else
					ret += e.node->get_string_value(context);
			}
			return ret;
		}
	};

	template<class Tfactory>
	struct filter_hash_renderer {
		struct my_entry {
			std::string key;
			parsers::where::node_type node;
			my_entry(const std::string &key, parsers::where::node_type node) : key(key), node(node) {}
			my_entry(const my_entry &other) : key(other.key), node(other.node) {}
			const my_entry& operator= (const my_entry &other) {
				key = other.key;
				node = other.node;
				return *this;
			}
		};

		typedef std::list<my_entry> entry_list;
		entry_list entries;
		filter_hash_renderer() {}

		bool empty() const {
			return entries.empty();
		}
		bool parse(boost::shared_ptr<Tfactory> context) {
			BOOST_FOREACH(const std::string &e, context->get_variables()) {
				my_entry my_e(e, context->create_variable(e, true));
				entries.push_back(my_e);
			}
			return true;
		}
		std::map<std::string,std::string> render(boost::shared_ptr<Tfactory> context) const {
			std::map<std::string, std::string> ret;
			BOOST_FOREACH(const my_entry &e, entries) {
				if (e.node->is_int())
					ret[e.key] = str::xtos_non_sci(e.node->get_int_value(context));
				else if (e.node->is_float())
					ret[e.key] = str::xtos(e.node->get_float_value(context));
				else
					ret[e.key] = e.node->get_string_value(context);
			}
			return ret;
		}
	};


	template<class Tfactory>
	struct perf_config_parser {
		typedef std::map<std::string, std::string> values_type;
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
		typedef boost::shared_ptr<parsers::where::error_handler_interface> error_handler;
		entry_list entries;
		std::list<std::string> extra_perf;
		perf_config_parser() {}

		bool parse(boost::shared_ptr<Tfactory> context, const std::string str, error_handler error) {
			parsers::perfconfig::result_type keys;
			parsers::perfconfig parser;
			if (!parser.parse(str, keys)) {
				error->log_error("Failed to parse: " + str);
				return false;
			}
			BOOST_FOREACH(const parsers::perfconfig::perf_rule &r, keys) {
				if (r.name == "extra") {
					BOOST_FOREACH(const parsers::perfconfig::perf_option &o, r.options) {
						extra_perf.push_back(o.key);
					}
				} else {
					std::map<std::string, std::string> options;
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
		bool is_done_;


		match_result()
			: matched_filter(false)
			, matched_bound(false)
			, is_done_(false)
		{}

		match_result(bool matched_filter, bool matched_bound)
			: matched_filter(matched_filter)
			, matched_bound(matched_bound)
			, is_done_(false) {}

		match_result(const match_result &other)
			: matched_filter(other.matched_filter)
			, matched_bound(other.matched_bound)
			, is_done_(other.is_done_) {}

		const match_result& operator=(const match_result &other) {
			matched_filter = other.matched_filter;
			matched_bound = other.matched_bound;
			is_done_ = other.is_done_;
			return *this;
		}

		void append(const match_result &other) {
			matched_filter |= other.matched_filter;
			matched_bound |= other.matched_bound;
			is_done_ |= other.is_done_;
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
		filter_hash_renderer<Tfactory> renderer_hash;
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
		typedef std::map<std::string, std::string> hash_type;
		typedef std::list<hash_type> hash_list_type;
		hash_list_type records_;
		boost::shared_ptr<Tfactory> context;
		bool fetch_hash_;
		bool has_unique_index;
		error_type error_handler_;

		struct perf_entry {
			std::string label;
			parsers::where::node_type current_value;
			parsers::where::node_type crit_value;
			parsers::where::node_type warn_value;
			parsers::where::node_type maximum_value;
			parsers::where::node_type minimum_value;
		};

		parsers::where::perf_list_type performance_instance_data;

		typedef std::map<std::string, perf_entry> leaf_performance_entry_type;
		leaf_performance_entry_type leaf_performance_data;

		modern_filters() : context(new Tfactory()), fetch_hash_(false), has_unique_index(false) {
			context->set_summary(&summary);
		}

		std::map<std::string, std::string> get_filter_syntax() const {
			std::map<std::string, std::string> ret;
			std::map<std::string, std::string> m1 = summary.get_filter_syntax();
			std::map<std::string, std::string> m2 = context->get_filter_syntax();
			ret.insert(m1.begin(), m1.end());
			ret.insert(m2.begin(), m2.end());
			return ret;
		}
		bool build_index(const std::string &unqie, std::string &gerror) {
			std::string lerror;
			if (!renderer_unqiue.parse(context, unqie, error_handler_)) {
				gerror = "Invalid unique-syntax: " + lerror;
				return false;
			}
			has_unique_index = true;
			return true;
		}
		bool build_syntax(const bool debug, const std::string &top, const std::string &detail, const std::string &perf, const std::string &perf_config_data, const std::string &ok_syntax, const std::string &empty_syntax, std::string &gerror) {
			if (debug)
				set_debug(true);
			if (!renderer_top.parse(context, top, get_error_handler(debug))) {
				return false;
			}
			if (!renderer_detail.parse(context, detail, get_error_handler(debug))) {
				return false;
			}
			if (!renderer_perf.parse(context, perf, get_error_handler(debug))) {
				return false;
			}
			if (!perf_config.parse(context, perf_config_data, get_error_handler(debug))) {
				return false;
			}
			if (!renderer_ok.parse(context, ok_syntax, get_error_handler(debug))) {
				return false;
			}
			if (!renderer_empty.parse(context, empty_syntax, get_error_handler(debug))) {
				return false;
			}
			renderer_hash.parse(context);
			return true;
		}
		bool build_engines(const bool debug, const std::string &filter, const std::string &ok, const std::string &warn, const std::string &crit) {
			std::vector<std::string> filter_; 
			if (!filter.empty())
				filter_.push_back(filter);
			std::vector<std::string> ok_; 
			if (!ok.empty())
				ok_.push_back(ok);
			std::vector<std::string> warn_; 
			if (!warn.empty())
				warn_.push_back(warn);
			std::vector<std::string> crit_;
			if (!crit.empty())
				crit_.push_back(crit);
			return build_engines(debug, filter_, ok_, warn_, crit_);
		}

		error_type get_error_handler(bool debug) {
			if (!error_handler_) {
				error_handler_.reset(new error_handler_impl(debug));
			}
			if (debug)
				set_debug(true);
			return error_handler_;
		}

		bool build_engines(const bool debug, const std::vector<std::string> &filter, const std::vector<std::string> &ok, const std::vector<std::string> &warn, const std::vector<std::string> &crit) {

			if (!filter.empty()) engine_filter.reset(new parsers::where::engine(filter, get_error_handler(debug)));
			if (!ok.empty()) engine_ok.reset(new parsers::where::engine(ok, get_error_handler(debug)));
			if (!warn.empty()) engine_warn.reset(new parsers::where::engine(warn, get_error_handler(debug)));
			if (!crit.empty()) engine_crit.reset(new parsers::where::engine(crit, get_error_handler(debug)));

			if (engine_warn) engine_warn->enabled_performance_collection();
			if (engine_crit) engine_crit->enabled_performance_collection();
			return true;
		}

		bool validate(std::string &error) {
			if (engine_filter && !engine_filter->validate(context)) {
				error = "Filter expression is not valid: " + engine_filter->to_string();
				return false;
			}
			if (engine_warn && !engine_warn->validate(context)) {
				error = "Warning expression is not valid: " + engine_warn->to_string();
				return false;
			}
			if (engine_warn) {
				BOOST_FOREACH(const boundries_type::value_type &v, engine_warn->fetch_performance_data()) {
					register_leaf_performance_data(v.second, false);
				}
			}
			if (engine_crit && !engine_crit->validate(context)) {
				error = "Critical expression is not valid:" + engine_crit->to_string();
				return false;
			}
			if (engine_crit) {
				BOOST_FOREACH(const boundries_type::value_type &v, engine_crit->fetch_performance_data()) {
					register_leaf_performance_data(v.second, true);
				}
			}
			if (engine_ok && !engine_ok->validate(context)) {
				error = "Ok expression is not valid: " + engine_ok->to_string();
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
			if (error_handler_)
				return error_handler_->has_errors();
			return false;
		}
		std::string get_errors() const {
			if (error_handler_)
				return error_handler_->get_errors();
			return "unknown";
		}
		bool should_log_debug() {
			return error_handler_ && error_handler_->is_debug();
		}
		void log_debug(const std::string &str) {
			if (error_handler_) {
				error_handler_->log_debug(str);
			}
		}

		void register_leaf_performance_data(const parsers::where::performance_node &node, const bool is_crit) {
			if (!context->has_variable(node.variable)) {
				error_handler_->log_error("Failed to register for performance data");
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
				error_handler_->log_error("Failed to register for performance data");
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
		void fetch_hash(bool fetch_hash) {
			fetch_hash_ = fetch_hash;
		}
		void start_match() {
			summary.returnCode = NSCAPI::query_return_codes::returnOK;
			has_matched = false;
			summary.reset();
			records_.clear();
		}
		match_result match(object_type record) {
			context->set_object(record);
			bool matched_filter = false;
			bool matched_bound = false;
			// done should be set if we want to bail out after the first hit!
			// I.e. mode==first (mode==all)
			summary.count();
			if (!engine_filter || engine_filter->match(context, true)) {
				matched_filter = true;
				if (fetch_hash_) {
					records_.push_back(renderer_hash.render(context));
				}
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
				if (engine_crit && engine_crit->match(context, true)) {
					if (should_log_debug()) log_debug("Crit match: " + current);
					if (second_unique_match)
						summary.matched_crit_unique();
					else
						summary.matched_crit(current);
					nscapi::plugin_helper::escalteReturnCodeToCRIT(summary.returnCode);
					matched_bound = true;
				} else if (engine_warn && engine_warn->match(context, true)) {
					if (should_log_debug()) log_debug("Warn match: " + current);
					if (second_unique_match)
						summary.matched_warn_unique();
					else
						summary.matched_warn(current);
					nscapi::plugin_helper::escalteReturnCodeToWARN(summary.returnCode);
					matched_bound = true;
				} else if (engine_ok && engine_ok->match(context, true)) {
					if (should_log_debug()) log_debug("Ok match: " + current);
					// TODO: Unsure of this, should this not re-set matched?
					// What is matched for?
					if (second_unique_match)
						summary.matched_ok_unique();
					else
						summary.matched_ok(current);
					matched_bound = true;
				} else {
					if (should_log_debug()) log_debug("Crit/warn/ok did not match: " + current);
					if (second_unique_match)
						summary.matched_ok_unique();
					else
						summary.matched_ok(current);
				}
				if (matched_bound) {
					has_matched = true;
				}
			} else if (should_log_debug()) {
				log_debug("Filter did not match: " + renderer_detail.render(context));
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
			if (engine_crit && engine_crit->match(context, false)) {
				nscapi::plugin_helper::escalteReturnCodeToCRIT(summary.returnCode);
				summary.move_hits_crit();
				matched = true;
			} else if (engine_warn && engine_warn->match(context, false)) {
				nscapi::plugin_helper::escalteReturnCodeToWARN(summary.returnCode);
				summary.move_hits_warn();
				matched = true;
			} else if (engine_ok && engine_ok->match(context, false)) {
				// TODO: Unsure of this, should this not re-set matched?
				// What is matched for?
				matched = true;
			} else if (should_log_debug()) {
				log_debug("Crit/warn/ok did not match: <END>");
				if (context->has_debug()) {
					log_debug(context->get_debug());
					context->clear();
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
#ifdef WIN32
#pragma warning(pop)
#endif
