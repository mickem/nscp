#include <boost/filesystem.hpp>

#include <nsclient/logger.hpp>
#include <nsclient/base_logger_impl.hpp>
#include <format.hpp>
#include "logger_impl.hpp"
#include <config.h>

#include <concurrent_queue.hpp>

#include <nscapi/functions.hpp>
#include <nscapi/nscapi_helper.hpp>

#include "../libs/settings_manager/settings_manager_impl.h"

#include <nscapi/nscapi_settings_helper.hpp>


nsclient::logging::impl::raw_subscribers subscribers_;

void log_fatal(std::string message) {
	std::cout << message << "\n";
}
std::string create_message(const std::string &module, Plugin::LogEntry::Entry::Level level, const char* file, const int line, const std::string &logMessage) {
	std::string str;
	try {
		Plugin::LogEntry message;
		Plugin::LogEntry::Entry *msg = message.add_entry();
		msg->set_sender(module);
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
std::string nsclient::logging::logger_helper::create(const std::string &module, NSCAPI::log_level::level level, const char* file, const int line, const std::string &message) {
	return create_message(module, nscapi::protobuf::functions::log_to_gpb(level), file, line, message);
}

std::string render_log_level_short(Plugin::LogEntry::Entry::Level l) {
	return nsclient::logging::logger_helper::render_log_level_short(nscapi::protobuf::functions::gpb_to_log(l));
}

std::string render_log_level_long(Plugin::LogEntry::Entry::Level l) {
	return nsclient::logging::logger_helper::render_log_level_long(nscapi::protobuf::functions::gpb_to_log(l));
}
std::string rpad(std::string str, std::size_t len) {
	if (str.length() > len)
		return str.substr(str.length()-len);
	return std::string(len-str.length(), ' ') + str;
}
std::string lpad(std::string str, std::size_t len) {
	if (str.length() > len)
		return str.substr(0, len);
	return str + std::string(len-str.length(), ' ');
}
std::pair<bool,std::string> render_console_message(const bool oneline, const std::string &data) {
	std::stringstream ss;
	bool is_error = false;
	try {
		Plugin::LogEntry message;
		if (!message.ParseFromString(data)) {
			log_fatal("Failed to parse message: " + format::strip_ctrl_chars(data));
			return std::make_pair(true, "ERROR");
		}

		for (int i=0;i<message.entry_size();i++) {
			const Plugin::LogEntry::Entry &msg = message.entry(i);
			std::string tmp = msg.message();
			strEx::replace(tmp, "\n", "\n    -    ");
			if (oneline) {
				ss << msg.file()
					<< "("
					<< msg.line() 
					<< "): "
					<< render_log_level_long(msg.level())
					<< ": "
					<< tmp
					<< "\n";
			} else {
				if (i > 0)
					ss << " -- ";
				ss << lpad(render_log_level_short(msg.level()), 1)
					<< " " << rpad(msg.sender(), 10)
					<< " " + msg.message()
					<< "\n";
				if (msg.level() == Plugin::LogEntry_Entry_Level_LOG_ERROR) {
					ss << "                    "
						<< msg.file()
						<< ":"
						<< msg.line() << "\n";
				}
			}
		}
#ifdef WIN32
		return std::make_pair(is_error, utf8::to_encoding(utf8::cvt<std::wstring>(ss.str()), "oem"));
#else
		return std::make_pair(is_error, ss.str());
#endif
	} catch (std::exception &e) {
		log_fatal("Failed to parse data from: " + format::strip_ctrl_chars(data) + ": " + e.what());
	} catch (...) {
		log_fatal("Failed to parse data from: " + format::strip_ctrl_chars(data));
	}
	return std::make_pair(true, "ERROR");
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
				int target_size = static_cast<int>(max_size_*0.7);
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
			std::string date = nsclient::logging::logger_helper::get_formated_date(format_);

			Plugin::LogEntry message;
			if (!message.ParseFromString(data)) {
				log_fatal("Failed to parse message: " + format::strip_ctrl_chars(data));
			} else {
				std::ofstream stream(file_.c_str(), std::ios::out|std::ios::app|std::ios::ate);
				for (int i=0;i<message.entry_size();i++) {
					Plugin::LogEntry::Entry msg = message.entry(i);
					if (!stream) {
						log_fatal("File could not be opened, Discarding: " + utf8::cvt<std::string>(render_log_level_long(msg.level())) + ": " + msg.message());
					} else {
						stream << date
							<< (": ") << utf8::cvt<std::string>(render_log_level_long(msg.level()))
							<< (":") << msg.file()
							<< (":") << msg.line()
							<< (": ") << msg.message() << "\n";
					}
				}
			}
		} catch (std::exception &e) {
			log_fatal("Failed to parse data from: " + format::strip_ctrl_chars(data) + ": " + e.what());
		} catch (...) {
			log_fatal("Failed to parse data from: " + format::strip_ctrl_chars(data));
		}
	}
	struct config_data {
		std::string file;
		std::string format;
		std::size_t max_size;
	};
	config_data do_config() {
		config_data ret;
		try {

			sh::settings_registry settings(settings_manager::get_proxy());
			settings.set_alias("log/file");

			settings.add_path_to_settings()
				("log", "LOG SECTION", "Configure log properties.")

				("log/file", "LOG SECTION", "Configure log file properties.")
				;


			settings.add_key_to_settings("log")
				("file name", sh::string_key(&ret.file, DEFAULT_LOG_LOCATION),
				"FILENAME", "The file to write log data to. Set this to none to disable log to file.")

				("date format", sh::string_key(&ret.format, "%Y-%m-%d %H:%M:%S"),
				"DATEMASK", "The size of the buffer to use when getting messages this affects the speed and maximum size of messages you can recieve.")

				;

			settings.add_key_to_settings("log/file")
				("max size", sh::size_key(&ret.max_size, 0),
				"MAXIMUM FILE SIZE", "When file size reaches this it will be truncated to 50% if set to 0 (default) truncation will be disabled")
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
		return ret;
	}
	void synch_configure() {
		do_config();
	}

	void asynch_configure() {
		try {
			config_data config = do_config();

			format_ = config.format;
			max_size_ = config.max_size;
			file_ = settings_manager::get_proxy()->expand_path(config.file);
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
	std::vector<char> buf_;
public:
	simple_console_logger() : format_("%Y-%m-%d %H:%M:%S"), buf_(65536) {
		std::cout.rdbuf()->pubsetbuf(buf_.data(), buf_.size());
	}

	void do_log(const std::string data) {
		if (is_console()) {
			std::pair<bool,std::string> m = render_console_message(is_oneline(), data);
			if (!is_no_std_err() && m.first)
				std::cerr << m.second;
			else
				std::cout << m.second;
		}
	}
	struct config_data {
		std::string format;
	};
	config_data do_config() {
		config_data ret;
		try {
			sh::settings_registry settings(settings_manager::get_proxy());
			settings.set_alias("log/file");

			settings.add_path_to_settings()
				("log", "LOG SECTION", "Configure log properties.")
				;

			settings.add_key_to_settings("log")
				("date format", sh::string_key(&format_, "%Y-%m-%d %H:%M:%S"),
				"DATEMASK", "The syntax of the dates in the log file.")
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
		return ret;
	}
	void synch_configure() {
		do_config();
	}
	void asynch_configure() {
		try {
			config_data config = do_config();
			format_ = config.format;
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
const static std::string SET_CONFIG_MESSAGE = "$$SET_CONFIG$$";

typedef boost::shared_ptr<nsclient::logging::logging_interface_impl> log_impl_type;


class threaded_logger : public nsclient::logging::logging_interface_impl {
	concurrent_queue<std::string> log_queue_;
	boost::thread thread_;

	boost::timed_mutex mutext_started;


	log_impl_type background_logger_;

public:

	threaded_logger(log_impl_type background_logger) : background_logger_(background_logger) {}
	~threaded_logger() {
		shutdown();
	}

	void do_log(const std::string data) {
		push(data);
	}
	void push(const std::string &data) {
		log_queue_.push(data);
	}

	void thread_proc() {
		std::string data;
		bool first = true;
		while (true) {
			if (first)
				mutext_started.unlock();
			first = false;
			try {
				log_queue_.wait_and_pop(data);
				if (data == QUIT_MESSAGE) {
					return;
				} else if (data == CONFIGURE_MESSAGE) {
					if (background_logger_)
						background_logger_->asynch_configure();
				} else if (data.size() > SET_CONFIG_MESSAGE.size() && data.substr(0, SET_CONFIG_MESSAGE.size()) == SET_CONFIG_MESSAGE) {
					background_logger_->set_config(data.substr(SET_CONFIG_MESSAGE.size()));
				} else {
					if (is_console()) {
						std::pair<bool,std::string> m = render_console_message(is_oneline(), data);
						if (!is_no_std_err() && m.first)
							std::cerr << m.second;
						else
							std::cout <<  m.second;
					}
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

	void asynch_configure() {
		push(CONFIGURE_MESSAGE);
	}
	void synch_configure() {
		background_logger_->synch_configure();
	}
	bool startup() {
		if (nsclient::logging::logging_interface_impl::is_started())
			return true;
		mutext_started.lock();
		thread_ = boost::thread(boost::bind(&threaded_logger::thread_proc, this));
		if (!mutext_started.timed_lock(boost::posix_time::seconds(10)))
			log_fatal("Failed to wait for logger thread");
		return nsclient::logging::logging_interface_impl::startup();
	}
	bool shutdown() {
		if (!nsclient::logging::logging_interface_impl::is_started())
			return true;
		try {
			push(QUIT_MESSAGE);
			if (!thread_.timed_join(boost::posix_time::seconds(10))) {
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
	virtual void set_config(const std::string &key) {
		nsclient::logging::logging_interface_impl::set_config(key);
		std::string message = SET_CONFIG_MESSAGE + key;
		push(message);
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
		if (old->is_console())
			tmp->set_config("console");
		if (old->is_no_std_err())
			tmp->set_config("no-std-err");
		if (old->is_oneline())
			tmp->set_config("oneline");
		if (old->is_started())
			tmp->startup();
		tmp->set_log_level(old->get_log_level());
	}
	logger_impl_ = tmp;
	logger_impl_->debug("log", __FILE__, __LINE__, "Creating logger: " + backend);
	delete old;
	old = NULL;
}


#define DEFAULT_BACKEND THREADED_FILE_BACKEND
//#define DEFAULT_BACKEND CONSOLE_BACKEND
nsclient::logging::logging_interface_impl* get_impl() {
	if (logger_impl_ == NULL)
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
	get_impl()->synch_configure();
	get_impl()->asynch_configure();
}

void nsclient::logging::logger::set_log_level(NSCAPI::log_level::level level) {
	get_impl()->set_log_level(level);
}
void nsclient::logging::logger::set_log_level(std::string level) {
	NSCAPI::log_level::level iLevel = nscapi::logging::parse(level);
	if (iLevel == NSCAPI::log_level::unknown)
		get_impl()->set_config(level);
	else 
		get_impl()->set_log_level(iLevel);
}
