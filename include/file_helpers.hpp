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

#include <boost/version.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <utf8.hpp>

namespace file_helpers {
	namespace fs = boost::filesystem;
	class checks {
	public:
		static bool is_directory(std::string path) {
			return fs::is_directory(path);
		}
		static bool is_file(std::string path) {
			return fs::is_regular(path);
		}
		static bool path_contains_file(fs::path dir, fs::path file) {
			if (dir.filename() == ".")
				dir.remove_filename();
			file.remove_filename();
			auto dir_len = std::distance(dir.begin(), dir.end());
			auto file_len = std::distance(file.begin(), file.end());
			if (dir_len > file_len)
				return false;
			return std::equal(dir.begin(), dir.end(), file.begin());
		}
		
	};

	class meta {
	public:
		static std::string get_filename(const fs::path &path) {
#if BOOST_VERSION >= 104600
			return path.filename().string();
#else
			return path.filename(); 
#endif
		}
		static std::string get_path(std::string file) {
			fs::path path(file);
			return path.parent_path().string();
		}
		static std::string get_filename(std::string file) {
			return get_filename(fs::path(file));
		}
		static std::string get_extension(const fs::path &path) {
#if BOOST_VERSION >= 104600
			return path.extension().string();
#else
			return path.extension(); 
#endif
		}
	};

	class patterns {
	public:
		typedef std::pair<fs::path,fs::path> pattern_type;

		static pattern_type split_pattern(fs::path path) {
			if (fs::is_directory(path))
				return pattern_type(path,fs::path());
			return pattern_type(path.branch_path(), path.filename());
		}
		static pattern_type split_path_ex(fs::path path) {
			if (fs::is_directory(path)) {
				return pattern_type(path, fs::path());
			}

			std::string spath = path.string();
			std::string::size_type pos = spath.find_last_of('\\');
			if (pos == std::string::npos) {
				pattern_type(spath, fs::path("*.*"));
			}
			return pattern_type(spath.substr(0, pos), spath.substr(pos+1));
		}
		static fs::path combine_pattern(pattern_type pattern) {
			return pattern.first / pattern.second;
		}

		static std::string glob_to_regexp(std::string mask) {
			boost::algorithm::replace_all(mask, ".", "\\.");
			boost::algorithm::replace_all(mask, "*", ".*");
			boost::algorithm::replace_all(mask, "?", ".");
			return mask;
		}
	}; // END patterns
}
