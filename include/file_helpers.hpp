#pragma once

#include <boost/filesystem.hpp>

namespace file_helpers {
	class checks {
	public:
		static bool is_directory(std::wstring path) {
			return boost::filesystem::is_directory(path);
		}
// 		static bool is_directory(DWORD dwAtt) {
// 			if (dwAtt == INVALID_FILE_ATTRIBUTES) {
// 				return false;
// 			} else if ((dwAtt&FILE_ATTRIBUTE_DIRECTORY)==FILE_ATTRIBUTE_DIRECTORY) {
// 				return true;
// 			}
// 			return false;
// 		}
		static bool is_file(std::wstring path) {
			return boost::filesystem::is_regular(path);
// 			DWORD dwAtt = ::GetFileAttributes(path.c_str());
// 			if (dwAtt == INVALID_FILE_ATTRIBUTES) {
// 				return false;
// 			} else if ((dwAtt&FILE_ATTRIBUTE_NORMAL)==FILE_ATTRIBUTE_NORMAL) {
// 				return true;
// 			}
// 			return false;
		}
		static bool exists(std::wstring path) {
			return boost::filesystem::exists(path);
// 			DWORD dwAtt = ::GetFileAttributes(path.c_str());
// 			if (dwAtt == INVALID_FILE_ATTRIBUTES) {
// 				return false;
// 			}
// 			return true;
		}
	};

	class meta {
	public:
		static boost::filesystem::wpath get_path(boost::filesystem::wpath path) {
			return path.branch_path();
		}
		static std::wstring get_filename(boost::filesystem::wpath path) {
			return path.leaf();
		}
		static std::wstring get_path(std::wstring file) {
			boost::filesystem::wpath path = file;
			return path.branch_path().string();
		}
		static std::wstring get_filename(std::wstring file) {
			boost::filesystem::wpath path = file;
			return path.leaf();
		}
	};

	class patterns {
	public:
		typedef std::pair<boost::filesystem::wpath,std::wstring> pattern_type;

		static pattern_type split_pattern(boost::filesystem::wpath path) {
			if (boost::filesystem::is_directory(path))
				return pattern_type(path, _T(""));
			return pattern_type(path.branch_path(), path.leaf() /*filename()*/);
		}
		static boost::filesystem::wpath combine_pattern(pattern_type pattern) {
			return pattern.first / pattern.second;
		}


	};

}
