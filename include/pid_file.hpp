#pragma once

#include <string>
#include <boost/filesystem/path.hpp>

#ifdef WIN32
typedef int pid_t;
#else
#include <unistd.h>
#include <sys/types.h>
#endif

class pidfile {
public:
	pidfile(boost::filesystem::path const & rundir, std::string const & process_name);
	pidfile(boost::filesystem::path const & filepath);
	~pidfile();

	bool create(pid_t const pid) const;
	bool create() const;
	bool remove() const;

	pid_t get_pid() const;

	static boost::filesystem::path get_default_rundir() {
		return "/var/run";
	}

	static std::string get_default_pidfile(const std::string &name) {
		return (get_default_rundir() / (name + ".pid")).string();
	}

private:
	boost::filesystem::path filepath_;
};
