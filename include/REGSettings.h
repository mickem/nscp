#pragma once

#include <string>
#include <windows.h>
#include <TSettings.h>
#define BUFF_LEN 4096

class REGSettings : public TSettings
{
public:
	typedef std::list<std::string> sectionList;
	REGSettings(void)
	{
	}

	virtual ~REGSettings(void)
	{
	}

	static bool hasSettings() {
		// @todo
		return false;
	}

	sectionList getSections(unsigned int bufferLength = BUFF_LEN) {
		sectionList ret;
		return ret;
	}

	/**
	* Get all keys from a section as a list<string>
	* @param section The section to return all keys from
	* @return A list with all keys from the section
	*/
	sectionList getSection(std::string section, unsigned int bufferLength = BUFF_LEN) {
		sectionList ret;
		return ret;
	}
	/**
	* Get a string from the settings file
	* @param section Section to read from 
	* @param key Key to retrieve
	* @param defaultValue Default value to return if key is not found
	* @return The value or defaultValue if the key is not found
	*/
	std::string getString(std::string section, std::string key, std::string defaultValue = "") const {
		std::string ret;
		return ret;
	}

	void setString(std::string section, std::string key, std::string value) {
	}

	/**
	* Get an integer from the settings file
	* @param section Section to read from 
	* @param key Key to retrieve
	* @param defaultValue Default value to return if key is not found
	* @return The value or defaultValue if the key is not found
	*/
	int getInt(std::string section, std::string key, int defaultValue = 0) {
		return 0;
	}
	void setInt(std::string section, std::string key, int value) {
	}
};
