#pragma once
#include <list>

#include <boost/filesystem.hpp>

#include <NSCAPI.h>
#include <unicode_char.hpp>
#include <utf8.hpp>


struct script_container {
	typedef std::list<script_container> list_type;

	std::wstring alias;
	boost::filesystem::path script;

	script_container(std::wstring alias, boost::filesystem::path script) 
		: alias(alias)
		, script(script) {}
	script_container(boost::filesystem::path script) : script(script) {}
	script_container(const script_container &other) : alias(other.alias), script(other.script) {}
	script_container& operator=(const script_container &other) {
		alias = other.alias;
		script = other.script;
		return *this;
	}

	bool validate(std::wstring &error) const {
		if (script.empty()) {
			error = _T("No script given on command line!");
			return false;
		}
		if (!boost::filesystem::exists(script)) {
			error = _T("Script not found: ") + utf8::cvt<std::wstring>(script.string());
			return false;
		}
		if (!boost::filesystem::is_regular(script)) {
			error = _T("Script is not a file: ") + utf8::cvt<std::wstring>(script.string());
			return false;
		}
		return true;
	}

	static void push(list_type &list, std::wstring alias, boost::filesystem::path script) {
		list.push_back(script_container(alias, script));
	}
	static void push(list_type &list, boost::filesystem::path script) {
		list.push_back(script_container(script));
	}
	std::wstring to_wstring() {
		return utf8::cvt<std::wstring>(script.string()) + _T(" as ") + alias;
	}
};
