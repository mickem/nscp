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

#include "realtime_data.hpp"

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

#include <str/utils.hpp>

#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>


void runtime_data::touch(boost::posix_time::ptime now) {
	BOOST_FOREACH(file_container &fc, files) {
		if (boost::filesystem::exists(fc.file)) {
			fc.size = boost::filesystem::file_size(fc.file);
		} else {
			fc.size = 0;
		}
	}
}

bool runtime_data::has_changed(const file_container &fc) const {
	if (!boost::filesystem::exists(fc.file)) {
		NSC_TRACE_ENABLED() {
			NSC_TRACE_MSG("File was not found: " + fc.file.string());
		}
		return false;
	}
	if (check_time) {
		std::time_t time = boost::filesystem::last_write_time(fc.file);
		if (std::difftime(time, fc.time) != 0) {
			NSC_TRACE_ENABLED() {
				NSC_TRACE_MSG("File was changed (time): " + fc.file.string());
			}
			return true;
		}
	} else {
		boost::uintmax_t sz = boost::filesystem::file_size(fc.file);
		if (sz != fc.size) {
			NSC_TRACE_ENABLED() {
				NSC_TRACE_MSG("File was changed (size): " + fc.file.string());
			}
			return true;
		}
	}
	return false;
}


bool runtime_data::has_changed(transient_data_type) const {
	BOOST_FOREACH(const file_container &fc, files) {
		if (has_changed(fc)) {
			return true;
		}
	}
	return false;
}

void runtime_data::add_file(const boost::filesystem::path &path) {
	try {
		file_container fc;
		if (boost::filesystem::exists(path)) {
			fc.file = path;
			fc.size = boost::filesystem::file_size(fc.file);
		} else {
			fc.file = path;
			fc.size = 0;
		}
		files.push_back(fc);
	} catch (std::exception &e) {
		NSC_LOG_ERROR("Failed to add " + path.string() + ": " + utf8::utf8_from_native(e.what()));
	}
}

modern_filter::match_result runtime_data::process_item(filter_type &filter, transient_data_type) {
	modern_filter::match_result ret;
	BOOST_FOREACH(file_container &c, files) {
		boost::uintmax_t sz = boost::filesystem::file_size(c.file);
		if (sz == 0) {
			NSC_TRACE_ENABLED() {
				NSC_TRACE_MSG("File was zero, no point in reading it: " + c.file.string());
			}
			continue;
		}
		if (!has_changed(c)) {
			NSC_TRACE_ENABLED() {
				NSC_TRACE_MSG("File was unchanged, no point in reading it: " + c.file.string());
			}
			continue;
		}
		c.time = boost::filesystem::last_write_time(c.file);
		c.size = sz;

		std::ifstream file(c.file.string().c_str());
		if (file.is_open()) {
			std::string line;
			if (!read_from_start && sz > c.size)
				file.seekg(c.size);
			while (file.good()) {
				std::getline(file, line, '\n');
				if (!line.empty()) {
					std::list<std::string> chunks = str::utils::split_lst(line, utf8::cvt<std::string>(column_split));
					boost::shared_ptr<logfile_filter::filter_obj> record(new logfile_filter::filter_obj(c.file.string(), line, chunks));
					ret.append(filter.match(record));
				}
			}
			file.close();
		} else {
			NSC_LOG_ERROR("Failed to open file: " + c.file.string());
		}
	}
	return ret;
}

void runtime_data::set_read_from_start(bool read_from_start_) {
	read_from_start = read_from_start_;
	if (read_from_start) {
		check_time = true;
	}
}
void runtime_data::set_comparison(bool check_time_) {
	check_time = check_time_;
}

void runtime_data::set_split(std::string line, std::string column) {
	if (column.empty())
		column_split = "\t";
	else
		column_split = column;
	str::utils::replace(column_split, "\\t", "\t");
	str::utils::replace(column_split, "\\n", "\n");
	std::size_t len = column_split.size();
	if (len == 0)
		column_split = " ";
	if (len > 2 && column_split[0] == '\"' && column_split[len - 1] == '\"')
		column_split = column_split.substr(1, len - 2);


	if (line.empty())
		line = "\n";
	else
		line_split = line;
	str::utils::replace(line_split, "\\t", "\t");
	str::utils::replace(line_split, "\\n", "\n");
	len = line_split.size();
	if (len == 0)
		line_split = " ";
	if (len > 2 && line_split[0] == '\"' && line_split[len - 1] == '\"')
		line_split = line_split.substr(1, len - 2);
}