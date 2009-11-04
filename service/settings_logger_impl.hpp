#pragma once

#include "core_api.h"
#include <settings/Settings.h>

class settings_logger : public Settings::LoggerInterface {
public:
	//////////////////////////////////////////////////////////////////////////
	/// Log an ERROR message.
	///
	/// @param file the file where the event happened
	/// @param line the line where the event happened
	/// @param message the message to log
	///
	/// @author mickem
	void err(std::wstring file, int line, std::wstring message) {
		NSAPIMessage(NSCAPI::error, file.c_str(), line, message.c_str());
	}
	//////////////////////////////////////////////////////////////////////////
	/// Log an WARNING message.
	///
	/// @param file the file where the event happened
	/// @param line the line where the event happened
	/// @param message the message to log
	///
	/// @author mickem
	void warn(std::wstring file, int line, std::wstring message) {
		NSAPIMessage(NSCAPI::warning, file.c_str(), line, message.c_str());
	}
	//////////////////////////////////////////////////////////////////////////
	/// Log an INFO message.
	///
	/// @param file the file where the event happened
	/// @param line the line where the event happened
	/// @param message the message to log
	///
	/// @author mickem
	void info(std::wstring file, int line, std::wstring message) {
		NSAPIMessage(NSCAPI::log, file.c_str(), line, message.c_str());
	}
	//////////////////////////////////////////////////////////////////////////
	/// Log an DEBUG message.
	///
	/// @param file the file where the event happened
	/// @param line the line where the event happened
	/// @param message the message to log
	///
	/// @author mickem
	void debug(std::wstring file, int line, std::wstring message) {
		NSAPIMessage(NSCAPI::debug, file.c_str(), line, message.c_str());
	}
};
