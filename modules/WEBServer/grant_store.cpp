#include "grant_store.hpp"

#include <str/utils.hpp>

#include <boost/foreach.hpp>

void grant_store::add_role(std::string &role, std::string &grant) {
	BOOST_FOREACH(const std::string &g, str::utils::split<std::list<std::string> >(grant, ",")) {
		roles[role].rules.push_back(g);
	}
}

void grant_store::add_user(std::string &user, std::string &role) {
	users[user] = role;
}

void grant_store::remove_role(std::string &role) {
	roles.erase(role);
}

void grant_store::remove_user(std::string &uid) {
	users.erase(uid);
}

void grant_store::clear() {
	roles.clear();
	users.clear();
}

bool grant_store::validate(const std::string &uid, std::string &check) {
	std::list<std::string> need = str::utils::split_lst(check, ".");
	grants g = fetch_role(uid);
	BOOST_FOREACH(const std::string &rule, g.rules) {
		std::list<std::string> tokens = str::utils::split_lst(rule, ".");
		if (validate_grants(tokens, need)) {
			return true;
		}
	}
	return false;
}

grants grant_store::fetch_role(const std::string &uid) {
	std::string role = users[uid];
	if (role.empty()) {
		return grants();
	}
	return roles[role];
}

bool grant_store::validate_grants(std::list<std::string> &grant, std::list<std::string> &need) {
	grant_list::const_iterator cg = grant.begin();
	grant_list::const_iterator cn = need.begin();
	while (cn != need.end()) {
		if (cg == grant.end()) {
			return false;
		}
		if (*cg == "*") {
			return true;
		}
		if (*cn != *cg) {
			return false;
		}
		cg++;
		cn++;
	}
	return true;
}
