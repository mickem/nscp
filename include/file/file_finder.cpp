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

#include <file/file_finder.hpp>

namespace file {

	class finder {
		template <class finder_function>
		void recursive_scan(std::wstring dir, boost::wregex  pattern, int current_level, int max_level, finder_function & f, error_reporter * errors) {
			if ((max_level != -1) && (current_level > max_level))
				return;

			boost::filesystem::path path = dir;
			if (path.is_directory()) {
				// scan and parse all sub folders.
				boost::filesystem::wdirectory_iterator end_itr; // default construction yields past-the-end
				for ( boost::filesystem::wdirectory_iterator itr( path ); itr != end_itr; ++itr ) {
					recursive_scan<finder_function>(itr->path().leaf(), pattern, current_level+1, max_level, f, errors, debug);
					//if ( !is_directory(itr->status()) ) {}
				}
			} else if (!path.is_regular_file()) {
				// Match against pattern
				if (regex_match(path.string(), pattern)) {
					NSC_DEBUG_MSG_STD(_T("Matched: ") + path.string());
					f(file_finder_data(wfd, path.string(), errors));
				}
			} else {
				error->(...);
			}
			WIN32_FIND_DATA wfd;

			DWORD fileAttr = GetFileAttributes(dir.c_str());
			NSC_DEBUG_MSG_STD(_T("Input is: ") + dir + _T(" / ") + strEx::ihextos(fileAttr));

			if (!file_helpers::checks::is_directory(fileAttr)) {
				NSC_DEBUG_MSG_STD(_T("Found a file dont do recursive scan: ") + dir);
				// It is a file check it an return (dont check recursivly)
				pattern_type single_path = split_path_ex(dir);
				NSC_DEBUG_MSG_STD(_T("Path is: ") + single_path.first);
				HANDLE hFind = FindFirstFile(dir.c_str(), &wfd);
				if (hFind != INVALID_HANDLE_VALUE) {
					f(file_finder_data(wfd, single_path.first, errors));
					FindClose(hFind);
				}
				return;
			std::wstring file_pattern = dir + _T("\\") + pattern;
			NSC_DEBUG_MSG_STD(_T("File pattern: ") + file_pattern);
			HANDLE hFind = FindFirstFile(file_pattern.c_str(), &wfd);
			if (hFind != INVALID_HANDLE_VALUE) {
				do {
					if (!f(file_finder_data(wfd, dir, errors)))
						break;
				} while (FindNextFile(hFind, &wfd));
				FindClose(hFind);
			}
			std::wstring dir_pattern = dir + _T("\\*.*");
			NSC_DEBUG_MSG_STD(_T("File pattern: ") + dir_pattern);
			hFind = FindFirstFile(dir_pattern.c_str(), &wfd);
			if (hFind != INVALID_HANDLE_VALUE) {
				do {
					if (file_helpers::checks::is_directory(wfd.dwFileAttributes)) {
						if ( (wcscmp(wfd.cFileName, _T(".")) != 0) && (wcscmp(wfd.cFileName, _T("..")) != 0) )
							recursive_scan<finder_function>(dir + _T("\\") + wfd.cFileName, pattern, current_level+1, max_level, f, errors, debug);
					}
				} while (FindNextFile(hFind, &wfd));
				FindClose(hFind);
			}
		}

	};



}