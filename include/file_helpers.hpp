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

#include <boost/version.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <utf8.hpp>

namespace file_helpers {
	class checks {
	public:
		static bool is_directory(std::string path) {
			return boost::filesystem::is_directory(path);
		}
		static bool is_file(std::string path) {
			return boost::filesystem::is_regular(path);
		}
	};

	class meta {
	public:
		static std::string get_filename(const boost::filesystem::path &path) {
#if BOOST_VERSION >= 104600
			return path.filename().string();
#else
			return path.filename(); 
#endif
		}
		static std::string get_path(std::string file) {
			boost::filesystem::path path(file);
			return path.parent_path().string();
		}
		static std::string get_filename(std::string file) {
			return get_filename(boost::filesystem::path(file));
		}
		static std::string get_extension(const boost::filesystem::path &path) {
#if BOOST_VERSION >= 104600
			return path.extension().string();
#else
			return path.extension(); 
#endif
		}
	};

	class patterns {
	public:
		typedef std::pair<boost::filesystem::path,boost::filesystem::path> pattern_type;

		static pattern_type split_pattern(boost::filesystem::path path) {
			if (boost::filesystem::is_directory(path))
				return pattern_type(path,boost::filesystem::path());
			return pattern_type(path.branch_path(), path.filename());
		}
		static pattern_type split_path_ex(boost::filesystem::path path) {
			if (boost::filesystem::is_directory(path)) {
				return pattern_type(path, boost::filesystem::path());
			}

			std::string spath = path.string();
			std::string::size_type pos = spath.find_last_of('\\');
			if (pos == std::string::npos) {
				pattern_type(spath, boost::filesystem::path("*.*"));
			}
			return pattern_type(spath.substr(0, pos), spath.substr(pos+1));
		}
		static boost::filesystem::path combine_pattern(pattern_type pattern) {
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
