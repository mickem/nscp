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


/**
 * Make a list out of a array of char arrays (arguments type)
 * @param argLen Length of argument array
 * @param *argument[] Argument array
 * @return Argument wrapped as a list
 */
array_buffer::arrayList array_buffer::arrayBuffer2list(const unsigned int argLen, wchar_t *argument[]) {
	arrayList ret;
	int i=0;
	for (unsigned int i=0;i<argLen;i++) {
		std::wstring s = argument[i];
		ret.push_back(s);
	}
	return ret;
}
/**
 * Make a list out of a array of char arrays (arguments type)
 * @param argLen Length of argument array
 * @param *argument[] Argument array
 * @return Argument wrapped as a list
 */
array_buffer::arrayVector array_buffer::arrayBuffer2vector(const unsigned int argLen, wchar_t *argument[]) {
	arrayVector ret;
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
array_buffer::arrayBuffer array_buffer::list2arrayBuffer(const arrayList lst, unsigned int &argLen) {
	argLen = static_cast<unsigned int>(lst.size());
	arrayBuffer arrayBuffer = new arrayBufferItem[argLen];
	arrayList::const_iterator it = lst.begin();
	int i;
	for (i=0;it!=lst.end();++it,i++) {
		std::wstring::size_type alen = (*it).size();
		arrayBuffer[i] = new wchar_t[alen+2];
		wcsncpy(arrayBuffer[i], (*it).c_str(), alen+1);
	}
	if (i != argLen)
		throw ArrayBufferException("Invalid length!");
	return arrayBuffer;
}
/**
* Creates an empty arrayBuffer (only used to allow consistency)
* @param &argLen [OUT] The length (items) of the arrayBuffer
* @return The arrayBuffer
*/
array_buffer::arrayBuffer array_buffer::createEmptyArrayBuffer(unsigned int &argLen) {
	argLen = 0;
	arrayBuffer arrayBuffer = new arrayBufferItem[0];
	return arrayBuffer;
}
/**
* Creates an arrayBuffer with N-elements
* @param &argLen [IN OUT] The length (items) of the arrayBuffer
* @return The arrayBuffer
*/
array_buffer::arrayBuffer array_buffer::createArrayBuffer(unsigned int &argLen) {
	arrayBuffer arrayBuffer = new arrayBufferItem[argLen];
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
std::wstring array_buffer::arrayBuffer2string(arrayBuffer argument, const unsigned int argLen, std::wstring join) {
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
array_buffer::arrayBuffer array_buffer::split2arrayBuffer(const wchar_t* buffer, wchar_t splitChar, unsigned int &argLen) {
	if (!buffer)
		throw ArrayBufferException("Invalid buffer specified!");
	argLen = 0;
	const wchar_t *p = buffer;
	if (!p[0]) {
		return createEmptyArrayBuffer(argLen);
	}
	while (*p) {
		if (*p == splitChar)
			argLen++;
		p++;
	}
	argLen++;
	wchar_t **arrayBuffer = new wchar_t*[argLen];
	p = buffer;
	for (unsigned int i=0;i<argLen;i++) {
		const wchar_t *q = wcschr(p, (i<argLen-1)?splitChar:0);
		unsigned int len = static_cast<int>(q-p);
		arrayBuffer[i] = new wchar_t[len+1];
		wcsncpy(arrayBuffer[i], p, len);
		arrayBuffer[i][len] = 0;
		p = ++q;
	}
	return arrayBuffer;
}


void array_buffer::set(arrayBuffer arrayBuffer, const unsigned int argLen, const unsigned int position, std::wstring argument) {
	if (position >= argLen)
		throw ArrayBufferException("position is outside the buffer");
	delete [] arrayBuffer[position];
	size_t len = argument.length();
	arrayBuffer[position] = new wchar_t[len+2];
	wcsncpy(arrayBuffer[position], argument.c_str(), len);
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
array_buffer::arrayBuffer array_buffer::split2arrayBuffer(const std::wstring inBuf, wchar_t splitChar, unsigned int &argLen, bool escape) {
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
		if (p1 == p2 && p1 != inBuf.size()) {
			p1++;
			continue;
		}
		// p1 = start of "this token"
		// p2 = end of "this token" (next split char)

		if (p2<=p1)
			throw ArrayBufferException("Invalid position");
		std::wstring token = inBuf.substr(p1,p2-p1);
		if (escape && token[0] == '\"')
			token = token.substr(1);
		if (escape && token[token.size()-1] == '\"')
			token = token.substr(0, token.size()-1);


		token_list.push_back(token);
		if (p2 < inBuf.size())
			p2++;
		if (p2 == inBuf.size())
			p2 = std::wstring::npos;
		p1 = p2;
	}
	arrayBuffer arrayBuffer = new arrayBufferItem[token_list.size()];
	argLen=0;
	for (std::list<std::wstring>::const_iterator cit=token_list.begin();cit!=token_list.end();++cit) {
		size_t len = (*cit).size();
		wchar_t* token = new wchar_t[len+1];
		wcsncpy(token, (*cit).c_str(), len);
		arrayBuffer[argLen++] = token;
	}
	token_list.clear();
	return arrayBuffer;
}

/**
* Destroy an arrayBuffer.
* The buffer should have been created with list2arrayBuffer.
*
* @param **argument 
* @param argLen 
*/
void array_buffer::destroyArrayBuffer(arrayBuffer argument, const unsigned int argLen) {
	for (unsigned int i=0;i<argLen;i++) {
		delete [] argument[i];
	}
	delete [] argument;
}


