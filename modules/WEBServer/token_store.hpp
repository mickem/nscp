#pragma once

#include "grant_store.hpp"

#include <boost/unordered_set.hpp>

#include <string>

class token_store {
	typedef boost::unordered_map<std::string, std::string> token_map;

	token_map tokens;
	grant_store grants;
public:
	static std::string generate_token(int len);

	bool validate(const std::string &token) {
		return tokens.find(token) != tokens.end();
	}

	std::string get(const std::string &token) const {
		token_map::const_iterator cit = tokens.find(token);
		if (cit != tokens.end()) {
			return cit->second;
		}
		return "";
	}

	std::string generate(std::string user) {
		std::string token = generate_token(32);
		tokens[token] = user;
		return token;
	}

	void revoke(const std::string &token) {
		token_map::iterator it = tokens.find(token);
		if (it != tokens.end())
			tokens.erase(it);
	}
	bool can(std::string uid, std::string grant);
	void add_user(std::string user, std::string role);
	void add_grant(std::string role, std::string grant);
};