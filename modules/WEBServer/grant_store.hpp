#pragma once

#include <boost/unordered_map.hpp>

#include <list>
#include <string>

struct grants {
	std::list<std::string> rules;
};
class grant_store {
	boost::unordered_map<std::string, grants> roles;
	boost::unordered_map<std::string, std::string> users;
public:

	void add_role(std::string &role, std::string &grant);
	void add_user(std::string &user, std::string &role);
	void remove_role(std::string &role);
	void remove_user(std::string &uid);
	void clear();

	bool validate(const std::string &uid, std::string &check);

private:
	typedef std::list<std::string> grant_list;
	grants fetch_role(const std::string &uid);
	bool validate_grants(grant_list &grant, grant_list &need);
};