#pragma once

#include <nsclient/logger/logger.hpp>

#include <boost/thread/mutex.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/shared_ptr.hpp>

#include <string>

namespace nsclient {
	namespace core {

		class path_manager {
		private:
			typedef std::map<std::string, std::string> paths_type;

			nsclient::logging::logger_instance log_instance_;
			boost::timed_mutex mutex_;
			boost::filesystem::path basePath;
			boost::filesystem::path tempPath;
			paths_type paths_cache_;


		public:

			path_manager(nsclient::logging::logger_instance log_instance_);
			std::string getFolder(std::string key);
			std::string expand_path(std::string file);

		private:
			boost::filesystem::path getBasePath();
			boost::filesystem::path getTempPath();
			nsclient::logging::logger_instance get_logger() {
				return log_instance_;
			}
		};
		typedef boost::shared_ptr<path_manager> path_instance;
	}

}
