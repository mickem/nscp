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
#include <windows.h>
#include <tchar.h>

namespace charEx {
	/**
	 * Function to split a char buffer into a list<string>
	 * @param buffer A char buffer to iterate over.
	 * @param split The char to split by
	 * @return a list with strings
	 */
	inline std::list<std::wstring> split(const TCHAR* buffer, TCHAR split) {
		std::list<std::wstring> ret;
		const TCHAR *start = buffer;
		for (const TCHAR *p = buffer;*p!='\0';p++) {
			if (*p==split) {
				std::wstring str(start, p-start);
				ret.push_back(str);
				start = p+1;
			}
		}
		ret.push_back(std::wstring(start));
		return ret;
	}


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
			TCHAR *ret = new TCHAR[1];
			ret[0] = 0;
			return ret;
		}

		// convert the narrow string to a wide string 
		TCHAR *ret = new TCHAR[nChars+1];
		MultiByteToWideChar(CP_ACP, 0 ,pStr ,len, const_cast<wchar_t*>(ret), nChars);
		return ret;
	}



	typedef std::pair<std::wstring,TCHAR*> token;
	inline token getToken(TCHAR *buffer, TCHAR split) {
		if (buffer == NULL)
			throw std::exception();
		TCHAR *p = wcschr(buffer, split);
		if (!p)
			return token(buffer, NULL);
		if (!p[1])
			return token(std::wstring(buffer, p-buffer), NULL);
		p++;
		return token(std::wstring(buffer, p-buffer-1), p);
	}
#ifdef _DEBUG
	inline void test_getToken(TCHAR* in1, TCHAR in2, std::wstring out1, TCHAR * out2) {
		token t = getToken(in1, in2);
		std::wcout << _T("charEx::test_getToken(") << in1 << _T(", ") << in2 << _T(") : ");
		if (t.first == out1)  {
			if ((t.second == NULL) && (out2 == NULL))
				std::wcout << _T("Succeeded") << std::endl;
			else if (t.second == NULL)
				std::wcout << _T("Failed [NULL=") << out2 << _T("]") << std::endl;
			else if (out2 == NULL)
				std::wcout << _T("Failed [") << t.second << _T("=NULL]") << std::endl;
			else if (wcscmp(t.second, out2) == 0)
				std::wcout << _T("Succeeded") << std::endl;
			else
				std::wcout << _T("Failed") << std::endl;
		} else
			std::wcout << _T("Failed [") << out1 << _T("=") << t.first << _T("]") << std::endl;
	}
	inline void run_test_getToken() {
		test_getToken(_T(""), '&', _T(""), NULL);
		test_getToken(_T("&"), '&', _T(""), NULL);
		test_getToken(_T("&&"), '&', _T(""), _T("&"));
		test_getToken(_T("foo"), '&', _T("foo"), NULL);
		test_getToken(_T("foo&"), '&', _T("foo"), NULL);
		test_getToken(_T("foo&bar"), '&', _T("foo"), _T("bar"));
		test_getToken(_T("foo&bar&test"), '&', _T("foo"), _T("bar&test"));
	}
#endif

};