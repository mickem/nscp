
#include "metrics_handler.hpp"

#include <boost/thread/locks.hpp>

void metrics_handler::set(const std::string &metrics) {
	boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock())
		return;
	metrics_ = metrics;
}
void metrics_handler::set_list(const std::string &metrics) {
	boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock())
		return;
	metrics_list_ = metrics;
}
std::string metrics_handler::get() {
	boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock())
		return "";
	return metrics_;
}

std::string metrics_handler::get_list() {
	boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock())
		return "";
	return metrics_list_;
}