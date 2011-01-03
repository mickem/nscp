#pragma once

#include "core_api.h"
#include "logger.hpp"
#include <settings/settings_core.hpp>

class settings_logger : public settings::logger_interface {
public:
	//////////////////////////////////////////////////////////////////////////
	/// Log an ERROR message.
	///
	/// @param file the file where the event happened
	/// @param line the line where the event happened
	/// @param message the message to log
	///
	/// @author mickem
	void err(std::string file, int line, std::wstring message);
	//////////////////////////////////////////////////////////////////////////
	/// Log an WARNING message.
	///
	/// @param file the file where the event happened
	/// @param line the line where the event happened
	/// @param message the message to log
	///
	/// @author mickem
	void warn(std::string file, int line, std::wstring message);
	//////////////////////////////////////////////////////////////////////////
	/// Log an INFO message.
	///
	/// @param file the file where the event happened
	/// @param line the line where the event happened
	/// @param message the message to log
	///
	/// @author mickem
	void info(std::string file, int line, std::wstring message);
	//////////////////////////////////////////////////////////////////////////
	/// Log an DEBUG message.
	///
	/// @param file the file where the event happened
	/// @param line the line where the event happened
	/// @param message the message to log
	///
	/// @author mickem
	void debug(std::string file, int line, std::wstring message);
};
