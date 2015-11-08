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
#include <boost/unordered_map.hpp>
#include <boost/thread/shared_mutex.hpp>


#include <has-threads.hpp>

namespace task_scheduler {
	struct scheduled_task {
		int id;
		std::string tag;
		boost::posix_time::time_duration duration;

		scheduled_task() : duration(boost::posix_time::seconds(0)) {}
		scheduled_task(std::string tag, boost::posix_time::time_duration duration) : tag(tag), duration(duration) {}

		std::string to_string() const {
			std::stringstream ss;
			ss << id << "[" << tag << "] = " << duration.total_seconds();
			return ss.str();
		}
	};

	class schedule_handler {
	public:
		virtual bool handle_schedule(scheduled_task item) = 0;
		virtual void on_error(std::string error) = 0;
		virtual void on_trace(std::string error) = 0;
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
			boost::shared_lock<boost::shared_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(timeout));
			if (!lock.owns_lock())
				return false;
			return queue_.empty();
		}

		boost::optional<T> top(unsigned int timeout = 5) {
			boost::shared_lock<boost::shared_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(timeout));
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

	class simple_scheduler : public boost::noncopyable {
	private:
		typedef scheduled_task task_object;
		typedef boost::unordered_map<int, task_object> target_list_type;
		typedef boost::optional<task_object> op_task_object;
		typedef safe_schedule_queue<schedule_instance> schedule_queue_type;

		// thread variables
		target_list_type tasks;
		unsigned int schedule_id_;
		volatile bool stop_requested_;
		volatile bool running_;
		std::size_t thread_count_;
		schedule_handler* handler_;

		has_threads threads_;
		boost::mutex mutex_;
		schedule_queue_type queue_;
		boost::mutex idle_thread_mutex_;
		boost::condition_variable idle_thread_cond_;

	public:

		simple_scheduler() : schedule_id_(0), stop_requested_(false), running_(false), thread_count_(1), handler_(NULL) {}
		~simple_scheduler() {}

		void set_handler(schedule_handler* handler) {
			handler_ = handler;
		}
		void unset_handler() {
			handler_ = NULL;
		}

		boost::mutex& get_mutex() {
			return mutex_;
		}

		int add_task(std::string tag, boost::posix_time::time_duration duration);
		int add_task(task_object item);
		void remove_task(int id);
		op_task_object get_task(int id);

		void start();
		void stop();

		void set_threads(int threads) {
			thread_count_ = threads;
			start_thread();
		}
		int get_threads() const { return thread_count_; }

	private:
		void thread_proc(int id);
		void watch_dog(int id);

		void reschedule(const task_object &item);
		void reschedule(const task_object &item, boost::posix_time::ptime now);
		void reschedule_wnext(int id, boost::posix_time::ptime next);
		void start_thread();

		void log_error(std::string err);
		void log_trace(std::string err);

		inline boost::posix_time::ptime now() {
			return boost::get_system_time();
		}
	};
}