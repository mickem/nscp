#pragma once

#include <win/windows.hpp>
#include <string>

class ExceptionManager {
public:
	explicit ExceptionManager();
	~ExceptionManager();

	static void StartMonitoring();

	void setup_service_name(std::string service);
  void setup_restart_flag();
  void setup_path(std::string crash_path);
	void setup_app(std::string application, std::string version, std::string date);
  static LONG WINAPI HandleException(EXCEPTION_POINTERS* exinfo);

  static ExceptionManager *instance();
  void handle_exception(EXCEPTION_POINTERS* exinfo);

private:
  std::string crash_path_;
  std::string application_;
	std::string version_;
	std::string date_;
  std::string service_;
	bool restart_;
};
