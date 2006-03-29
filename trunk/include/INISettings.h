#pragma once

#include <Singleton.h>
#include <string>
#include <map>
#include <windows.h>
#include <TSettings.h>
#include <config.h>

#define BUFF_LEN 4096

class INISettings : public TSettings
{
private:
//	typedef std::map<std::string,std::string> saveKeyList;
//	typedef std::map<std::string,saveKeyList> saveSectionList;
	std::string file_;
public:
	typedef std::list<std::string> sectionList;
	INISettings(std::string file) : file_(file)
	{
	}

	virtual ~INISettings(void)
	{
	}
	std::string getActiveType() {
		return "INI-file";
	}

	static bool hasSettings(std::string file) {
		return GetPrivateProfileInt(MAIN_SECTION_TITLE, MAIN_USEFILE, MAIN_USEFILE_DEFAULT, file.c_str()) == 1;
	}

	sectionList getSections(unsigned int bufferLength = BUFF_LEN) {
		sectionList ret;
		char* buffer = new char[bufferLength+1];
		unsigned int count = ::GetPrivateProfileSectionNames(buffer, BUFF_LEN, file_.c_str());
		if (count == bufferLength-2) {
			delete [] buffer;
			return getSections(bufferLength*10);
		}
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
	* Get all keys from a section as a list<string>
	* @param section The section to return all keys from
	* @return A list with all keys from the section
	*/
	sectionList getSection(std::string section, unsigned int bufferLength = BUFF_LEN) {
		sectionList ret;
		char* buffer = new char[bufferLength+1];
		unsigned int count = GetPrivateProfileSection(section.c_str(), buffer, bufferLength, file_.c_str());
		if (count == bufferLength-2) {
			delete [] buffer;
			return getSection(section, bufferLength*10);
		}
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

	void setString(std::string section, std::string key, std::string value) {
		WritePrivateProfileString(section.c_str(), key.c_str(), value.c_str(), file_.c_str());
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
	void setInt(std::string section, std::string key, int value) {
		WritePrivateProfileString(section.c_str(), key.c_str(), strEx::itos(value).c_str(), file_.c_str());
	}
};
