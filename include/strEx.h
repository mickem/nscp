#pragma once

#include <sstream>
#include <string>
#include <utility>

namespace strEx {

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
	inline std::pair<std::string,std::string> split(std::string str, std::string key) {
		std::string::size_type pos = str.find(key);
		if (pos == std::string::npos)
			return std::pair<std::string,std::string>(str, "");
		return std::pair<std::string,std::string>(str.substr(0, pos), str.substr(pos+key.length()));
	}
}