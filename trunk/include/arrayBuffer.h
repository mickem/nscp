#pragma once


namespace arrayBuffer {


	std::list<std::string> arrayBuffer2list(const unsigned int argLen, char **argument);
	char ** list2arrayBuffer(const std::list<std::string> lst, unsigned int &argLen);
	char ** split2arrayBuffer(const char* buffer, char splitChar, unsigned int &argLen);
	char ** split2arrayBuffer(const std::string buffer, char splitChar, unsigned int &argLen);
	std::string arrayBuffer2string(char **argument, const unsigned int argLen, std::string join);
	char ** createEmptyArrayBuffer(unsigned int &argLen);
	void destroyArrayBuffer(char **argument, const unsigned int argLen);


#ifdef _DEBUG
	void test_createEmptyArrayBuffer();
	void test_split2arrayBuffer_str(std::string buffer, char splitter, int OUT_argLen);
	void test_split2arrayBuffer_char(char* buffer, char splitter, int OUT_argLen);

	void run_testArrayBuffer();
#endif

}