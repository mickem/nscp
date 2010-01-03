#pragma once
#include <string>
#include <list>
#include <queue>
#include <boost/date_time.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/time_duration.hpp>
#include <boost/thread.hpp>
#include <boost/date_time/local_time/local_time.hpp>

namespace scheduler {

	class target {
	public:
		int id;
		std::wstring command;
		std::list<std::wstring> arguments;
		std::wstring tag;
		boost::posix_time::time_duration duration;

		target() {}
		target(const target &other) : id(other.id), command(other.command), duration(other.duration) {}
		target& operator=(target const& other) {
			command = other.command;
			duration = other.duration;
			id = other.id;
			return *this;
		}
		static target empty() {
			return target();
		}
	};
	struct schedule_instance {
		boost::posix_time::ptime time;
		int schedule_id;
		friend inline bool operator < (const schedule_instance& p1, const schedule_instance& p2) {
			return p1.time > p2.time;
		}
	};

	class simple_scheduler {
	private:
		typedef std::map<int,target> target_list_type;
		typedef std::priority_queue<schedule_instance> schedule_queue_type;
		target_list_type targets_;
		unsigned int target_id_;
		schedule_queue_type queue_;


		// thread variables
		volatile bool stop_requested_;
		boost::shared_ptr<boost::thread> thread_;
		boost::mutex mutex_;

	public:

		simple_scheduler() : target_id_(0) {}
		~simple_scheduler() {}


		int add_task(target item);
		void remove_task(int id);
		target get_task(int id);
		
		void start();
		void stop();


	private:
		void thread_proc();

		void reschedule(target item);
		void reschedule(target item, boost::posix_time::ptime now);
		void execute(target item);
		void start_thread();

		boost::posix_time::ptime now() {
			return boost::get_system_time();
		}
	};

}


