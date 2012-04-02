#pragma once

#include <queue>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/shared_mutex.hpp>

template<typename T>
class concurrent_queue {
private:
	std::queue<T> queue_;
	mutable boost::mutex mutex_;
	boost::condition_variable condition_;
public:
	void push(T const& data) {
		{
			boost::mutex::scoped_lock lock(mutex_);
			queue_.push(data);
		}
		condition_.notify_one();
	}

	bool empty() const {
		boost::mutex::scoped_lock lock(mutex_);
		return queue_.empty();
	}

	bool try_pop(T& popped_value) {
		boost::mutex::scoped_lock lock(mutex_);
		if(queue_.empty()) {
			return false;
		}

		popped_value=queue_.front();
		queue_.pop();
		return true;
	}

	void wait_and_pop(T& popped_value) {
		boost::mutex::scoped_lock lock(mutex_);
		while(queue_.empty()) {
			condition_.wait(lock);
		}

		popped_value=queue_.front();
		queue_.pop();
	}

};