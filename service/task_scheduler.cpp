#include "task_scheduler.hpp"

#include <boost/bind.hpp>
#include <nscp_string.hpp>
#include <utf8.hpp>

namespace task_scheduler {
	int simple_scheduler::add_task(std::string tag, boost::posix_time::time_duration duration) {
		return add_task(task_object(tag, duration));
	}

	int simple_scheduler::add_task(task_object item) {
		{
			boost::mutex::scoped_lock l(mutex_);
			item.id = ++schedule_id_;
			tasks[item.id] = item;
		}
		reschedule(item);
		return item.id;
	}
	void simple_scheduler::remove_task(int id) {
		boost::mutex::scoped_lock l(mutex_);
		target_list_type::iterator it = tasks.find(id);
		tasks.erase(it);
	}
	simple_scheduler::op_task_object simple_scheduler::get_task(int id) {
		boost::mutex::scoped_lock l(mutex_);
		target_list_type::iterator it = tasks.find(id);
		if (it == tasks.end())
			return op_task_object();
		return op_task_object((*it).second);
	}

	void simple_scheduler::start() {
		log_trace("starting all threads");
		running_ = true;
		start_thread();
		log_trace("Thread pool contains: " + strEx::s::xtos(threads_.threadCount()));
	}
	void simple_scheduler::stop() {
		log_trace("stopping all threads");
		running_ = false;
		stop_requested_ = true;
		threads_.interruptThreads();
		threads_.waitForThreads();
		log_trace("Thread pool contains: " + strEx::s::xtos(threads_.threadCount()));
	}

	void simple_scheduler::start_thread() {
		if (!running_)
			return;
		stop_requested_ = false;
		std::size_t missing_threads = 0;
		if (thread_count_ > threads_.threadCount())
			missing_threads = thread_count_ - threads_.threadCount();
		if (missing_threads > 0 && missing_threads <= thread_count_) {
			for (std::size_t i = 0; i < missing_threads; i++) {
				boost::function<void()> f = boost::bind(&simple_scheduler::thread_proc, this, 100 + i);
				threads_.createThread(f);
			}
		}
		boost::function<void()> f = boost::bind(&simple_scheduler::watch_dog, this, 0);
		threads_.createThread(f);
	}

	void simple_scheduler::watch_dog(int id) {
		schedule_queue_type::value_type instance;
		while (!stop_requested_) {
			try {
				instance = queue_.top();
				if (instance) {
					boost::posix_time::time_duration off = now() - (*instance).time;
					if (off.total_seconds() > 5) {
						if (thread_count_ < 10)
							thread_count_++;
						if (threads_.threadCount() > thread_count_) {
							log_error("Scheduler is overloading: " + strEx::s::xtos(instance->schedule_id) + " is " + strEx::s::xtos(off.total_seconds()) + " seconds slow");
						} else {
							std::size_t missing_threads = thread_count_ - threads_.threadCount();
							if (missing_threads > 0) {
								start_thread();
							} else {
								log_error("Scheduler is overloading: " + strEx::s::xtos(instance->schedule_id) + " is " + strEx::s::xtos(off.total_seconds()) + " seconds slow");
							}
						}
					}
				}
			} catch (const std::exception &e) {
				log_error("Watchdog issue: " + utf8::utf8_from_native(e.what()));
			} catch (...) {
				log_error("Watchdog issue");
			}
			boost::thread::sleep(boost::get_system_time() + boost::posix_time::seconds(5));
		}
		log_trace("Terminating thread: " + strEx::s::xtos(id));
	}

	void simple_scheduler::thread_proc(int id) {
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
							reschedule(*item, now_time);
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
		} catch (const std::exception &e) {
			log_error("Exception in scheduler thread (thread will be killed): " + utf8::utf8_from_native(e.what()));
		} catch (...) {
			log_error("Exception in scheduler thread (thread will be killed)");
		}
		log_trace("Terminating thread: " + strEx::s::xtos(id));
	}

	void simple_scheduler::reschedule(const task_object &item) {
		if (item.duration.total_seconds() == 0) {
			log_trace("Warning scheduling task now: " + item.to_string());
			reschedule_wnext(item.id, now() + boost::posix_time::seconds(0));
		} else 
			reschedule_wnext(item.id, now() + boost::posix_time::seconds(rand() % item.duration.total_seconds()));
	}
	void simple_scheduler::reschedule(const task_object &item, boost::posix_time::ptime now) {
		reschedule_wnext(item.id, now + item.duration);
	}
	void simple_scheduler::reschedule_wnext(int id, boost::posix_time::ptime next) {
		schedule_instance instance;
		instance.schedule_id = id;
		instance.time = next;
		if (!queue_.push(instance)) {
			log_error("Failed to reschedule item");
		}
		idle_thread_cond_.notify_one();
	}

	void simple_scheduler::log_error(std::string err) {
		if (handler_)
			handler_->on_error(err);
	}
	void simple_scheduler::log_trace(std::string err) {
		if (handler_)
			handler_->on_trace(err);
	}
}