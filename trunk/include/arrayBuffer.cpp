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
#include "stdafx.h"

#include <arrayBuffer.h>

/**
 * Make a list out of a array of char arrays (arguments type)
 * @param argLen Length of argument array
 * @param *argument[] Argument array
 * @return Argument wrapped as a list
 */
arrayBuffer::arrayList arrayBuffer::arrayBuffer2list(const unsigned int argLen, char *argument[]) {
	arrayList ret;
	int i=0;
	for (unsigned int i=0;i<argLen;i++) {
		std::string s = argument[i];
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
		std::string::size_type alen = (*it).size();
		arrayBuffer[i] = new char[alen+2];
		strncpy(arrayBuffer[i], (*it).c_str(), alen+1);
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
* Joins an arrayBuffer back into a string
* @param **argument The ArrayBuffer
* @param argLen The length of the ArrayBuffer
* @param join The char to use as separators when joining
* @return The joined arrayBuffer
*/
std::string arrayBuffer::arrayBuffer2string(arrayBuffer::arrayBuffer argument, const unsigned int argLen, std::string join) {
	std::string ret;
	for (unsigned int i=0;i<argLen;i++) {
		ret += argument[i];
		if (i != argLen-1)
			ret += join;
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
arrayBuffer::arrayBuffer arrayBuffer::split2arrayBuffer(const char* buffer, char splitChar, unsigned int &argLen) {
	assert(buffer);
	argLen = 0;
	const char *p = buffer;
	if (!p[0]) {
		return createEmptyArrayBuffer(argLen);
	}
	while (*p) {
		if (*p == splitChar)
			argLen++;
		p++;
	}
	argLen++;
	char **arrayBuffer = new char*[argLen];
	p = buffer;
	for (unsigned int i=0;i<argLen;i++) {
		const char *q = strchr(p, (i<argLen-1)?splitChar:0);
		unsigned int len = static_cast<int>(q-p);
		arrayBuffer[i] = new char[len+1];
		strncpy(arrayBuffer[i], p, len);
		arrayBuffer[i][len] = 0;
		p = ++q;
	}
	return arrayBuffer;
}
/**
 * Split a string into elements as a newly created arrayBuffer
 * @param inBuf The CharArray to split along
 * @param splitChar The char to use as splitter
 * @param &argLen [OUT] The length of the Array
 * @return The arrayBuffer
 */
arrayBuffer::arrayBuffer arrayBuffer::split2arrayBuffer(const std::string inBuf, char splitChar, unsigned int &argLen) {
	if (inBuf.empty())
		return createEmptyArrayBuffer(argLen);

	argLen = 1;
	std::string::size_type p = inBuf.find(splitChar);
	while (p != std::string::npos) {
		argLen++;
		p = inBuf.find(splitChar, p+1);
	}
	arrayBuffer::arrayBuffer arrayBuffer = new arrayBuffer::arrayBufferItem[argLen];
	p = inBuf.find(splitChar);
	std::string::size_type l = 0;
	for (unsigned int i=0;i<argLen;i++) {
		if (p == std::string::npos)
			p = inBuf.size();
		//		char *q = strchr(p, (i<argLen-1)?splitChar:0);
		unsigned int len = static_cast<int>(p-l);
		arrayBuffer[i] = new char[len+1];
		strncpy(arrayBuffer[i], inBuf.substr(l,p).c_str(), len);
		arrayBuffer[i][len] = 0;
		l = ++p;
		p = inBuf.find(splitChar, p);
	}
	return arrayBuffer;
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
	std::cout << "arrayBuffer::test_createEmptyArrayBuffer() : ";
	unsigned int argLen;
	char ** c = createEmptyArrayBuffer(argLen);
	if ((c) && (argLen == 0))
		std::cout << "Succeeded" << std::endl;
	else
		std::cout << "Failed" << std::endl;
	destroyArrayBuffer(c, argLen);
}
/**
 * Test function for split2arrayBuffer
 * @param buffer 
 * @param splitter 
 * @param OUT_argLen 
 */
void arrayBuffer::test_split2arrayBuffer_str(std::string buffer, char splitter, int OUT_argLen) {
	std::cout << "arrayBuffer::test_split2arrayBuffer(" << buffer << ", ...) : ";
	unsigned int argLen = 0;
	char ** c = split2arrayBuffer(buffer, splitter, argLen);
	if ((c) && (argLen == OUT_argLen))
		std::cout << "Succeeded" << std::endl;
	else
		std::cout << "Failed |" << argLen << "=" << OUT_argLen << "]" << std::endl;
	destroyArrayBuffer(c, argLen);
}
/**
 * Test function for split2arrayBuffer
 * @param buffer 
 * @param splitter 
 * @param OUT_argLen 
 */
void arrayBuffer::test_split2arrayBuffer_char(char* buffer, char splitter, int OUT_argLen) {
	std::cout << "arrayBuffer::test_split2arrayBuffer(" << buffer << ", ...) : ";
	unsigned int argLen = 0;
	char ** c = split2arrayBuffer(buffer, splitter, argLen);
	if ((c) && (argLen == OUT_argLen))
		std::cout << "Succeeded" << std::endl;
	else
		std::cout << "Failed |" << argLen << "=" << OUT_argLen << "]" << std::endl;
	destroyArrayBuffer(c, argLen);
}

/**
 * Test function for ArrayBuffer
 */
void arrayBuffer::run_testArrayBuffer() {
	test_createEmptyArrayBuffer();
	test_split2arrayBuffer_str("", '&', 0);
	test_split2arrayBuffer_str("foo", '&', 1);
	test_split2arrayBuffer_str("&", '&', 2);
	test_split2arrayBuffer_str("foo&", '&', 2);
	test_split2arrayBuffer_str("&foo&", '&', 3);
	test_split2arrayBuffer_str("foo&bar", '&', 2);
	test_split2arrayBuffer_str("foo&bar&test", '&', 3);
	test_split2arrayBuffer_str("foo&&&", '&', 4);

	test_split2arrayBuffer_char("", '&', 0);
	test_split2arrayBuffer_char("foo", '&', 1);
	test_split2arrayBuffer_char("&", '&', 2);
	test_split2arrayBuffer_char("foo&", '&', 2);
	test_split2arrayBuffer_char("&foo&", '&', 3);
	test_split2arrayBuffer_char("foo&bar", '&', 2);
	test_split2arrayBuffer_char("foo&bar&test", '&', 3);
	test_split2arrayBuffer_char("foo&&&", '&', 4);
}
#endif