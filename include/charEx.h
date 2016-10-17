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
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#endif

namespace charEx {
	/**
	 * Function to split a char buffer into a list<string>
	 * @param buffer A char buffer to iterate over.
	 * @param split The char to split by
	 * @return a list with strings
	 */
	inline std::list<std::wstring> split(const wchar_t* buffer, wchar_t split) {
		std::list<std::wstring> ret;
		const wchar_t *start = buffer;
		for (const wchar_t *p = buffer; *p != '\0'; p++) {
			if (*p == split) {
				std::wstring str(start, p - start);
				ret.push_back(str);
				start = p + 1;
			}
		}
		ret.push_back(std::wstring(start));
		return ret;
	}

	/*
		inline char* tchar_to_char( const wchar_t* pStr, int len, int &nChars) {
			if (pStr == NULL)
				throw std::exception();
			if (len < -1)
				throw std::exception();

			// figure out how many narrow characters we are going to get
			nChars = WideCharToMultiByte(CP_ACP, 0, pStr, len, NULL, 0, NULL, NULL);
			if (len == -1)
				--nChars;
			if (nChars==0) {
				char *ret = new char[1];
				ret[0] = 0;
				return ret;
			}

			// convert the wide string to a narrow string
			char *ret = new char[nChars+1];
			WideCharToMultiByte(CP_ACP, 0, pStr, len, ret, nChars, NULL, NULL);
			return ret;
		}

		inline wchar_t* char_to_tchar(const char* pStr, int len, int &nChars) {
			if (pStr == NULL)
				throw std::exception();
			if (len < -1)
				throw std::exception();

			// figure out how many wide characters we are going to get
			nChars = MultiByteToWideChar( CP_ACP , 0 , pStr , len , NULL , 0 );
			if (len == -1)
				--nChars;
			if (nChars == 0) {
				wchar_t *ret = new wchar_t[1];
				ret[0] = 0;
				return ret;
			}

			// convert the narrow string to a wide string
			wchar_t *ret = new wchar_t[nChars+1];
			MultiByteToWideChar(CP_ACP, 0 ,pStr ,len, const_cast<wchar_t*>(ret), nChars);
			return ret;
		}
	*/

	typedef std::pair<std::wstring, wchar_t*> token;
	inline token getToken(wchar_t *buffer, wchar_t split) {
		if (buffer == NULL)
			throw std::exception();
		wchar_t *p = wcschr(buffer, split);
		if (!p)
			return token(buffer, NULL);
		if (!p[1])
			return token(std::wstring(buffer, p - buffer), NULL);
		p++;
		return token(std::wstring(buffer, p - buffer - 1), p);
	}
}
