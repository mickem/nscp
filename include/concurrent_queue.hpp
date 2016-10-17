/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

		if (!queue_.empty()) {
			popped_value=queue_.front();
			queue_.pop();
		}
	}

};