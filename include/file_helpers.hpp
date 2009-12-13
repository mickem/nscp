#pragma once

namespace file_helpers {
	class checks {
	public:
		static bool is_directory(std::wstring path) {
			DWORD dwAtt = ::GetFileAttributes(path.c_str());
			if (dwAtt == INVALID_FILE_ATTRIBUTES) {
				return false;
			} else if ((dwAtt&FILE_ATTRIBUTE_DIRECTORY)==FILE_ATTRIBUTE_DIRECTORY) {
				return true;
			}
			return false;
		}
		static bool is_file(std::wstring path) {
			DWORD dwAtt = ::GetFileAttributes(path.c_str());
			if (dwAtt == INVALID_FILE_ATTRIBUTES) {
				return false;
			} else if ((dwAtt&FILE_ATTRIBUTE_NORMAL)==FILE_ATTRIBUTE_NORMAL) {
				return true;
			}
			return false;
		}
		static bool exists(std::wstring path) {
			DWORD dwAtt = ::GetFileAttributes(path.c_str());
			if (dwAtt == INVALID_FILE_ATTRIBUTES) {
				return false;
			}
			return true;
		}
	};
	class meta {
	public:
		static std::wstring get_path(std::wstring file) {
			std::wstring::size_type pos = file.find_last_of('\\');
			if (pos == std::wstring::npos) {
				return file;
			}
			return file.substr(0, pos);
		}
		static std::wstring get_filename(std::wstring file) {
			std::wstring::size_type pos = file.find_last_of('\\');
			if (pos == std::wstring::npos || ++pos == std::wstring::npos) {
				return _T("");
			}
			return file.substr(pos);
		}
	};

	class patterns {
	public:
		typedef std::pair<std::wstring,std::wstring> pattern_type;

		static pattern_type split_pattern(std::wstring path) {
			std::wstring baseDir;
			if (file_helpers::checks::exists(path)) {
				return pattern_type(path, _T(""));
			}
			std::wstring::size_type pos = path.find_last_of('\\');
			if (pos == std::wstring::npos) {
				pattern_type(path, _T("*.*"));
			}
			return pattern_type(path.substr(0, pos), path.substr(pos+1));
		}
		static std::wstring combine_pattern(pattern_type pattern) {
			return pattern.first + _T("\\") + pattern.second;
		}


	};

}
