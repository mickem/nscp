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
#include <boost/function.hpp>

#include <has-threads.hpp>

#include <parsers/cron/cron_parser.hpp>

namespace simple_scheduler {

	struct task {
		int id;
		std::string tag;
	private:
		boost::posix_time::time_duration duration;
		cron_parser::schedule schedule;
		bool has_duration;
		bool has_schedule;
		double randomeness;

	public:
		task() : id(0), duration(boost::posix_time::seconds(0)), has_duration(false), has_schedule(false), randomeness(0.0) {}
		task(std::string tag, boost::posix_time::time_duration duration, double randomeness) : id(0), tag(tag), duration(duration), has_duration(true), has_schedule(false), randomeness(randomeness) {}
		task(std::string tag, cron_parser::schedule schedule) : id(0), tag(tag), schedule(schedule), has_duration(false), has_schedule(true), randomeness(0.0) {}

		bool is_disabled() const {
			return !has_duration && !has_schedule;
		}
		std::string to_string() const {
			std::stringstream ss;
			ss << id << "[" << tag << "] = ";
			if (has_duration)
				ss << duration.total_seconds() << " " << (randomeness*100) << "% randomness";
			else if (has_schedule)
				ss << schedule.to_string();
			else
				ss << "disabled";
			return ss.str();
		}
		boost::posix_time::ptime get_next(boost::posix_time::ptime now_time) const {
			if (has_duration && duration.total_seconds() > 0) {
				double total_delay = duration.total_seconds();
				double val = (total_delay * randomeness) * (static_cast<double>(rand()) / static_cast<double>(RAND_MAX));
				double time_to_wait = (total_delay * (1.0 - randomeness)) + val;
				if (time_to_wait < 1.0) {
					time_to_wait = 1.0;
				}
				return now_time + boost::posix_time::seconds(time_to_wait);
			} else if (has_duration) {
				return now_time;
			}
			return schedule.find_next(now_time);
		}
	};


	class handler {
	public:
		virtual bool handle_schedule(task item) = 0;
		virtual void on_error(const char* file, int line, std::string error) = 0;
		virtual void on_trace(const char* file, int line, std::string error) = 0;
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

		std::size_t size(unsigned int timeout = 5) {
			boost::shared_lock<boost::shared_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(timeout));
			if (!lock || queue_.empty())
				return 0;
			return queue_.size();
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

	class scheduler : public boost::noncopyable {
	private:
		typedef boost::unordered_map<int, task> tasks_list_type;
		typedef boost::optional<task> op_task_object;
		typedef safe_schedule_queue<schedule_instance> schedule_queue_type;

		// thread variables
		unsigned int schedule_id_;
		volatile bool stop_requested_;
		volatile bool running_;
		volatile bool has_watchdog_;
		std::size_t thread_count_;
		handler* handler_;
		int error_threshold_;

		has_threads threads_;
		boost::mutex mutex_;
		tasks_list_type tasks_;
		schedule_queue_type queue_;
		boost::mutex idle_thread_mutex_;
		boost::condition_variable idle_thread_cond_;
	public:

		scheduler() : schedule_id_(0), stop_requested_(false), running_(false), has_watchdog_(false), thread_count_(10), handler_(NULL), error_threshold_(5) {}
		~scheduler() {}

		void set_handler(handler* handler) {
			handler_ = handler;
		}
		void unset_handler() {
			handler_ = NULL;
		}

		boost::mutex& get_mutex() {
			return mutex_;
		}

		int get_metric_executed() const;
		int get_metric_compleated() const;
		int get_metric_errors() const;
		int get_avg_time() const;
		int get_metric_rate() const;
		std::size_t get_metric_threads() const;
		std::size_t get_metric_ql();
		bool has_metrics() const;

		int add_task(std::string tag, boost::posix_time::time_duration duration, double randomness);
		int add_task(std::string tag, cron_parser::schedule schedule);
		void remove_task(int id);
		op_task_object get_task(int id);
		void clear_tasks();

		void start();
		void stop();
		void prepare_shutdown();


		void set_threads(int threads) {
			thread_count_ = threads;
			start_threads();
		}
		int get_threads() const { return thread_count_; }

	private:

		void watch_dog(int id);
		void thread_proc(int id);


		void reschedule(const task &item, boost::posix_time::ptime now_time);
		void reschedule_at(const int id, boost::posix_time::ptime new_time);
		void start_threads();

		void log_error(const char* file, int line, std::string err) {
			if (handler_)
				handler_->on_error(file, line, err);
		}
		void log_trace(const char* file, int line, std::string err) {
			if (handler_)
				handler_->on_trace(file, line, err);
		}

		inline boost::posix_time::ptime now() const {
			return boost::get_system_time();
		}
	};
}