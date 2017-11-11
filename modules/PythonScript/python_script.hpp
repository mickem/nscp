#pragma once

#include <boost/python/dict.hpp>
#include <boost/filesystem/path.hpp>

#include <string>
#include <list>

struct python_script : public boost::noncopyable {
	std::string base_path;
	unsigned int plugin_id;
	boost::python::dict localDict;
	python_script(unsigned int plugin_id, const std::string base_path, const std::string plugin_alias, const std::string script_alias, const std::string script);
	~python_script();
	bool callFunction(const std::string& functionName);
	bool callFunction(const std::string& functionName, unsigned int i1, const std::string &s1, const std::string &s2);
	bool callFunction(const std::string& functionName, const std::list<std::string> &args);
	void _exec(const std::string &scriptfile);

	static void init();
};

