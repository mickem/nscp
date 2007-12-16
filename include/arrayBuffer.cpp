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
#include <arrayBuffer.h>
#include <msvc_wrappers.h>

/**
 * Make a list out of a array of char arrays (arguments type)
 * @param argLen Length of argument array
 * @param *argument[] Argument array
 * @return Argument wrapped as a list
 */
arrayBuffer::arrayList arrayBuffer::arrayBuffer2list(const unsigned int argLen, TCHAR *argument[]) {
	arrayList ret;
	int i=0;
	for (unsigned int i=0;i<argLen;i++) {
		std::wstring s = argument[i];
		ret.push_back(s);
	}
	return ret;
}
/**
* Create an arrayBuffer from a list.
* This is the reverse of arrayBuffer2list.
* <b>Notice</b> it is up to the caller to free the memory allocated in the returned buffer.
*
* @param lst A list to convert.
* @param &argLen Write the length to this argument.
* @return A pointer that is managed by the caller.
*/
arrayBuffer::arrayBuffer arrayBuffer::list2arrayBuffer(const arrayList lst, unsigned int &argLen) {
	argLen = static_cast<unsigned int>(lst.size());
	arrayBuffer::arrayBuffer arrayBuffer = new arrayBuffer::arrayBufferItem[argLen];
	arrayList::const_iterator it = lst.begin();
	int i;
	for (i=0;it!=lst.end();++it,i++) {
		std::wstring::size_type alen = (*it).size();
		arrayBuffer[i] = new TCHAR[alen+2];
		wcsncpy_s(arrayBuffer[i], alen+2, (*it).c_str(), alen+1);
	}
	assert(i == argLen);
	return arrayBuffer;
}
/**
* Creates an empty arrayBuffer (only used to allow consistency)
* @param &argLen [OUT] The length (items) of the arrayBuffer
* @return The arrayBuffer
*/
arrayBuffer::arrayBuffer arrayBuffer::createEmptyArrayBuffer(unsigned int &argLen) {
	argLen = 0;
	arrayBuffer::arrayBuffer arrayBuffer = new arrayBuffer::arrayBufferItem[0];
	return arrayBuffer;
}
/**
* Creates an arrayBuffer with N-elements
* @param &argLen [IN OUT] The length (items) of the arrayBuffer
* @return The arrayBuffer
*/
arrayBuffer::arrayBuffer arrayBuffer::createArrayBuffer(unsigned int &argLen) {
	arrayBuffer::arrayBuffer arrayBuffer = new arrayBuffer::arrayBufferItem[argLen];
	for (unsigned int i=0;i<argLen;i++) {
		arrayBuffer[i] = NULL;
	}
	return arrayBuffer;
}
/**
* Joins an arrayBuffer back into a string
* @param **argument The ArrayBuffer
* @param argLen The length of the ArrayBuffer
* @param join The char to use as separators when joining
* @return The joined arrayBuffer
*/
std::wstring arrayBuffer::arrayBuffer2string(arrayBuffer::arrayBuffer argument, const unsigned int argLen, std::wstring join) {
	std::wstring ret;
	for (unsigned int i=0;i<argLen;i++) {
		if (argument[i] != NULL) {
			ret += argument[i];
			if (i != argLen-1)
				ret += join;
		}
	}
	return ret;
}
/**
* Split a string into elements as a newly created arrayBuffer
* @param buffer The CharArray to split along
* @param splitChar The char to use as splitter
* @param &argLen [OUT] The length of the Array
* @return The arrayBuffer
*/
arrayBuffer::arrayBuffer arrayBuffer::split2arrayBuffer(const TCHAR* buffer, TCHAR splitChar, unsigned int &argLen) {
	assert(buffer);
	argLen = 0;
	const TCHAR *p = buffer;
	if (!p[0]) {
		return createEmptyArrayBuffer(argLen);
	}
	while (*p) {
		if (*p == splitChar)
			argLen++;
		p++;
	}
	argLen++;
	TCHAR **arrayBuffer = new TCHAR*[argLen];
	p = buffer;
	for (unsigned int i=0;i<argLen;i++) {
		const TCHAR *q = wcschr(p, (i<argLen-1)?splitChar:0);
		unsigned int len = static_cast<int>(q-p);
		arrayBuffer[i] = new TCHAR[len+1];
		wcsncpy_s(arrayBuffer[i], len+1, p, len);
		arrayBuffer[i][len] = 0;
		p = ++q;
	}
	return arrayBuffer;
}


void arrayBuffer::set(arrayBuffer arrayBuffer, const unsigned int argLen, const unsigned int position, std::wstring argument) {
	if (position >= argLen)
		assert(false);
	delete [] arrayBuffer[position];
	size_t len = argument.length();
	arrayBuffer[position] = new TCHAR[len+2];
	wcsncpy_s(arrayBuffer[position], len+1, argument.c_str(), len);
	arrayBuffer[position][len] = 0;
}

/**
 * Split a string into elements as a newly created arrayBuffer
 * @param inBuf The CharArray to split along
 * @param splitChar The char to use as splitter
 * @param &argLen [OUT] The length of the Array
 * @param escape [IN] Set to true to try to escape ":s ie. //token1 "token2 with space" token3//
 * @return The arrayBuffer
 */
arrayBuffer::arrayBuffer arrayBuffer::split2arrayBuffer(const std::wstring inBuf, TCHAR splitChar, unsigned int &argLen, bool escape) {
	if (inBuf.empty())
		return createEmptyArrayBuffer(argLen);

	std::wstring::size_type p1 = 0;
	std::wstring::size_type p2 = 0;
	std::list<std::wstring> token_list;
	while (p1 != std::wstring::npos && p2 != std::wstring::npos) {
		if (escape && inBuf[p1] == '\"') {
			p2 = inBuf.find('\"', p1+1);
			if (p2 != std::wstring::npos)
				p2 = inBuf.find(splitChar, p2);
		} else {
			p2 = inBuf.find(splitChar, p1);
		}
		if (p2 == std::wstring::npos)
			p2 = inBuf.size();
		// p1 = start of "this token"
		// p2 = end of "this token" (next split char)
		//std::wcout << _T("found token: ") << p1 << _T(":") << p2;

		assert(p2>p1);
		std::wstring token = inBuf.substr(p1,p2-p1);
		if (escape && token[0] == '\"')
			token = token.substr(1);
		if (escape && token[token.size()-1] == '\"')
			token = token.substr(0, token.size()-1);

		//std::wcout << _T(" -- ") << token << std::endl;

		token_list.push_back(token);
		if (p2 < inBuf.size())
			p2++;
		if (p2 == inBuf.size())
			p2 = std::wstring::npos;
		p1 = p2;
	}
	arrayBuffer::arrayBuffer arrayBuffer = new arrayBuffer::arrayBufferItem[token_list.size()];
	argLen=0;
	for (std::list<std::wstring>::const_iterator cit=token_list.begin();cit!=token_list.end();++cit) {
		size_t len = (*cit).size();
		TCHAR* token = new TCHAR[len+1];
		wcsncpy_s(token, len+1, (*cit).c_str(), len);
		arrayBuffer[argLen++] = token;
	}
	token_list.clear();
	return arrayBuffer;
/*

		unsigned int len = static_cast<unsigned int>(p-l);
		arrayBuffer[i] = new TCHAR[len+1];
		wcsncpy_s(arrayBuffer[i], len+1, inBuf.substr(l,p).c_str(), len);
		arrayBuffer[i][len] = 0;
		if (p == std::wstring::npos)
			break;
		l = ++p;
		if (escape && l < inBuf.size() && inBuf[l] == '\"') {
			p = inBuf.find('\"', l+1);
			if (p != std::wstring::npos)
				p = inBuf.find(splitChar, p);
		} else if (p < inBuf.size()) {
			p = inBuf.find(splitChar, p);
		} else {
			p = std::wstring::npos;
		}
		*/
//	}

/*

	argLen = 1;
	std::wstring::size_type p = inBuf.find(splitChar);
	while (p != std::wstring::npos) {
		argLen++;
		p = inBuf.find(splitChar, p+1);
	}
	arrayBuffer::arrayBuffer arrayBuffer = new arrayBuffer::arrayBufferItem[argLen];
	if (escape && inBuf[0] == '\"') {
		p = inBuf.find('\"');
		if (p != std::wstring::npos)
			p = inBuf.find(splitChar, p);
	} else {
		p = inBuf.find(splitChar);
	}
	std::wstring::size_type l = 0;
	for (unsigned int i=0;i<argLen;i++) {
		if (p == std::wstring::npos)
			p = inBuf.size();
		//		TCHAR *q = strchr(p, (i<argLen-1)?splitChar:0);
		assert(p>l);
		unsigned int len = static_cast<unsigned int>(p-l);
		arrayBuffer[i] = new TCHAR[len+1];
		wcsncpy_s(arrayBuffer[i], len+1, inBuf.substr(l,p).c_str(), len);
		arrayBuffer[i][len] = 0;
		if (p == std::wstring::npos)
			break;
		l = ++p;
		if (escape && l < inBuf.size() && inBuf[l] == '\"') {
			p = inBuf.find('\"', l+1);
			if (p != std::wstring::npos)
				p = inBuf.find(splitChar, p);
		} else if (p < inBuf.size()) {
			p = inBuf.find(splitChar, p);
		} else {
			p = std::wstring::npos;
		}
	}
	*/
	//return arrayBuffer;
}

/**
* Destroy an arrayBuffer.
* The buffer should have been created with list2arrayBuffer.
*
* @param **argument 
* @param argLen 
*/
void arrayBuffer::destroyArrayBuffer(arrayBuffer::arrayBuffer argument, const unsigned int argLen) {
	for (unsigned int i=0;i<argLen;i++) {
		delete [] argument[i];
	}
	delete [] argument;
}



#ifdef _DEBUG
/**
 * Test function for createEmptyArrayBuffer
 */
void arrayBuffer::test_createEmptyArrayBuffer() {
	std::wcout << "arrayBuffer::test_createEmptyArrayBuffer() : ";
	unsigned int argLen;
	TCHAR ** c = createEmptyArrayBuffer(argLen);
	if ((c) && (argLen == 0))
		std::wcout << "Succeeded" << std::endl;
	else
		std::wcout << "Failed" << std::endl;
	destroyArrayBuffer(c, argLen);
}
/**
 * Test function for split2arrayBuffer
 * @param buffer 
 * @param splitter 
 * @param OUT_argLen 
 */
void arrayBuffer::test_split2arrayBuffer_str(std::wstring buffer, TCHAR splitter, int OUT_argLen) {
	std::wcout << _T("arrayBuffer::test_split2arrayBuffer(") << buffer << _T(", ...) : ");
	unsigned int argLen = 0;
	TCHAR ** c = split2arrayBuffer(buffer, splitter, argLen);
	if ((c) && (argLen == OUT_argLen))
		std::wcout << _T("Succeeded") << std::endl;
	else
		std::wcout << _T("Failed |") << argLen << _T("=") << OUT_argLen << _T("]") << std::endl;
	destroyArrayBuffer(c, argLen);
}
/**
 * Test function for split2arrayBuffer
 * @param buffer 
 * @param splitter 
 * @param OUT_argLen 
 */
void arrayBuffer::test_split2arrayBuffer_char(TCHAR* buffer, TCHAR splitter, int OUT_argLen) {
	std::wcout << _T("arrayBuffer::test_split2arrayBuffer(") << buffer << _T(", ...) : ");
	unsigned int argLen = 0;
	TCHAR ** c = split2arrayBuffer(buffer, splitter, argLen);
	if ((c) && (argLen == OUT_argLen))
		std::wcout << _T("Succeeded") << std::endl;
	else
		std::wcout << _T("Failed |") << argLen << _T("=") << OUT_argLen << _T("]") << std::endl;
	destroyArrayBuffer(c, argLen);
}

/**
 * Test function for ArrayBuffer
 */
void arrayBuffer::run_testArrayBuffer() {
	test_createEmptyArrayBuffer();
	test_split2arrayBuffer_str(_T(""), '&', 0);
	test_split2arrayBuffer_str(_T("foo"), '&', 1);
	test_split2arrayBuffer_str(_T("&"), '&', 2);
	test_split2arrayBuffer_str(_T("foo&"), '&', 2);
	test_split2arrayBuffer_str(_T("&foo&"), '&', 3);
	test_split2arrayBuffer_str(_T("foo&bar"), '&', 2);
	test_split2arrayBuffer_str(_T("foo&bar&test"), '&', 3);
	test_split2arrayBuffer_str(_T("foo&&&"), '&', 4);

	test_split2arrayBuffer_char(_T(""), '&', 0);
	test_split2arrayBuffer_char(_T("foo"), '&', 1);
	test_split2arrayBuffer_char(_T("&"), '&', 2);
	test_split2arrayBuffer_char(_T("foo&"), '&', 2);
	test_split2arrayBuffer_char(_T("&foo&"), '&', 3);
	test_split2arrayBuffer_char(_T("foo&bar"), '&', 2);
	test_split2arrayBuffer_char(_T("foo&bar&test"), '&', 3);
	test_split2arrayBuffer_char(_T("foo&&&"), '&', 4);
}
#endif