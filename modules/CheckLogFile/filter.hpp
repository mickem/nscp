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

#include <map>
#include <string>

#include <parsers/where.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>

#include <error/error.hpp>

#include <parsers/where/node.hpp>
#include <parsers/where/engine.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace logfile_filter {
	struct filter_obj {
		std::string filename;
		std::string line;
		std::vector<std::string> chunks;
		typedef parsers::where::node_type node_type;
		filter_obj(std::string filename, std::string line, std::list<std::string> chunks) : filename(filename), line(line), chunks(chunks.begin(), chunks.end()) {}

		std::string get_column(std::size_t col) const {
			if (col >= 1 && col <= chunks.size())
				return chunks[col - 1];
			return "";
		}
		long long get_column_number(std::size_t col) const {
			if (col >= 1 && col <= chunks.size())
				return str::stox<long long>(chunks[col - 1]);
			return 0;
		}
		std::string get_filename() const {
			return filename;
		}
		std::string get_line() const {
			return line;
		}
		node_type get_column_fun(parsers::where::value_type target_type, parsers::where::evaluation_context context, const node_type subject);
		std::string to_string() const { return filename; }
	};

	typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
	struct filter_obj_handler : public native_context {
		filter_obj_handler();
	};

	typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}