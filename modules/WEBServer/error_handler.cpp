#include "error_handler.hpp"

#include <boost/foreach.hpp>

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
error_handler::log_list error_handler::get_messages(std::list<std::string> levels, std::size_t &position, std::size_t &ipp, std::size_t &count) {
	log_list ret;
	boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock())
		return ret;
	if (levels.empty()) {
		count = log_entries.size();
		if (position >= count) {
			return ret;
		}
		if ((position + ipp) >= count) {
			ipp = count - position;
		}
		log_list::iterator cit = log_entries.begin() + position;
		log_list::iterator end = log_entries.begin() + position + ipp;

		for (; cit != end; ++cit) {
			ret.push_back(*cit);
		}
	} else {
		int i = 0;
		BOOST_FOREACH(const log_entry &e, log_entries) {
			if (std::find(levels.begin(), levels.end(), e.type) == levels.end()) {
				continue;
			}
			i++;
			if (i < position) {
				continue;
			}
			if (i <= position + ipp) {
				ret.push_back(e);
			}
		}
		count = i;
	}
	return ret;
}

