#pragma once

#include <Singleton.h>
#define BUFF_LEN 4096

class Section {
};

class SettingsT
{
private:
	std::string file_;
public:
	typedef std::list<std::string> sectionList;

	SettingsT(void)
	{
	}

	virtual ~SettingsT(void)
	{
	}

	/**
	 * Set the file to read from
	 * @param file A INI-file to use as settings repository
	 */
	void setFile(std::string file) {
		file_ = file;
	}
	/**
	 * Get all keys from a section as a list<string>
	 * @param section The section to return all keys from
	 * @return A list with all keys from the section
	 */
	std::list<std::string> getSection(std::string section) {
		std::list<std::string> ret;
		char* buffer = new char[BUFF_LEN+1];
		unsigned int count = GetPrivateProfileSection(section.c_str(), buffer, BUFF_LEN, file_.c_str());
		if (count == BUFF_LEN-2)
			throw "Fuck...";
		unsigned int last = 0;
		for (unsigned int i=0;i<count;i++) {
			if (buffer[i] == '\0') {
				std::string s = &buffer[last];
				ret.push_back(s);
				last = i+1;
			}
		}
		delete [] buffer;
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
		char* buffer = new char[1024];
		GetPrivateProfileString(section.c_str(), key.c_str(), defaultValue.c_str(), buffer, 1023, file_.c_str());
		std::string ret = buffer;
		delete [] buffer;
		return ret;
	}
	/**
	 * Get an integer from the settings file
	 * @param section Section to read from 
	 * @param key Key to retrieve
	 * @param defaultValue Default value to return if key is not found
	 * @return The value or defaultValue if the key is not found
	 */
	int getInt(std::string section, std::string key, int defaultValue = 0) {
		return GetPrivateProfileInt(section.c_str(), key.c_str(), defaultValue, file_.c_str());
	}
};

typedef Singleton<SettingsT> Settings;		// Implement the settings manager as a singleton