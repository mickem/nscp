/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
			void debug(std::string msg) {
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