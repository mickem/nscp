#pragma once

#include <boost/thread/mutex.hpp>

namespace nsclient {
	namespace logging {
		namespace impl {
			class raw_subscribers {
				typedef boost::shared_ptr<nsclient::logging::raw_subscriber> subscriber_type;
				typedef std::list<subscriber_type> subscribers_type;

				subscribers_type subscribers_;
				mutable boost::timed_mutex mutex_;

			public:
				void add(subscriber_type subscriber) {
					boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
					if (!lock.owns_lock())
						return;
					subscribers_.push_back(subscriber);
				}
				void clear() {
					boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
					if (!lock.owns_lock())
						return;
					subscribers_.clear();
				}

				void notify(std::string &data) {
					boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
					if (!lock.owns_lock())
						return;
					if (subscribers_.empty())
						return;
					BOOST_FOREACH(subscriber_type & s, subscribers_) {
						s->on_raw_log_message(data);
					}
				}
			};
		}
	}
}