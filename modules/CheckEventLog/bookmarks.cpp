#include "bookmarks.hpp"


void bookmarks::add(const std::string key, const std::string bookmark) {
	boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock()) {
		return;
	}
	bookmarks_[key] = bookmark;
}

bookmarks::op_string bookmarks::get(const std::string key) {
	boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock()) {
		return op_string();
	}
	map_type::const_iterator cit = bookmarks_.find(key);
	if (cit == bookmarks_.end()) {
		return op_string();
	}
	return cit->second;
}

bookmarks::map_type bookmarks::get_copy() {
	bookmarks::map_type ret;
	boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock()) {
		return ret;
	}
	ret.insert(bookmarks_.begin(), bookmarks_.end());
	return ret;
}
