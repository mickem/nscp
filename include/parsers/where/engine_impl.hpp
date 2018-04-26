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
#include <boost/foreach.hpp>

#include <parsers/where/node.hpp>
#include <parsers/where/engine.hpp>

namespace parsers {
	namespace where {
		template<class TObject>
		struct generic_summary;

		template<class TObject>
		struct evaluation_context_impl : public parsers::where::object_factory_interface{
			typedef TObject object_type;
			typedef generic_summary<TObject>* summary_type;
			typedef std::list<std::string> errors_type;
			bool enable_debug_;

			evaluation_context_impl() : enable_debug_(false) {}
			boost::optional<TObject> object;
			boost::optional<summary_type> summary;
			errors_type errors_;
			errors_type warnings_;
			errors_type debugs_;
			TObject get_object() {
				return *object;
			}
			void enable_debug(bool enable_debug) {
				enable_debug_ = enable_debug;
			}
			void debug(const std::string msg) {
				if (enable_debug_)
					debugs_.push_back(msg);
			}
			summary_type get_summary() {
				return *summary;
			}
			bool has_object() {
				return static_cast<bool>(object);
			}
			bool has_summary() {
				return static_cast<bool>(summary);
			}
			void remove_object() {
				object.reset();
			}
			void remove_summary() {
				summary.reset();
			}
			void set_object(TObject  o) {
				object = o;
			}
			void set_summary(summary_type o) {
				summary = o;
			}

			virtual bool has_error() const {
				return !errors_.empty();
			}
			virtual bool has_warn() const {
				return !warnings_.empty();
			}
			virtual bool has_debug() const {
				return !debugs_.empty();
			}
			virtual std::string get_error() const {
				std::string ret;
				BOOST_FOREACH(const std::string &m, errors_) {
					if (!ret.empty())
						ret += ", ";
					ret += m;
				}
				return ret;
			}
			virtual std::string get_warn() const {
				std::string ret;
				BOOST_FOREACH(const std::string &m, warnings_) {
					if (!ret.empty())
						ret += ", ";
					ret += m;
				}
				return ret;
			}
			virtual std::string get_debug() const {
				std::string ret;
				BOOST_FOREACH(const std::string &m, debugs_) {
					if (!ret.empty())
						ret += ", ";
					ret += m;
				}
				return ret;
			}
			virtual void clear() {
				errors_.clear();
				warnings_.clear();
				debugs_.clear();
			}
			virtual void error(const std::string msg) {
				errors_.push_back(msg);
			}
			virtual void warn(const std::string msg) {
				warnings_.push_back(msg);
			}
		};
	}
}