#pragma once
#include <string>
#include <list>
#include <queue>
#include <boost/date_time.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/time_duration.hpp>
#include <boost/thread.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/optional.hpp>
#include <unicode_char.hpp>

namespace scheduler {


	class task_not_found {
		int id_;
	public:
		task_not_found(int id) : id_(id) {}
		int get_id() {return id_; }
	};
	class target {
	public:
		int id;
		std::wstring alias;

		std::wstring command;
		std::list<std::wstring> arguments;
		std::wstring tag;

		boost::posix_time::time_duration duration;
		std::wstring  channel;
		unsigned int report;

		void set_duration(boost::posix_time::time_duration duration_) {
			duration = duration_;
			// TODO!
		}

		target() : duration(boost::posix_time::minutes(0))
		{}
		
 		target(const target &other) : id(other.id), alias(other.alias)
			, command(other.command), arguments(other.arguments), tag(other.tag)
			, duration(other.duration), report(other.report), channel(other.channel) {}
		target& operator=(target const& other) {
			id = other.id;
			alias = other.alias;

			command = other.command;
			arguments = other.arguments;
			tag = other.tag;

			duration = other.duration;
			report = other.report;
			channel = other.channel;
			return *this;
		}
 		~target() {}
		std::wstring to_string() {
			std::wstringstream ss;
			ss << alias << _T("[") << id << _T("] = {command: ") << command << _T(", channel") << channel << _T("}");
			return ss.str();
		}
	};
	class schedule_handler {
	public:
		virtual void handle_schedule(target item) = 0;
	};
	struct schedule_instance {
		boost::posix_time::ptime time;
		int schedule_id;
		friend inline bool operator < (const schedule_instance& p1, const schedule_instance& p2) {
			return p1.time > p2.time;
		}
	};

	template<typename T>
	class safe_schedule_queue {
	public:
		typedef boost::optional<T> value_type;
	private:
		typedef std::priority_queue<T> schedule_queue_type;
		schedule_queue_type queue_;
		boost::shared_mutex mutex_;
	public:
		bool empty(unsigned int timeout = 5) {
			boost::shared_lock<boost::shared_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!lock.owns_lock()) 
				return false;
			return queue_.empty();
		}

		boost::optional<T> top(unsigned int timeout = 5) {
			boost::shared_lock<boost::shared_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!lock || queue_.empty())
				return boost::optional<T>();
			return boost::optional<T>(queue_.top());
		}

		boost::optional<T> pop(unsigned int timeout = 5) {
			boost::unique_lock<boost::shared_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(timeout));
			if (!lock || queue_.empty())
				return boost::optional<T>();
			boost::optional<T>  ret = queue_.top();
			queue_.pop();
			return ret;
		}

		bool push(T instance, unsigned int timeout = 5) {
			boost::unique_lock<boost::shared_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(timeout));
			if (!lock) {
				return false;
			}
			queue_.push(instance);
			return true;
		}
	};

	class simple_scheduler {
	private:
		typedef std::map<int,target> target_list_type;
		typedef safe_schedule_queue<schedule_instance> schedule_queue_type;
		target_list_type targets_;
		unsigned int target_id_;
		schedule_queue_type queue_;
		unsigned int thread_count_;


		// thread variables
		volatile bool stop_requested_;
		volatile bool running_;
		boost::thread_group threads_;
		//boost::shared_ptr<boost::thread> thread_;
		boost::mutex mutex_;
		schedule_handler* handler_;

	public:

		simple_scheduler() : target_id_(0), stop_requested_(false), running_(false), thread_count_(10), handler_(NULL) {}
		~simple_scheduler() {}


		void set_handler(schedule_handler* handler) {
			handler_ = handler;
		}
		void unset_handler() {
			handler_ = NULL;
		}

		int add_task(target item);
		void remove_task(int id);
		boost::optional<target> get_task(int id);
		
		void start();
		void stop();

		void set_threads(int threads) {
			thread_count_ = threads;
			start_thread();
		}


	private:
		void thread_proc();

		void reschedule(target item);
		void reschedule(target item, boost::posix_time::ptime now);
		void reschedule_wnext(target item, boost::posix_time::ptime next);
		void start_thread();

		inline boost::posix_time::ptime now() {
			return boost::get_system_time();
		}
	};

}


