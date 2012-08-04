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
#include <unicode_char.hpp>
#include <types.hpp>

#include <string>
#include <locale>
#include <cctype>

#ifdef __GNUC__
#include <iconv.h>
#include <errno.h>
#endif

namespace utf8 {
	/** Converts a std::wstring into a std::string with UTF-8 encoding. */
	template<typename StringT>
	StringT cvt(std::wstring const & string);

	/** Converts a std::String with UTF-8 encoding into a std::wstring.	*/
	template<typename StringT>
	StringT cvt(std::string const & string );

	/** Nop specialization for std::string. */
	template <>
	inline std::string cvt(std::string const & string) {
		return string;
	}

	/** Nop specialization for std::wstring. */
	template<>
	inline std::wstring cvt(std::wstring const & rc_string) {
		return rc_string;
	}

	inline std::wstring to_unicode(std::string const & str) {
#ifdef WIN32
		int len = static_cast<int>(str.length());
		int nChars = MultiByteToWideChar(CP_ACP, 0, str.c_str(), len, NULL, 0);
		if (nChars == 0)
			return L"";
		wchar_t *buffer = new wchar_t[nChars+1];
		if (buffer == NULL)
			return L"";
		MultiByteToWideChar(CP_ACP, 0, str.c_str(), len, buffer, nChars);
		buffer[nChars] = 0;
		std::wstring buf(buffer, nChars);
		delete [] buffer;
		return buf;
#else
		size_t utf8Length = str.length();
		size_t outbytesLeft = utf8Length*sizeof(wchar_t);

		//Copy the instring
		char *inString = new char[str.length()+1];
		strcpy(inString, str.c_str());

		//Create buffer for output
		char *outString = (char*)new wchar_t[utf8Length+1];
		memset(outString, 0, sizeof(wchar_t)*(utf8Length+1));

		char *inPointer = inString;
		char *outPointer = outString;

		iconv_t convDesc = iconv_open("WCHAR_T", "");
		iconv(convDesc, &inPointer, &utf8Length, &outPointer, &outbytesLeft);
		iconv_close(convDesc);

		std::wstring retval( (wchar_t *)outString );

		//Cleanup
		delete[] inString;
		delete[] outString;

		return retval;
#endif	
	}

	template<>
	inline std::string cvt(std::wstring const & str) {
#ifdef WIN32
		// figure out how many narrow characters we are going to get 
		int nChars = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), NULL, 0, NULL, NULL);
		if (nChars == 0)
			return "";

		// convert the wide string to a narrow string
		// nb: slightly naughty to write directly into the string like this
		std::string buf;
		buf.resize(nChars);
		WideCharToMultiByte(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), const_cast<char*>(buf.c_str()), nChars, NULL, NULL);
		return buf;
#else
		size_t wideSize = sizeof(wchar_t)*str.length();
		size_t outbytesLeft = wideSize+sizeof(char); //We cannot know how many wide character there is yet

		//Copy the instring
		char *inString = (char*)new wchar_t[str.length()+1];
		memcpy(inString, str.c_str(), wideSize+sizeof(wchar_t));

		//Create buffer for output
		char *outString = new char[outbytesLeft];
		memset(outString, 0, sizeof(char)*(outbytesLeft));

		char *inPointer = inString;
		char *outPointer = outString;

		iconv_t convDesc = iconv_open("UTF-8", "WCHAR_T");
		iconv(convDesc, &inPointer, &wideSize, &outPointer, &outbytesLeft);
		iconv_close(convDesc);

		std::string retval(outString);

		//Cleanup
		delete[] inString;
		delete[] outString;

		return retval;
#endif
	}

	template<>
	inline std::wstring cvt(std::string const & str) {
#ifdef WIN32
		int len = static_cast<int>(str.length());
		int nChars = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), len, NULL, 0);
		if (nChars == 0)
			return L"";
		wchar_t *buffer = new wchar_t[nChars+1];
		if (buffer == NULL)
			return L"";
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), len, buffer, nChars);
		buffer[nChars] = 0;
		std::wstring buf(buffer, nChars);
		delete [] buffer;
		return buf;
#else
		size_t utf8Length = str.length();
		size_t outbytesLeft = utf8Length*sizeof(wchar_t);

		//Copy the instring
		char *inString = new char[str.length()+1];
		strcpy(inString, str.c_str());

		//Create buffer for output
		char *outString = (char*)new wchar_t[utf8Length+1];
		memset(outString, 0, sizeof(wchar_t)*(utf8Length+1));

		char *inPointer = inString;
		char *outPointer = outString;

		iconv_t convDesc = iconv_open("WCHAR_T", "UTF-8");
		iconv(convDesc, &inPointer, &utf8Length, &outPointer, &outbytesLeft);
		iconv_close(convDesc);

		std::wstring retval( (wchar_t *)outString );

		//Cleanup
		delete[] inString;
		delete[] outString;

		return retval;
#endif
	}
	inline std::string utf8_from_native(std::string const & str) {
		return cvt<std::string>(to_unicode(str));
	}


}

namespace boost
{
	template<>
	inline std::wstring lexical_cast<std::wstring, std::string>(const std::string& arg) {
		return utf8::cvt<std::wstring>(arg);
	}

	template<>
	inline std::string lexical_cast<std::string, std::wstring>(const std::wstring& arg) {
		return utf8::cvt<std::string>(arg);
	}
}
