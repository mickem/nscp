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