#pragma once
#include <assert.h>

namespace charEx {
	/**
	 * Function to split a char buffer into a list<string>
	 * @param buffer A char buffer to iterate over.
	 * @param split The char to split by
	 * @return a list with strings
	 */
	inline std::list<std::string> split(const char* buffer, char split) {
		std::list<std::string> ret;
		const char *start = buffer;
		for (const char *p = buffer;*p!='\0';p++) {
			if (*p==split) {
				std::string str(start, p-start);
				ret.push_back(str);
				start = p+1;
			}
		}
		ret.push_back(std::string(start));
		return ret;
	}



	typedef std::pair<std::string,char*> token;
	inline token getToken(char *buffer, char split) {
		assert(buffer != NULL);
		char *p = strchr(buffer, split);
		if (!p)
			return token(buffer, NULL);
		if (!p[1])
			return token(std::string(buffer, p-buffer), NULL);
		p++;
		return token(std::string(buffer, p-buffer-1), p);
	}
#ifdef _DEBUG
	inline void test_getToken(char* in1, char in2, std::string out1, char * out2) {
		token t = getToken(in1, in2);
		std::cout << "charEx::test_getToken(" << in1 << ", " << in2 << ") : ";
		if (t.first == out1)  {
			if ((t.second == NULL) && (out2 == NULL))
				std::cout << "Succeeded" << std::endl;
			else if (t.second == NULL)
				std::cout << "Failed [NULL=" << out2 << "]" << std::endl;
			else if (out2 == NULL)
				std::cout << "Failed [" << t.second << "=NULL]" << std::endl;
			else if (strcmp(t.second, out2) == 0)
				std::cout << "Succeeded" << std::endl;
			else
				std::cout << "Failed" << std::endl;
		} else
			std::cout << "Failed [" << out1 << "=" << t.first << "]" << std::endl;
	}
	inline void run_test_getToken() {
		test_getToken("", '&', "", NULL);
		test_getToken("&", '&', "", NULL);
		test_getToken("&&", '&', "", "&");
		test_getToken("foo", '&', "foo", NULL);
		test_getToken("foo&", '&', "foo", NULL);
		test_getToken("foo&bar", '&', "foo", "bar");
		test_getToken("foo&bar&test", '&', "foo", "bar&test");
	}
#endif

};