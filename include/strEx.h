#pragma once

#include <sstream>
#include <string>
#include <utility>

namespace strEx {

	inline void replace(std::string &string, std::string replace, std::string with) {
		std::string::size_type pos = string.find(replace);
		std::string::size_type len = replace.length();
		while (pos != std::string::npos) {
			string = string.substr(0,pos)+with+string.substr(pos+len);
			pos = string.find(replace, pos+1);
		}
	}
	inline std::string itos(unsigned int i) {
		std::stringstream ss;
		ss << i;
		return ss.str();
	}
	inline std::string itos(int i) {
		std::stringstream ss;
		ss << i;
		return ss.str();
	}
	inline std::string itos(__int64 i) {
		std::stringstream ss;
		ss << i;
		return ss.str();
	}
	inline std::string itos(DWORD i) {
		std::stringstream ss;
		ss << i;
		return ss.str();
	}
	inline int stoi(std::string s) {
		return atoi(s.c_str());
	}
	inline long long stoi64_as_BKMG(std::string s) {
		std::string::size_type p = s.find_first_of("BMKG");
		if (p == std::string::npos)
			return _atoi64(s.c_str());
		else if (s[p] == 'B') 
			return _atoi64(s.c_str());
		else if (s[p] == 'K') 
			return _atoi64(s.c_str())*1024;
		else if (s[p] == 'M') 
			return _atoi64(s.c_str())*1024*1024;
		else if (s[p] == 'G') 
			return _atoi64(s.c_str())*1024*1024*1024;
		else
			return _atoi64(s.c_str());
	}
	inline std::string itos_as_BKMG(long long i) {
		if (i > (1024*1024*1024))
			return itos(i/(1024*1024*1024))+"G (" + itos(i) + "B)";
		if (i > (1024*1024))
			return itos(i/(1024*1024))+"M (" + itos(i) + "B)";
		if (i > (1024))
			return itos(i/(1024))+"K (" + itos(i) + "B)";
		return itos(i>>24)+"B";
	}

	typedef std::list<std::string> splitList;
	inline splitList splitEx(std::string str, std::string key) {
		splitList ret;
		std::string::size_type pos = 0, lpos = 0;
		while ((pos = str.find(key, pos)) !=  std::string::npos) {
			ret.push_back(str.substr(lpos, pos-lpos));
			lpos = ++pos;
		}
		if (lpos < str.size())
			ret.push_back(str.substr(lpos));
		return ret;
	}

	inline std::pair<std::string,std::string> split(std::string str, std::string key) {
		std::string::size_type pos = str.find(key);
		if (pos == std::string::npos)
			return std::pair<std::string,std::string>(str, "");
		return std::pair<std::string,std::string>(str.substr(0, pos), str.substr(pos+key.length()));
	}
	typedef std::pair<std::string,std::string> token;
	inline token getToken(std::string buffer, char split) {
		std::string::size_type pos = buffer.find(split);
		if (pos == std::string::npos)
			return token(buffer, "");
		if (pos == buffer.length()-1)
			return token(buffer.substr(0, pos), "");
		return token(buffer.substr(0, pos-1), buffer.substr(++pos));
	}
#ifdef _DEBUG
	inline void test_getToken(std::string in1, char in2, std::string out1, std::string out2) {
		token t = getToken(in1, in2);
		std::cout << "strEx::test_getToken(" << in1 << ", " << in2 << ") : ";
		if ((t.first == out1) && (t.second == out2))
			std::cout << "Succeeded" << std::endl;
		else
			std::cout << "Failed [" << out1 << "=" << t.first << ", " << out2 << "=" << t.second << "]" << std::endl;
	}
	inline void run_test_getToken() {
		test_getToken("", '&', "", "");
		test_getToken("&", '&', "", "");
		test_getToken("&&", '&', "", "&");
		test_getToken("foo", '&', "foo", "");
		test_getToken("foo&", '&', "foo", "");
		test_getToken("foo&bar", '&', "foo", "bar");
		test_getToken("foo&bar&test", '&', "foo", "bar&test");
	}

	inline void test_replace(std::string source, std::string replace, std::string with, std::string out) {
		std::cout << "strEx::test_replace(" << source << ", " << replace << ", " << with << ") : ";
		std::string s = source;
		strEx::replace(s, replace, with);
		if (s == out)
			std::cout << "Succeeded" << std::endl;
		else
			std::cout << "Failed [" << s << "=" << out << "]" << std::endl;
	}
	inline void run_test_replace() {
		test_replace("", "", "", "");
		test_replace("foo", "", "", "foo");
		test_replace("foobar", "foo", "", "bar");
		test_replace("foobar", "foo", "bar", "barbar");
	}

#endif
}