#include "simple_scheduler.hpp"

#include <boost/bind.hpp>
#include <strEx.h>
#include <unicode_char.hpp>

using namespace nscp::helpers;

namespace scheduler {

	int simple_scheduler::add_task(target item) {
		{
			boost::mutex::scoped_lock l(mutex_);
			item.id = ++target_id_;
			targets_[item.id] = item;
		}
		reschedule(item);
		return item.id;
	}
	void simple_scheduler::remove_task(int id) {
		boost::mutex::scoped_lock l(mutex_);
		target_list_type::iterator it = targets_.find(id);
		targets_.erase(it);
	}
	boost::optional<target> simple_scheduler::get_task(int id) {
		boost::mutex::scoped_lock l(mutex_);
		target_list_type::iterator it = targets_.find(id);
 		if (it == targets_.end())
			return boost::optional<target>();
		return boost::optional<target>((*it).second);
	}

	void simple_scheduler::start() {
		running_ = true;
		start_thread();
	}
	void simple_scheduler::stop() {
		running_ = false;
		stop_requested_ = true;
		threads_.interrupt_all();
		threads_.join_all();
	}

	void simple_scheduler::start_thread() {
		if (!running_)
			return;
		stop_requested_ = false;
		int missing_threads = thread_count_ - threads_.size();
		if (missing_threads > 0 && missing_threads <= thread_count_) {
			for (int i=0;i<missing_threads;i++) {
				//std::wcout << _T("***START_THREAD: ") << threads_.size() << std::endl;
				threads_.create_thread(boost::bind(&simple_scheduler::thread_proc, this, i));
			}
		}
		threads_.create_thread(boost::bind(&simple_scheduler::watch_dog, this, 0));
		//thread_ = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&simple_scheduler::thread_proc, this)));
	}

	void simple_scheduler::watch_dog(int id) {

		schedule_queue_type::value_type instance;
		while(!stop_requested_) {
			instance = queue_.top();
			if (instance) {
				boost::posix_time::time_duration off = now() - (*instance).time;
				if (off.total_seconds() > error_threshold_) {
					log_error(_T("NOONE IS HANDLING scheduled item ") + to_wstring((*instance).schedule_id) + _T(" ") + to_wstring(off.total_seconds()) + _T(" seconds to late from thread ") + to_wstring(id));
				}
// 			} else {
// 				log_error(_T("Nothing is scheduled to run"));
			}

			// add support for checking queue length
			boost::thread::sleep(boost::get_system_time() + boost::posix_time::seconds(5));
		}

	}

	void simple_scheduler::thread_proc(int id) {
		try {
			int iteration = 0;
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
						log_error(_T("Ran scheduled item ") + to_wstring((*instance).schedule_id) + _T(" ") + to_wstring(off.total_seconds()) + _T(" seconds to late from thread ") + to_wstring(id));
					}
					boost::thread::sleep((*instance).time);
				} catch (boost::thread_interrupted  &e) {
					if (!queue_.push(*instance))
						log_error(_T("ERROR"));
					if (stop_requested_) {
						log_error(_T("Terminating thread: ") + to_wstring(id));
						return;
					}
					continue;
				} catch (...) {
					if (!queue_.push(*instance))
						log_error(_T("ERROR"));
					continue;
				}

				boost::posix_time::ptime now_time = now();
				boost::optional<target> item = get_task((*instance).schedule_id);
				if (item) {
					try {
						if (handler_)
							handler_->handle_schedule(*item);
						reschedule(*item,now_time);
					} catch (...) {
						log_error(_T("UNKNOWN ERROR RUNING TASK: "));
						reschedule(*item);
					}
				} else {
					log_error(_T("Task not found: ") + to_wstring((*instance).schedule_id));
				}
			}
		} catch (const std::exception &e) {
			log_error(_T("Exception in scheduler thread (thread will be killed): ") + utf8::to_unicode(e.what()));
		} catch (...) {
			log_error(_T("Exception in scheduler thread (thread will be killed)"));
		}

	}

	void simple_scheduler::reschedule(target item) {
		reschedule_wnext(item, now() + boost::posix_time::seconds(rand()%item.duration.total_seconds()));
	}
	void simple_scheduler::reschedule(target item, boost::posix_time::ptime now) {
		reschedule_wnext(item, now + item.duration);
	}
	void simple_scheduler::reschedule_wnext(target item, boost::posix_time::ptime next) {
		schedule_instance instance;
		instance.schedule_id = item.id;
		instance.time = next;
		if (!queue_.push(instance)) {
			log_error(_T("ERROR"));
		}
		idle_thread_cond_.notify_one();
	}


	void simple_scheduler::log_error(std::wstring err) {
		if (handler_)
			handler_->on_error(err);
	}

}



