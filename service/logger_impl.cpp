#include "StdAfx.h"

#include <nsclient/logger.hpp>
#include <nsclient/base_logger_impl.hpp>
#include <format.hpp>
#include "logger_impl.hpp"

#include <concurrent_queue.hpp>

#include <nscapi/functions.hpp>
#include <nscapi/nscapi_helper.hpp>

#include "../helpers/settings_manager/settings_manager_impl.h"

#include <settings/client/settings_client.hpp>


nsclient::logging::impl::raw_subscribers subscribers_;

void log_fatal(std::string message) {
	std::cout << message << std::endl;
}

std::string create_message(const std::wstring &module, Plugin::LogEntry::Entry::Level level, const char* file, const int line, const std::wstring &logMessage) {
	std::string str;
	try {
		Plugin::LogEntry message;
		Plugin::LogEntry::Entry *msg = message.add_entry();
		msg->set_sender(utf8::cvt<std::string>(module));
		msg->set_level(level);
		msg->set_file(file);
		msg->set_line(line);
		msg->set_message(utf8::cvt<std::string>(logMessage));
		return message.SerializeAsString();
	} catch (std::exception &e) {
		log_fatal(std::string("Failed to generate message: ") + e.what());
	} catch (...) {
		log_fatal("Failed to generate message: <UNKNOWN>");
	}
	return str;
}
std::string create_message(const std::wstring &module, Plugin::LogEntry::Entry::Level level, const char* file, const int line, const std::string &logMessage) {
	std::string str;
	try {
		Plugin::LogEntry message;
		Plugin::LogEntry::Entry *msg = message.add_entry();
		msg->set_sender(utf8::cvt<std::string>(module));
		msg->set_level(level);
		msg->set_file(file);
		msg->set_line(line);
		msg->set_message(logMessage);
		return message.SerializeAsString();
	} catch (std::exception &e) {
		log_fatal(std::string("Failed to generate message: ") + e.what());
	} catch (...) {
		log_fatal("Failed to generate message: <UNKNOWN>");
	}
	return str;
}
std::string nsclient::logging::logger_helper::create(const std::wstring &module, NSCAPI::log_level::level level, const char* file, const int line, const std::wstring &message) {
	return create_message(module, nscapi::protobuf::functions::log_to_gpb(level), file, line, message);
}
std::string nsclient::logging::logger_helper::create(const std::wstring &module, NSCAPI::log_level::level level, const char* file, const int line, const std::string &message) {
	return create_message(module, nscapi::protobuf::functions::log_to_gpb(level), file, line, message);
}

std::wstring render_log_level_short(Plugin::LogEntry::Entry::Level l) {
	return nsclient::logging::logger_helper::render_log_level_short(nscapi::protobuf::functions::gpb_to_log(l));
}

std::wstring render_log_level_long(Plugin::LogEntry::Entry::Level l) {
	return nsclient::logging::logger_helper::render_log_level_long(nscapi::protobuf::functions::gpb_to_log(l));
}
std::wstring rpad(std::wstring str, std::size_t len) {
	if (str.length() > len)
		return str.substr(str.length()-len);
	return std::wstring(len-str.length(), L' ') + str;
}
std::wstring lpad(std::wstring str, std::size_t len) {
	if (str.length() > len)
		return str.substr(0, len);
	return str + std::wstring(len-str.length(), L' ');
}
std::wstring render_console_message(const std::string &data) {
	std::wstringstream ss;
	try {
		Plugin::LogEntry message;
		if (!message.ParseFromString(data)) {
			log_fatal("Failed to parse message: " + format::strip_ctrl_chars(data));
			return ss.str();
		}

		for (int i=0;i<message.entry_size();i++) {
			Plugin::LogEntry::Entry msg = message.entry(i);
			if (i > 0)
				ss << _T(" -- ");
			std::string tmp = msg.message();
			strEx::replace(tmp, "\n", "\n    -    ");
			ss << lpad(render_log_level_long(msg.level()), 8)
				<< _T(" ") << rpad(utf8::cvt<std::wstring>(msg.sender()), 10)
				<< _T(" ") + utf8::cvt<std::wstring>(msg.message())
				<< std::endl;
			if (msg.level() == Plugin::LogEntry_Entry_Level_LOG_ERROR) {
				ss << _T("                    ") 
					<< utf8::cvt<std::wstring>(msg.file())
					<< _T(":")
					<< msg.line() << std::endl;

			}
		}
		return ss.str();
	} catch (std::exception &e) {
		log_fatal("Failed to parse data from: " + format::strip_ctrl_chars(data) + ": " + e.what());
	} catch (...) {
		log_fatal("Failed to parse data from: " + format::strip_ctrl_chars(data));
	}
	return ss.str();
}

namespace sh = nscapi::settings_helper;

class simple_file_logger : public nsclient::logging::logging_interface_impl {
	std::string file_;
	std::size_t max_size_;
	std::string format_;

public:
	simple_file_logger(std::string file) : max_size_(0), format_("%Y-%m-%d %H:%M:%S") {
		file_ = base_path() + file;
	}
	std::string base_path() {
#ifdef WIN32
		unsigned int buf_len = 4096;
		char* buffer = new char[buf_len+1];
		GetModuleFileNameA(NULL, buffer, buf_len);
		std::string path = buffer;
		std::string::size_type pos = path.rfind('\\');
		path = path.substr(0, pos+1);
		delete [] buffer;
		return path;
#else
		return "";
#endif
	}


	void do_log(const std::string data) {
		if (file_.empty())
			return;
		try {
			if (max_size_ != 0 &&  boost::filesystem::exists(file_.c_str()) && boost::filesystem::file_size(file_.c_str()) > max_size_) {
				int target_size = max_size_*0.7;
				char *tmpBuffer = new char[target_size+1];
				try {
					std::ifstream ifs(file_.c_str());
					ifs.seekg(-target_size, std::ios_base::end);
					ifs.read(tmpBuffer, target_size);
					ifs.close();
					std::ofstream ofs(file_.c_str(), std::ios::trunc);
					ofs.write(tmpBuffer, target_size);
				} catch (...) {
					log_fatal("Failed to truncate log file: " + file_);
				}
				delete [] tmpBuffer;
			}
			std::ofstream stream(file_.c_str(), std::ios::out|std::ios::app|std::ios::ate);
			if (!stream) {
				log_fatal("File could not be opened, Discarding: " + format::strip_ctrl_chars(data));
			}
			std::string date = nsclient::logging::logger_helper::get_formated_date(format_);

			Plugin::LogEntry message;
			if (!message.ParseFromString(data)) {
				log_fatal("Failed to parse message: " + format::strip_ctrl_chars(data));
			} else {
				for (int i=0;i<message.entry_size();i++) {
					Plugin::LogEntry::Entry msg = message.entry(i);
					stream << date
						<< (": ") << utf8::cvt<std::string>(render_log_level_long(msg.level()))
						<< (":") << msg.file()
						<< (":") << msg.line()
						<< (": ") << msg.message() << std::endl;
				}
			}
		} catch (std::exception &e) {
			log_fatal("Failed to parse data from: " + format::strip_ctrl_chars(data) + ": " + e.what());
		} catch (...) {
			log_fatal("Failed to parse data from: " + format::strip_ctrl_chars(data));
		}
	}
	void configure() {
		try {
			std::wstring file;

			sh::settings_registry settings(settings_manager::get_proxy());
			settings.set_alias(_T("log/file"));

			settings.add_path_to_settings()
				(_T("log"),_T("LOG SECTION"), _T("Configure log properties."))

				(_T("log/file"), _T("LOG SECTION"), _T("Configure log file properties."))
				;


			settings.add_key_to_settings(_T("log"))
				(_T("file name"), sh::wstring_key(&file, _T("${exe-path}/nsclient.log")),
				_T("FILENAME"), _T("The file to write log data to. Set this to none to disable log to file."))

				(_T("date format"), sh::string_key(&format_, "%Y-%m-%d %H:%M:%S"),
				_T("DATEMASK"), _T("The size of the buffer to use when getting messages this affects the speed and maximum size of messages you can recieve."))

				;

			settings.add_key_to_settings(_T("log/file"))
				(_T("max size"), sh::size_key(&max_size_, 0),
				_T("MAXIMUM FILE SIZE"), _T("When file size reaches this it will be truncated to 50% if set to 0 (default) truncation will be disabled"))
				;

			settings.register_all();
			settings.notify();

			file_ = utf8::cvt<std::string>(settings_manager::get_proxy()->expand_path(file));
			if (file_.empty())
				file_ = base_path() + "nsclient.log";
			if (file_.find('\\') == std::string::npos && file_.find('/') == std::string::npos) {
				file_ = base_path() + file_;
			}
			if (file_ == "none") {
				file_ = "";
			}

		} catch (nscapi::nscapi_exception &e) {
			log_fatal(std::string("Failed to register command: ") + e.what());
		} catch (std::exception &e) {
			log_fatal(std::string("Exception caught: ") + e.what());
		} catch (...) {
			log_fatal("Failed to register command.");
		}
	}
};



class simple_console_logger : public nsclient::logging::logging_interface_impl {
	std::string format_;
public:
	simple_console_logger() : format_("%Y-%m-%d %H:%M:%S") {}

	void do_log(const std::string data) {
		if (get_console_log()) {
			std::wcout << render_console_message(data);
		}
	}
	void configure() {
		try {
			std::wstring file;

			sh::settings_registry settings(settings_manager::get_proxy());
			settings.set_alias(_T("log/file"));

			settings.add_path_to_settings()
				(_T("log"),_T("LOG SECTION"), _T("Configure log properties."))
				;

			settings.add_key_to_settings(_T("log"))
				(_T("date format"), sh::string_key(&format_, "%Y-%m-%d %H:%M:%S"),
				_T("DATEMASK"), _T("The size of the buffer to use when getting messages this affects the speed and maximum size of messages you can recieve."))

				;

			settings.register_all();
			settings.notify();

		} catch (nscapi::nscapi_exception &e) {
			log_fatal(std::string("Failed to register command: ") + e.what());
		} catch (std::exception &e) {
			log_fatal(std::string("Exception caught: ") + e.what());
		} catch (...) {
			log_fatal("Failed to register command.");
		}
	}
};




const static std::string QUIT_MESSAGE = "$$QUIT$$";
const static std::string CONFIGURE_MESSAGE = "$$CONFIGURE$$";

typedef boost::shared_ptr<nsclient::logging::logging_interface_impl> log_impl_type;


class threaded_logger : public nsclient::logging::logging_interface_impl {
	concurrent_queue<std::string> log_queue_;
	boost::thread thread_;

	log_impl_type background_logger_;

public:

	threaded_logger(log_impl_type background_logger) : background_logger_(background_logger) {}
	~threaded_logger() {
		shutdown();
	}

	void do_log(const std::string data) {
		if (get_console_log()) {
			std::wcout << render_console_message(data);
		}
		push(data);
	}
	void push(const std::string &data) {
		log_queue_.push(data);
	}

	void thread_proc() {
		std::string data;
		while (true) {
			try {
				log_queue_.wait_and_pop(data);
				if (data == QUIT_MESSAGE) {
					break;
				} else if (data == CONFIGURE_MESSAGE) {
					if (background_logger_)
						background_logger_->configure();
				} else {
					if (background_logger_)
						background_logger_->do_log(data);
					subscribers_.notify(data);
				}
			} catch (const std::exception &e) {
				log_fatal(std::string("Failed to process log message: ") + e.what());
			} catch (...) {
				log_fatal("Failed to process log message");
			}
		}
	}

	void configure() {
		push(CONFIGURE_MESSAGE);
	}
	bool startup() {
		if (nsclient::logging::logging_interface_impl::is_started())
			return true;
		thread_ = boost::thread(boost::bind(&threaded_logger::thread_proc, this));
		return nsclient::logging::logging_interface_impl::startup();
	}
	bool shutdown() {
		if (!nsclient::logging::logging_interface_impl::is_started())
			return true;
		try {
			push(QUIT_MESSAGE);
			if (!thread_.timed_join(boost::posix_time::seconds(5))) {
				log_fatal("Failed to exit log slave!");
				return false;
			}
			background_logger_->shutdown();
			return nsclient::logging::logging_interface_impl::shutdown();
		} catch (const std::exception &e) {
			log_fatal(std::string("Failed to exit log slave: ") + e.what());
		} catch (...) {
			log_fatal("Failed to exit log slave");
		}
		return false;
	}

	virtual void set_log_level(NSCAPI::log_level::level level) {
		nsclient::logging::logging_interface_impl::set_log_level(level);
		background_logger_->set_log_level(level);
	}
	virtual void set_console_log(bool console_log) {
		nsclient::logging::logging_interface_impl::set_console_log(console_log);
		background_logger_->set_console_log(console_log);
	}
};

static nsclient::logging::logging_interface_impl *logger_impl_ = NULL;

#define CONSOLE_BACKEND "console"
#define THREADED_FILE_BACKEND "threaded-file"
#define FILE_BACKEND "file"


void nsclient::logging::logger::set_backend(std::string backend) {
	nsclient::logging::logging_interface_impl *tmp = NULL;
	if (backend == CONSOLE_BACKEND) {
		tmp = new simple_console_logger();
	} else if (backend == THREADED_FILE_BACKEND) {
		tmp = new threaded_logger(log_impl_type(new simple_file_logger("nsclient.log")));
	} else if (backend == FILE_BACKEND) {
		tmp = new simple_file_logger("nsclient.log");
	} else {
		tmp = new simple_console_logger();
	}
	nsclient::logging::logging_interface_impl *old = logger_impl_ ;
	if (old != NULL && tmp != NULL) {
		tmp->set_console_log(old->get_console_log());
		tmp->set_log_level(old->get_log_level());
		if (old->is_started())
			tmp->startup();
	}
	logger_impl_ = tmp;
	logger_impl_->debug(_T("log"), __FILE__, __LINE__, "Creating logger: " + backend);
	delete old;
	old = NULL;
}


#define DEFAULT_BACKEND THREADED_FILE_BACKEND
nsclient::logging::logging_interface_impl* get_impl() {
	if (logger_impl_  == NULL)
		nsclient::logging::logger::set_backend(DEFAULT_BACKEND);
	return logger_impl_ ;
}
void nsclient::logging::logger::destroy() {
	nsclient::logging::logging_interface_impl* old = logger_impl_;
	logger_impl_ = NULL;
	delete old;
}


nsclient::logging::logger_interface* nsclient::logging::logger::get_logger() {
	return get_impl();
}

void nsclient::logging::logger::subscribe_raw(raw_subscriber_type subscriber) {
	subscribers_.add(subscriber);
}
void nsclient::logging::logger::clear_subscribers() {
	subscribers_.clear();
}
bool nsclient::logging::logger::startup() {
	return get_impl()->startup();
}
bool nsclient::logging::logger::shutdown() {
	return get_impl()->shutdown();
}
void nsclient::logging::logger::configure() {
	return get_impl()->configure();
}

void nsclient::logging::logger::set_log_level(NSCAPI::log_level::level level) {
	get_impl()->set_log_level(level);
}
void nsclient::logging::logger::set_log_level(std::wstring level) {
	get_impl()->set_log_level(nscapi::logging::parse(level));
}
