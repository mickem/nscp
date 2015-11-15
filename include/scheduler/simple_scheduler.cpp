#include <scheduler/simple_scheduler.hpp>

#include <boost/bind.hpp>
#include <nscp_string.hpp>
#include <utf8.hpp>

#include <nscapi/macros.hpp>


namespace simple_scheduler {


	void scheduler::start() {
		log_trace("starting all threads");
		running_ = true;
		start_threads();
		log_trace("Thread pool contains: " + strEx::s::xtos(threads_.threadCount()));
	}
	void scheduler::stop() {
		log_trace("stopping all threads");
		running_ = false;
		stop_requested_ = true;
		has_watchdog_ = false;
		threads_.interruptThreads();
		threads_.waitForThreads();
		log_trace("Thread pool contains: " + strEx::s::xtos(threads_.threadCount()));
	}

	int scheduler::add_task(std::string tag, boost::posix_time::time_duration duration) {
		task item(tag, duration);
		{
			boost::mutex::scoped_lock l(mutex_);
			item.id = ++schedule_id_;
			tasks_[item.id] = item;
		}
		reschedule(item);
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
		while (!stop_requested_) {
			try {
				try {
					instance = queue_.top();
					if (instance) {
						boost::posix_time::time_duration off = now() - (*instance).time;
						if (off.total_seconds() > 5) {
							if (thread_count_ < 10)
								thread_count_++;
							if (threads_.threadCount() > thread_count_) {
								log_error("Scheduler is overloading: " + strEx::s::xtos(instance->schedule_id) + " is " + strEx::s::xtos(off.total_seconds()) + " seconds slow");
							}
						}
					}
				} catch (const std::exception &e) {
					log_error("Watchdog issue: " + utf8::utf8_from_native(e.what()));
				} catch (...) {
					log_error("Watchdog issue");
				}

				boost::thread::sleep(boost::get_system_time() + boost::posix_time::seconds(5));
			} catch (const boost::thread_interrupted &e) {
				break;
			} catch (const std::exception &e) {
				log_error("Watchdog issue: " + utf8::utf8_from_native(e.what()));
				break;
			} catch (...) {
				log_error("Watchdog issue");
				break;
			}
		}
		log_trace("Terminating thread: " + strEx::s::xtos(id));
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
						log_error("Ran scheduled item " + strEx::s::xtos(instance->schedule_id) + " " + strEx::s::xtos(off.total_seconds()) + " seconds to late from thread " + strEx::s::xtos(id));
					}
					boost::thread::sleep((*instance).time);
				} catch (const boost::thread_interrupted &) {
					if (!queue_.push(*instance))
						log_error("Failed to push item");
					if (stop_requested_) {
						log_trace("Terminating thread: " + strEx::s::xtos(id));
						return;
					}
					continue;
				} catch (...) {
					if (!queue_.push(*instance))
						log_error("Failed to push item");
					continue;
				}

				boost::posix_time::ptime now_time = now();
				op_task_object item = get_task((*instance).schedule_id);
				if (item) {
					try {
						bool to_reschedule = false;
						if (handler_)
							to_reschedule = handler_->handle_schedule(*item);
						if (to_reschedule)
							reschedule_at(item->id, now_time);
						else
							log_trace("Abandoning: " + item->to_string());
					} catch (...) {
						log_error("UNKNOWN ERROR RUNING TASK: ");
						reschedule(*item);
					}
				} else {
					log_error("Task not found: " + strEx::s::xtos(instance->schedule_id));
				}
			}
		} catch (const boost::thread_interrupted &e) {
		} catch (const std::exception &e) {
			log_error("Exception in scheduler thread (thread will be killed): " + utf8::utf8_from_native(e.what()));
		} catch (...) {
			log_error("Exception in scheduler thread (thread will be killed)");
		}
		log_trace("Terminating thread: " + strEx::s::xtos(id));
	}



	void scheduler::reschedule(const task &item) {
		if (item.duration.total_seconds() == 0) {
			log_trace("Warning scheduling task now: " + item.to_string());
			reschedule_at(item.id, now() + boost::posix_time::seconds(0));
		} else
			reschedule_at(item.id, now() + boost::posix_time::seconds(rand() % item.duration.total_seconds()));
	}
	void scheduler::reschedule_at(const int id, boost::posix_time::ptime new_time) {
		schedule_instance instance;
		instance.schedule_id = id;
		instance.time = new_time;
		if (!queue_.push(instance)) {
			log_error("Failed to reschedule item");
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