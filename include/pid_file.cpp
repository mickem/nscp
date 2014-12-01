#include <pid_file.hpp>

#include <fstream>
#include <boost/filesystem/operations.hpp>

#ifdef WIN32
#include <process.h>
#endif
pidfile::pidfile(boost::filesystem::path const & rundir, std::string const & process_name)
	: filepath_((rundir.empty() ? get_default_rundir() : rundir) / (process_name + ".pid")) {}

pidfile::pidfile(boost::filesystem::path const & filepath) : filepath_(filepath) {}

pidfile::~pidfile() {
	try {
		remove();
	} catch(...) {}
}

bool pidfile::create(pid_t const pid) const {
	remove();

	std::ofstream out(filepath_.string().c_str());
	out << pid;
	return out.good();
}

bool pidfile::create() const {
	return create(get_pid());
}

pid_t pidfile::get_pid() const {
#ifdef WIN32
	return ::_getpid();
#else
	return ::getpid();
#endif
}

bool pidfile::remove() const {
	return boost::filesystem::remove(filepath_);
}


