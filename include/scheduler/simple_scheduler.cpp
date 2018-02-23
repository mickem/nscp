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

#include <scheduler/simple_scheduler.hpp>

#include <utf8.hpp>
#include <nscapi/macros.hpp>

#include <boost/bind.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

#if BOOST_VERSION >= 105300
#include <boost/interprocess/detail/atomic.hpp>
#endif

boost::posix_time::ptime time_t_epoch(boost::gregorian::date(1970, 1, 1));


namespace simple_scheduler {


#if BOOST_VERSION >= 105300
	volatile boost::uint32_t metric_executed = 0;
	volatile boost::uint32_t metric_compleated = 0;
	volatile boost::uint32_t metric_errors = 0;
	volatile boost::uint32_t metric_time = 0;
	volatile boost::uint32_t metric_count = 0;
	volatile boost::uint32_t metric_max_time = 0;
	volatile boost::uint32_t metric_start = 0;
	using namespace boost::interprocess::ipcdetail;
	inline void my_atomic_add(volatile boost::uint32_t *mem, boost::uint32_t value) {
		boost::uint32_t old, c(atomic_read32(mem));
		while ((old = atomic_cas32(mem, c + value, c)) != c) {
			c = old;
		}
	}
#else
	volatile int metric_executed = 0;
	volatile int metric_compleated = 0;
	volatile int metric_errors = 0;
	volatile long long metric_time = 0;
	volatile long long metric_count = 0;
	volatile long long metric_max_time = 0;
	volatile int metric_start = 0;
	int atomic_inc32(volatile int *i) { return 0;  }
	int atomic_read32(volatile int *i) { return 0; }
	void my_atomic_add(volatile int *i, int j) { }
#endif

	bool scheduler::has_metrics() const {
#if BOOST_VERSION >= 105300
		return true;
#else
		return false;
#endif

	}

	int scheduler::get_metric_executed() const {
		return atomic_read32(&metric_executed);
	}
	int scheduler::get_metric_compleated() const {
		return atomic_read32(&metric_compleated);
	}
	int scheduler::get_metric_errors() const {
		return atomic_read32(&metric_errors);
	}
	std::size_t scheduler::get_metric_threads() const {
		return thread_count_;
	}
	std::size_t scheduler::get_metric_ql() {
		return queue_.size();
	}
	int scheduler::get_avg_time() const {
		boost::uint32_t t = atomic_read32(&metric_time);
		boost::uint32_t c = atomic_read32(&metric_count);
		if (c == 0) {
			return 0;
		}
		if (t > 4000000000) {
			atomic_write32(&metric_time, 0);
			atomic_write32(&metric_count, 0);
		}
		return t / c;
	}

	int scheduler::get_metric_rate() const {
		boost::posix_time::time_duration diff = now() - time_t_epoch;
		boost::uint32_t total_time = diff.total_seconds() - metric_start;
		boost::uint32_t count = atomic_read32(&metric_compleated);
		if (total_time == 0) {
			return 0;
		}
		return count / total_time;
	}


	void scheduler::start() {
		boost::posix_time::time_duration diff = now() - time_t_epoch;
		metric_start = diff.total_seconds();
		log_trace(__FILE__, __LINE__, "starting all threads");
		running_ = true;
		start_threads();
		log_trace(__FILE__, __LINE__, "Thread pool contains: " + str::xtos(threads_.threadCount()));
	}

	void scheduler::prepare_shutdown() {
		log_trace(__FILE__, __LINE__, "prepare to shutdown");
		running_ = false;
		stop_requested_ = true;
		has_watchdog_ = false;
		threads_.interruptThreads();
	}
	void scheduler::stop() {
		log_trace(__FILE__, __LINE__, "stopping all threads");
		running_ = false;
		stop_requested_ = true;
		has_watchdog_ = false;
		threads_.interruptThreads();
		threads_.waitForThreads();
		log_trace(__FILE__, __LINE__, "Thread pool contains: " + str::xtos(threads_.threadCount()));
	}

	int scheduler::add_task(std::string tag, boost::posix_time::time_duration duration, double randomness) {
		task item(tag, duration, randomness);
		{
			boost::mutex::scoped_lock l(mutex_);
			item.id = ++schedule_id_;
			tasks_[item.id] = item;
		}
		reschedule(item, now());
		return item.id;
	}
	int scheduler::add_task(std::string tag, cron_parser::schedule schedule) {
		task item(tag, schedule);
		{
			boost::mutex::scoped_lock l(mutex_);
			item.id = ++schedule_id_;
			tasks_[item.id] = item;
		}
		reschedule(item, now());
		return item.id;
	}
	void scheduler::remove_task(int id) {
		boost::mutex::scoped_lock l(mutex_);
		tasks_list_type::iterator it = tasks_.find(id);
		tasks_.erase(it);
	}
	scheduler::op_task_object scheduler::get_task(int id) {
		boost::mutex::scoped_lock l(mutex_);
		tasks_list_type::iterator it = tasks_.find(id);
		if (it == tasks_.end())
			return op_task_object();
		return op_task_object((*it).second);
	}

	void scheduler::clear_tasks() {
		boost::mutex::scoped_lock l(mutex_);
		tasks_.clear();
	}



	void scheduler::watch_dog(int id) {
		schedule_queue_type::value_type instance;
		bool maximum_threads_reached = false;
		while (!stop_requested_) {
			try {
				try {
					instance = queue_.top();
					if (instance) {
						boost::posix_time::time_duration off = now() - (*instance).time;
						if (off.total_seconds() > 5) {
							if (thread_count_ < 10) {
								thread_count_++;
							}  else if (!maximum_threads_reached) {
								log_error(__FILE__, __LINE__, "Auto-scaling of scheduler failed (maximum of 10 threads reached) you need to manually configure threads to resolve items running slow");
								maximum_threads_reached = true;
							}
						}
					}
				} catch (const std::exception &e) {
					log_error(__FILE__, __LINE__, "Watchdog issue: " + utf8::utf8_from_native(e.what()));
				} catch (...) {
					log_error(__FILE__, __LINE__, "Watchdog issue");
				}

				boost::thread::sleep(boost::get_system_time() + boost::posix_time::seconds(5));
			} catch (const boost::thread_interrupted &e) {
				break;
			} catch (const std::exception &e) {
				log_error(__FILE__, __LINE__, "Watchdog issue: " + utf8::utf8_from_native(e.what()));
				break;
			} catch (...) {
				log_error(__FILE__, __LINE__, "Watchdog issue");
				break;
			}
		}
		log_trace(__FILE__, __LINE__, "Terminating thread: " + str::xtos(id));
	}

	void scheduler::thread_proc(int id) {
		try {
			schedule_queue_type::value_type instance;
			while (!stop_requested_) {
				instance = queue_.pop();
				if (!instance) {
					boost::unique_lock<boost::mutex> lock(idle_thread_mutex_);
					idle_thread_cond_.wait(lock);
					continue;
				}

				try {
					boost::posix_time::time_duration off = now() - (*instance).time;
					if (off.total_seconds() > error_threshold_) {
						log_error(__FILE__, __LINE__, "Ran scheduled item " + str::xtos(instance->schedule_id) + " " + str::xtos(off.total_seconds()) + " seconds to late from thread " + str::xtos(id));
					}
					boost::thread::sleep((*instance).time);
				} catch (const boost::thread_interrupted &) {
					if (!queue_.push(*instance))
						log_error(__FILE__, __LINE__, "Failed to push item");
					if (stop_requested_) {
						log_trace(__FILE__, __LINE__, "Terminating thread: " + str::xtos(id));
						return;
					}
					continue;
				} catch (...) {
					if (!queue_.push(*instance)) {
						atomic_inc32(&metric_errors);
						log_error(__FILE__, __LINE__, "Failed to push item");
					}
					continue;
				}

				boost::posix_time::ptime now_time = now();
				atomic_inc32(&metric_executed);
				op_task_object item = get_task((*instance).schedule_id);
				if (item) {
					try {
						bool to_reschedule = false;
						if (handler_) {
							to_reschedule = handler_->handle_schedule(*item);
						}
						boost::posix_time::time_duration duration = now() - now_time;

						my_atomic_add(&metric_time, duration.total_milliseconds());
						atomic_inc32(&metric_count);
						if (to_reschedule) {
							reschedule(*item, now_time);
							atomic_inc32(&metric_compleated);
						} else {
							atomic_inc32(&metric_errors);
							log_trace(__FILE__, __LINE__, "Abandoning: " + item->to_string());
						}
					} catch (...) {
						atomic_inc32(&metric_errors);
						log_error(__FILE__, __LINE__, "UNKNOWN ERROR RUNING TASK: " + item->tag);
						reschedule(*item, now_time);
					}
				} else {
					atomic_inc32(&metric_errors);
					log_error(__FILE__, __LINE__, "Task not found: " + str::xtos(instance->schedule_id));
				}
			}
		} catch (const boost::thread_interrupted &e) {
		} catch (const std::exception &e) {
			atomic_inc32(&metric_errors);
			log_error(__FILE__, __LINE__, "Exception in scheduler thread (thread will be killed): " + utf8::utf8_from_native(e.what()));
		} catch (...) {
			atomic_inc32(&metric_errors);
			log_error(__FILE__, __LINE__, "Exception in scheduler thread (thread will be killed)");
		}
		log_trace(__FILE__, __LINE__, "Terminating thread: " + str::xtos(id));
	}



	void scheduler::reschedule(const task &item, boost::posix_time::ptime now_time) {
		if (item.is_disabled()) {
			log_error(__FILE__, __LINE__, "Found disabled task: " + item.to_string());
		} else {
			reschedule_at(item.id, item.get_next(now_time));
		}
	}
	void scheduler::reschedule_at(const int id, boost::posix_time::ptime new_time) {
		schedule_instance instance;
		instance.schedule_id = id;
		instance.time = new_time;
		if (!queue_.push(instance)) {
			log_error(__FILE__, __LINE__, "Failed to reschedule item");
		}
		idle_thread_cond_.notify_one();
	}

	void scheduler::start_threads() {
		if (!running_)
			return;
		stop_requested_ = false;
		std::size_t missing_threads = 0;
		if (thread_count_ > threads_.threadCount())
			missing_threads = thread_count_ - threads_.threadCount();
		if (missing_threads > 0 && missing_threads <= thread_count_) {
			for (std::size_t i = 0; i < missing_threads; i++) {
				boost::function<void()> f = boost::bind(&scheduler::thread_proc, this, 100 + i);
				threads_.createThread(f);
			}
		}
		if (!has_watchdog_) {
			has_watchdog_ = true;
			boost::function<void()> f = boost::bind(&scheduler::watch_dog, this, 0);
			threads_.createThread(f);
		}
	}
}