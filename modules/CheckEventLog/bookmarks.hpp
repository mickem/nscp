#pragma once

#include <boost/thread/mutex.hpp>
#include <boost/optional.hpp>

class bookmarks {
public:
	typedef boost::optional<std::string> op_string;

private:

	boost::timed_mutex mutex_;
	typedef std::map<std::string, std::string> map_type;
	map_type bookmarks_;


public:

	void add(const std::string key, const std::string bookmark);
	op_string get(const std::string key);
};

