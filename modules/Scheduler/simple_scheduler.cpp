#include "simple_scheduler.hpp"

#include <boost/bind.hpp>

#include <unicode_char.hpp>

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
		if (!queue_.empty())
			start_thread();
	}
	void simple_scheduler::stop() {
		running_ = false;
		//if (!thread_)
		//	return;
		stop_requested_ = true;
		threads_.interrupt_all();
		threads_.join_all();
		/*
		if (!threads.join_all(boost::posix_time::seconds(5))) {
			std::wcout << _T("FAILED TO TERMINATE!!!") << std::endl;
		} else {
			std::wcout << _T("THREAD TERMINATED NICELY!") << std::endl;
		}
		*/
	}

	void simple_scheduler::start_thread() {
		if (!running_)
			return;
		stop_requested_ = false;
		int missing_threads = thread_count_ - threads_.size();
		if (missing_threads > 0 && missing_threads <= thread_count_) {
			for (int i=0;i<missing_threads;i++) {
				std::wcout << _T("***START_THREAD***") << std::endl;
				threads_.create_thread(boost::bind(&simple_scheduler::thread_proc, this));
			}
		}
		//thread_ = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&simple_scheduler::thread_proc, this)));
	}

	void simple_scheduler::thread_proc() {
		int iteration = 0;
		schedule_queue_type::value_type instance;
		while (!stop_requested_) {
			instance = queue_.pop();
			if (!instance)
				return;

			try {

				boost::posix_time::time_duration off = now() - (*instance).time;
				if (off.total_seconds() > 0) {
					std::wcout << _T("MISSED IT!") << off.total_seconds() << std::endl;
				}
				boost::thread::sleep((*instance).time);
			} catch (boost::thread_interrupted  &e) {
				if (!queue_.push(*instance))
					std::wcout << _T("ERROR") << std::endl;
				if (stop_requested_)
					return;
				continue;
			} catch (...) {
				if (!queue_.push(*instance))
					std::wcout << _T("ERROR") << std::endl;
				std::wcout << _T("ERROR!!!") << std::endl;
				return;
			}

			boost::posix_time::ptime now_time = now();
			boost::optional<target> item = get_task((*instance).schedule_id);
			if (item) {
				try {
					if (handler_)
						handler_->handle_schedule(*item);
					reschedule(*item,now_time);
				} catch (...) {
					std::wcout << _T("UNKNOWN ERROR RUNING TASK: ") << std::endl;
					reschedule(*item);
				}
			} else {
				std::wcout << _T("Task not found: ") << (*instance).schedule_id << std::endl;
			}
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
			std::wcout << _T("ERROR") << std::endl;
		}
		start_thread();
	}
}



