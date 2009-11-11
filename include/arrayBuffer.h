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
#include <string>
#include <list>
#include <iostream>
#include <types.hpp>
#include <msvc_wrappers.h>
/**
 * @ingroup NSClient++
 *
 * A simple namespace (wrapper) to wrap functions to manipulate an array buffer.
 *
 * @version 1.0
 * first version
 *
 * @date 05-14-2005
 *
 * @author mickem
 *
 * @par license
 * This code is absolutely free to use and modify. The code is provided "as is" with
 * no expressed or implied warranty. The author accepts no liability if it causes
 * any damage to your computer, causes your pet to fall ill, increases baldness
 * or makes your car start emitting strange noises when you start it up.
 * This code has no bugs, just undocumented features!
 * 
 * @todo 
 *
 * @bug 
 *
 */
namespace arrayBuffer {
	class ArrayBufferException {
	public:
		ArrayBufferException(std::wstring error) {}
	};
	typedef wchar_t* arrayBufferItem;
	typedef arrayBufferItem* arrayBuffer;
	typedef std::list<std::wstring> arrayList;
	void set(arrayBuffer arrayBuffer, const unsigned int argLen, const unsigned int position, std::wstring argument);
	arrayList arrayBuffer2list(const unsigned int argLen, wchar_t **argument);
	arrayBuffer list2arrayBuffer(const arrayList lst, unsigned int &argLen);
	arrayBuffer split2arrayBuffer(const wchar_t* buffer, wchar_t splitChar, unsigned int &argLen);
	arrayBuffer split2arrayBuffer(const std::wstring inBuf, wchar_t splitChar, unsigned int &argLen, bool escape = false);
	std::wstring arrayBuffer2string(wchar_t **argument, const unsigned int argLen, std::wstring join);
	arrayBuffer createEmptyArrayBuffer(unsigned int &argLen);
	arrayBuffer createArrayBuffer(unsigned int &argLen);
	void destroyArrayBuffer(arrayBuffer argument, const unsigned int argLen);
	inline arrayBuffer copy(const arrayBuffer &other, const unsigned int argLen) {
		arrayBufferItem* ret = new arrayBufferItem[argLen];
		for (unsigned int i=0; i<argLen; i++) {
			size_t s = wcslen(other[i]);
			ret[i] = new wchar_t[s+2];
			wcsncpy_s(ret[i], s+2, other[i], s);
		}
		return ret;
	}

#ifdef _DEBUG
	void test_createEmptyArrayBuffer();
	void test_split2arrayBuffer_str(std::wstring buffer, wchar_t splitter, int OUT_argLen);
	void test_split2arrayBuffer_char(wchar_t* buffer, wchar_t splitter, int OUT_argLen);
	void run_testArrayBuffer();
#endif
}