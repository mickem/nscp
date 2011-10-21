#include "settings_logger_impl.hpp"
#include "NSClient++.h"

//////////////////////////////////////////////////////////////////////////
/// Log an ERROR message.
///
/// @param file the file where the event happened
/// @param line the line where the event happened
/// @param message the message to log
///
/// @author mickem
void settings_logger::err(std::string file, int line, std::wstring message) {
	std::string s = nsclient::logger_helper::create_error(file.c_str(), line, message.c_str());
	mainClient.reportMessage(s);
}
//////////////////////////////////////////////////////////////////////////
/// Log an WARNING message.
///
/// @param file the file where the event happened
/// @param line the line where the event happened
/// @param message the message to log
///
/// @author mickem
void settings_logger::warn(std::string file, int line, std::wstring message) {
	std::string s = nsclient::logger_helper::create_warning(file.c_str(), line, message.c_str());
	mainClient.reportMessage(s);
}
//////////////////////////////////////////////////////////////////////////
/// Log an INFO message.
///
/// @param file the file where the event happened
/// @param line the line where the event happened
/// @param message the message to log
///
/// @author mickem
void settings_logger::info(std::string file, int line, std::wstring message) {
	std::string s = nsclient::logger_helper::create_info(file.c_str(), line, message.c_str());
	mainClient.reportMessage(s);
}
//////////////////////////////////////////////////////////////////////////
/// Log an DEBUG message.
///
/// @param file the file where the event happened
/// @param line the line where the event happened
/// @param message the message to log
///
/// @author mickem
void settings_logger::debug(std::string file, int line, std::wstring message) {
	if (!mainClient.logDebug())
		return;
	std::string s = nsclient::logger_helper::create_debug(file.c_str(), line, message.c_str());
	mainClient.reportMessage(s);
}
