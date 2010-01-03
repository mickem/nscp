#include "simple_scheduler.hpp"
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
	target simple_scheduler::get_task(int id) {
		boost::mutex::scoped_lock l(mutex_);
		target_list_type::iterator it = targets_.find(id);
		if (it == targets_.end())
			return target::empty();
		return (*it).second;
	}

	void simple_scheduler::start() {
		if (!queue_.empty())
			start_thread();
	}
	void simple_scheduler::stop() {
		if (thread_)
			return;
		stop_requested_ = true;
		thread_->join();
	}

	void simple_scheduler::start_thread() {
		stop_requested_ = false;
		thread_ = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&simple_scheduler::thread_proc, this)));
	}

	void simple_scheduler::thread_proc() {
		int iteration = 0;
		schedule_instance instance;
		while (!stop_requested_) {
			{
				boost::mutex::scoped_lock l(mutex_);
				//std::wcout << _T("#### COUNT: ") << queue_.size() << _T(" ####") << std::endl;
				if (queue_.empty())
					return;
				instance = queue_.top();
				queue_.pop();
			}
			target item = get_task(instance.schedule_id);
			//boost::posix_time::ptime delay = now() + instance.time;
			
			try {
				boost::thread::sleep(instance.time);
			} catch (...) {
				std::wcout << _T("Excepting...") << std::endl;
				return;
			}
			boost::posix_time::ptime ctime = now();
			execute(item);
			reschedule(item,ctime);
		}
	}

	void simple_scheduler::reschedule(target item) {
		reschedule(item, now());
	}
	void simple_scheduler::reschedule(target item, boost::posix_time::ptime now) {
		boost::mutex::scoped_lock l(mutex_);
		schedule_instance instance;
		instance.schedule_id = item.id;
		instance.time = now + item.duration;
		queue_.push(instance);
		if (!thread_)
			start_thread();
	}
	void simple_scheduler::execute(target item) {
		std::wcout << _T("Running: ") << item.command << std::endl;
	}


}



