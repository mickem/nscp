#pragma once

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
	typedef char* arrayBufferItem;
	typedef arrayBufferItem* arrayBuffer;
	typedef std::list<std::string> arrayList;
	arrayList arrayBuffer2list(const unsigned int argLen, char **argument);
	arrayBuffer list2arrayBuffer(const arrayList lst, unsigned int &argLen);
	arrayBuffer split2arrayBuffer(const char* buffer, char splitChar, unsigned int &argLen);
	arrayBuffer split2arrayBuffer(const std::string buffer, char splitChar, unsigned int &argLen);
	std::string arrayBuffer2string(char **argument, const unsigned int argLen, std::string join);
	arrayBuffer createEmptyArrayBuffer(unsigned int &argLen);
	void destroyArrayBuffer(arrayBuffer argument, const unsigned int argLen);

#ifdef _DEBUG
	void test_createEmptyArrayBuffer();
	void test_split2arrayBuffer_str(std::string buffer, char splitter, int OUT_argLen);
	void test_split2arrayBuffer_char(char* buffer, char splitter, int OUT_argLen);
	void run_testArrayBuffer();
#endif
}