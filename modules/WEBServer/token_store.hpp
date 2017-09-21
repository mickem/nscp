#pragma once

#include <string>

#include <boost/unordered_set.hpp>

class token_store {
	typedef boost::unordered_set<std::string> token_set;

	token_set tokens;
public:
	static std::string generate_token(int len);

	bool validate(const std::string &token) {
		return tokens.find(token) != tokens.end();
	}

	std::string generate() {
		std::string token = generate_token(32);
		tokens.emplace(token);
		return token;
	}

	void revoke(const std::string &token) {
		token_set::iterator it = tokens.find(token);
		if (it != tokens.end())
			tokens.erase(it);
	}
};