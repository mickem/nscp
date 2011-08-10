#pragma once

#include <boost/filesystem.hpp>

struct script_container {
	typedef std::list<script_container> list_type;

	script_container(std::wstring alias, boost::filesystem::wpath script) : alias(alias), script(script) {}
	script_container(boost::filesystem::wpath script) : script(script) {}
	script_container(const script_container &other) : alias(other.alias), script(other.script) {}
	script_container& operator=(const script_container &other) {
		alias = other.alias;
		script = other.script;
	}

	static void push(list_type &list, std::wstring alias, boost::filesystem::wpath script) {
		list.push_back(script_container(alias, script));
	}
	static void push(list_type &list, boost::filesystem::wpath script) {
		list.push_back(script_container(script));
	}
	boost::filesystem::wpath script;
	std::wstring alias;

	std::wstring to_wstring() {
		return script.string() + _T(" as ") + alias;
	}
};
