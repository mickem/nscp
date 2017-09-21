#include "error_handler.hpp"



void error_handler::add_message(bool is_error, const log_entry &message) {
	{
		boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
		if (!lock.owns_lock())
			return;
		log_entries.push_back(message);
		if (is_error) {
			error_count_++;
			last_error_ = message.message;
		}
	}
}
void error_handler::reset() {
	boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock())
		return;
	log_entries.clear();
	last_error_ = "";
	error_count_ = 0;
}
error_handler::status error_handler::get_status() {
	status ret;
	boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock())
		return ret;
	ret.error_count = error_count_;
	ret.last_error = last_error_;
	return ret;
}
error_handler::log_list error_handler::get_errors(std::size_t &position) {
	log_list ret;
	boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock())
		return ret;
	if (position >= log_entries.size())
		return ret;
	log_list::iterator cit = log_entries.begin() + position;
	log_list::iterator end = log_entries.end();

	for (; cit != end; ++cit) {
		ret.push_back(*cit);
		position++;
	}
	return ret;
}

