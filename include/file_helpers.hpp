#pragma once

#include <boost/version.hpp>
#include <boost/filesystem.hpp>
#include <unicode_char.hpp>
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
	}; // END patterns
}
