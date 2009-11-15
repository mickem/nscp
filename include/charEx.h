/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#pragma once
#ifdef WIN32
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
		for (const wchar_t *p = buffer;*p!='\0';p++) {
			if (*p==split) {
				std::wstring str(start, p-start);
				ret.push_back(str);
				start = p+1;
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


	typedef std::pair<std::wstring,wchar_t*> token;
	inline token getToken(wchar_t *buffer, wchar_t split) {
		if (buffer == NULL)
			throw std::exception();
		wchar_t *p = wcschr(buffer, split);
		if (!p)
			return token(buffer, NULL);
		if (!p[1])
			return token(std::wstring(buffer, p-buffer), NULL);
		p++;
		return token(std::wstring(buffer, p-buffer-1), p);
	}
};