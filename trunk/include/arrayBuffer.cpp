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
char ** arrayBuffer::list2arrayBuffer(const arrayList lst, unsigned int &argLen) {
	argLen = static_cast<unsigned int>(lst.size());
	char **arrayBuffer = new char*[argLen];
	arrayList::const_iterator it = lst.begin();
	for (int i=0;it!=lst.end();++it,i++) {
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
char ** arrayBuffer::createEmptyArrayBuffer(unsigned int &argLen) {
	argLen = 0;
	char **arrayBuffer = new char*[0];
	return arrayBuffer;
}



/**
* Joins an arrayBuffer back into a string
* @param **argument The ArrayBuffer
* @param argLen The length of the ArrayBuffer
* @param join The char to use as separators when joining
* @return The joined arrayBuffer
*/
std::string arrayBuffer::arrayBuffer2string(char **argument, const unsigned int argLen, std::string join) {
	std::string ret;
	for (unsigned int i=0;i<argLen;i++) {
		ret += argument[i];
		if (i != argLen-1)
			ret += join;
	}
	return ret;
}
/**
* Split a string into elements as an arrayBuffer
* @param buffer The CharArray to split along
* @param splitChar The char to use as splitter
* @param &argLen [OUT] The length of the Array
* @return The arrayBuffer
*/
char ** arrayBuffer::split2arrayBuffer(const char* buffer, char splitChar, unsigned int &argLen) {
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
		char *q = strchr(p, (i<argLen-1)?splitChar:0);
		unsigned int len = static_cast<int>(q-p);
		arrayBuffer[i] = new char[len+1];
		strncpy(arrayBuffer[i], p, len);
		arrayBuffer[i][len] = 0;
		p = ++q;
	}
	return arrayBuffer;
}
char ** arrayBuffer::split2arrayBuffer(const std::string inBuf, char splitChar, unsigned int &argLen) {
	if (inBuf.empty())
		return createEmptyArrayBuffer(argLen);

	argLen = 1;
	std::string::size_type p = inBuf.find(splitChar);
	while (p != std::string::npos) {
		argLen++;
		p = inBuf.find(splitChar, p+1);
	}
	char **arrayBuffer = new char*[argLen];
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
void arrayBuffer::destroyArrayBuffer(char **argument, const unsigned int argLen) {
	for (unsigned int i=0;i<argLen;i++) {
		delete [] argument[i];
	}
	delete [] argument;
}



#ifdef _DEBUG
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