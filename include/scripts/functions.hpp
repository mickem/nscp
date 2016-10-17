#pragma once
#include <list>

#include <boost/filesystem.hpp>

#include <NSCAPI.h>
#include <unicode_char.hpp>
#include <utf8.hpp>


struct script_container {
	typedef std::list<script_container> list_type;

	std::string alias;
	boost::filesystem::path script;

	script_container(std::string alias, boost::filesystem::path script) 
		: alias(alias)
		, script(script) {}
	script_container(boost::filesystem::path script) : script(script) {}
	script_container(const script_container &other) : alias(other.alias), script(other.script) {}
	script_container& operator=(const script_container &other) {
		alias = other.alias;
		script = other.script;
		return *this;
	}

	bool validate(std::string &error) const {
		if (script.empty()) {
			error = "No script given on command line!";
			return false;
		}
		if (!boost::filesystem::exists(script)) {
			error = "Script not found: " + script.string();
			return false;
		}
		if (!boost::filesystem::is_regular(script)) {
			error = "Script is not a file: " + script.string();
			return false;
		}
		return true;
	}

	static void push(list_type &list, std::string alias, boost::filesystem::path script) {
		list.push_back(script_container(alias, script));
	}
	static void push(list_type &list, boost::filesystem::path script) {
		list.push_back(script_container(script));
	}
	std::string to_string() {
		return script.string() + " as " + alias;
	}
};
