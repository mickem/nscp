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
	inline stoi(std::string s) {
		return atoi(s.c_str());
	}
	inline std::pair<std::string,std::string> split(std::string str, std::string key) {
		std::string::size_type pos = str.find(key);
		if (pos == std::string::npos)
			return std::pair<std::string,std::string>(str, "");
		return std::pair<std::string,std::string>(str.substr(0, pos), str.substr(pos+key.length()));
	}
}