/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
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