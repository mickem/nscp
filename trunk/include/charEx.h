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
		if (!p) {
			return token(buffer, NULL);
		}
		p++;
		return token(std::string(buffer, p-buffer-1), p);
	}
};