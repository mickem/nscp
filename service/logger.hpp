#pragma once

#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/thread/thread_time.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>

#include <protobuf/plugin.pb.h>

#include <strEx.h>

#include "NSCPlugin.h"
#include <nscapi/functions.hpp>

using namespace nscp::helpers;

namespace nsclient {
	class logger {
	public:
		virtual void nsclient_log_error(std::string file, int line, std::wstring error) = 0;


	};


	class logger_helper {
	public:
		static inline std::string create_debug(const char* file, const int line, std::wstring message) {
			return create_message(Plugin::LogEntry_Entry_Level_LOG_DEBUG, file, line, message);
		}
		static inline std::string create_info(const char* file, const int line, std::wstring message) {
			return create_message(Plugin::LogEntry_Entry_Level_LOG_INFO, file, line, message);
		}
		static inline std::string create_error(const char* file, const int line, std::wstring message) {
			return create_message(Plugin::LogEntry_Entry_Level_LOG_ERROR, file, line, message);
		}
		static inline std::string create_error(const std::string file, const int line, std::wstring message) {
			return create_message(Plugin::LogEntry_Entry_Level_LOG_ERROR, file.c_str(), line, message);
		}
		static inline std::string create_warning(const char* file, const int line, std::wstring message) {
			return create_message(Plugin::LogEntry_Entry_Level_LOG_WARNING, file, line, message);
		}

		static std::string create_message(Plugin::LogEntry::Entry::Level msgType, const char* file, const int line, std::wstring logMessage) {
			std::string str;
			try {
				Plugin::LogEntry message;
				Plugin::LogEntry::Entry *msg = message.add_entry();
				msg->set_level(msgType);
				msg->set_file(file);
				msg->set_line(line);
				msg->set_message(utf8::cvt<std::string>(logMessage));
				if (!message.SerializeToString(&str)) {
					return "Failed to generate message";
				}
				return str;
			} catch (std::exception &e) {
				return std::string("Failed to generate message: ") + e.what();
			} catch (...) {
				return "Failed to generate message";
			}
// 			return to_string(logMessage);
		}
	};
	namespace logging_queue {

		namespace ip = boost::interprocess;
		const int max_message_size = 40960;
		const std::string queue_name = "logging_queue";



		class slave {
		public:
			typedef boost::shared_ptr<NSCPlugin> plugin_type;
		private:
			ip::message_queue mq_;
			boost::shared_ptr<boost::thread> thread_;
			std::list<plugin_type> plugins_;
			boost::timed_mutex mutex_;
			bool console_log_;
			bool plugins_loaded_;
			typedef std::list<std::string> log_cache_type;
			log_cache_type cache_;


		public:
			slave() : mq_(ip::open_only,queue_name.c_str()), plugins_loaded_(false), console_log_(false) {
#ifdef WIN32
// 				if(!SetConsoleOutputCP(CP_UTF8)) { // 65001
// 					std::cerr << "Failed to set console output mode!\n";
// 				}
#endif
			}

			void add_plugin(plugin_type plugin) {
				boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
				if (!lock.owns_lock()) {
					log_fatal_error("Failed to add logging plugin");
				}
				plugins_.push_back(plugin);
			}

			void remove_all_plugins() {
				boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
				if (!lock.owns_lock()) {
					log_fatal_error("Failed to remove all logger plugins");
				}
				plugins_loaded_ = false;
				plugins_.clear();
			}
			void all_plugins_loaded() {
				plugins_loaded_ = true;
			}
			void set_console_log() {
				console_log_ = true;
			}

			~slave() {
				stop();
			}

			void start() {
				thread_ = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&slave::run, this)));
			}

			void stop() {
				remove_all_plugins();
				if (thread_)
					thread_->join();
			}

			void run() {
				while (true) {
					std::string buffer;
					buffer.resize(max_message_size);
					size_t msg_size;
					unsigned msg_prio;

					mq_.receive(&buffer[0], buffer.size(), msg_size, msg_prio);
					buffer.resize(msg_size);

					if (buffer == "$$QUIT$$")
						return;

					forward_message(buffer);
				}
			}

			std::wstring render_log_level(Plugin::LogEntry::Entry::Level l) {
				int code = nscapi::functions::gpb_to_log(l);
				switch (code) {
					case NSCAPI::critical:
						return _T("c");
					case NSCAPI::warning:
						return _T("w");
					case NSCAPI::error:
						return _T("e");
					case NSCAPI::log:
						return _T("l");
					case NSCAPI::debug:
						return _T("d");
					default:
						return _T("?");
				}
			}
			std::wstring rpad(std::wstring str, int len) {
				if (str.length() > len)
					return str.substr(str.length()-len);
				return std::wstring(len-str.length(), L' ') + str;
			}
			std::wstring lpad(std::wstring str, int len) {
				if (str.length() > len)
					return str.substr(0, len);
				return str + std::wstring(len-str.length(), L' ');
			}

			std::wstring render_console_message(std::string data) {
				std::wstring str;
				try {
					Plugin::LogEntry message;
					if (!message.ParseFromString(data)) {
						return _T("Failed to parse message: ") + to_wstring(strEx::strip_hex(data));
					}

					for (int i=0;i<message.entry_size();i++) {
						Plugin::LogEntry::Entry msg = message.entry(i);
						if (!str.empty())
							str += _T(" -- ");
						str += render_log_level(msg.level()) + _T(" ") + rpad(to_wstring(msg.file()), 40) + _T(":") + lpad(to_wstring(msg.line()),4) + _T(" ") + to_wstring(msg.message());
					}
					return str;
				} catch (std::exception &e) {
					return _T("Failed to parse data from: ") + to_wstring(strEx::strip_hex(data)) + _T(": ") + to_wstring(e.what());
				} catch (...) {
					return _T("Failed to parse data from: ") + to_wstring(strEx::strip_hex(data));
				}
				return to_wstring(data);
			}

			void forward_message(std::string buffer) {
				if (console_log_) {
					std::wstring s = render_console_message(buffer);
					strEx::replace(s, _T("\n"), _T(""));
					strEx::replace(s, _T("\r"), _T(""));

#ifdef WIN32
					s += _T("\n");
					HANDLE const consout = GetStdHandle(STD_OUTPUT_HANDLE);
					DWORD nNumberOfCharsWritten;
					if(!WriteConsole(consout, s.c_str(), s.length(), &nNumberOfCharsWritten, NULL)) {
						DWORD const err = GetLastError();
						cerr << "WriteConsole failed with << " << err << "!\n";
					}
#else
					std::wcout << s << std::endl;
#endif
				}

				boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
				if (!lock.owns_lock()) {
					log_fatal_error("Failed to send message: " + buffer);
					return;
				}
				if (!plugins_loaded_) {
					cache_.push_back(buffer);
					return;
				}
				if (cache_.size() > 0) {
					BOOST_FOREACH(std::string s, cache_) {
						const char* cache_buf = s.c_str();
						BOOST_FOREACH(plugin_type p, plugins_) {
							p->handleMessage(cache_buf, s.length());
						}
					}
					cache_.clear();
				}
				const char* buf = buffer.c_str();
				unsigned int len = buffer.length();
				BOOST_FOREACH(plugin_type p, plugins_) {
					p->handleMessage(buf, len);
				}
			}

			void log_fatal_error(std::string message) {
#ifdef WIN32
				OutputDebugString(to_wstring(message).c_str());
#else
				if (!console_log_)
					std::cout << "BROKEN MESSAGE: " << message << std::endl;
#endif
			}

		};
		class master {

			boost::shared_ptr<ip::message_queue> mq_;
			boost::shared_ptr<slave> slave_;
		public:

			master() {
				try {
					ip::message_queue::remove(queue_name.c_str());

					mq_.reset(new ip::message_queue(ip::create_only,queue_name.c_str(),100,max_message_size));

				} catch(ip::interprocess_exception &e) {
					log_fatal_error(std::string("Failed to create logging queue: ") + e.what());
				}

			}
			~master() {
				stop_slave();
				mq_.reset();
				destroy();
			}

			void add_plugin(slave::plugin_type plugin) {
				slave_->add_plugin(plugin);
			}
			void remove_all_plugins() {
				slave_->remove_all_plugins();
			}
			void all_plugins_loaded() {
				slave_->all_plugins_loaded();
			}
			void set_console_log() {
				slave_->set_console_log();
			}

			void stop_slave() {
				if (slave_) {
					log("$$QUIT$$");
					slave_->stop();
					slave_.reset();
				}
			}

			void start_slave() {
				slave_.reset(new slave());
				slave_->start();
			}

			void destroy() {
				try {
					ip::message_queue::remove(queue_name.c_str());
				} catch(ip::interprocess_exception &e) {
					log_fatal_error(std::string("Failed to remove logging queue: ") + e.what());
				}
			}

			void log(std::string data) {
				if (!mq_) {
					log_fatal_error("Failed to send to logging queue: " + data);
					return;
				}
				if (data.size() >= max_message_size) {
					log_fatal_error("Message to large to fit buffer: " + to_string(data.size()) + " > " + to_string(max_message_size));
					return;
				}
				try {
					mq_->send(&data[0], data.size(), 0);
				} catch(ip::interprocess_exception &e) {
					log_fatal_error(std::string("Failed to send to logging queue: ") + e.what());
				}
			}

			void log_fatal_error(std::string message) {
#ifdef WIN32
				OutputDebugString(strEx::string_to_wstring(message).c_str());
#endif
				std::cout << "TODO: " << message << std::endl;
			}


		};
	}

};