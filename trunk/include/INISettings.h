#pragma once

#include <Singleton.h>
#include <string>
#include <map>
#include <TSettings.h>
#include <config.h>
#include <iostream>
#include <fstream>

#define BUFF_LEN 4096


class INIFile {
private:
	std::string file_;
public:
	typedef std::list<std::string> sectionList;

public:
	INIFile(std::string file) : file_(file) {}

	std::string getFile() const {
		return file_;
	}
	/**
	 * Retrieves a list of section
	 * @access public 
	 * @returns INIFile::sectionList
	 * @qualifier
	 * @param unsigned int bufferLength
	 */
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
				std::size_t p = s.find('=');
				if (p == std::string::npos)
					ret.push_back(s);
				else
					ret.push_back(s.substr(0,p));
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
		//		if (value.size() > 0)
		//WritePrivateProfileString(section.c_str(), key.c_str(), NULL, file_.c_str());
		WritePrivateProfileString(section.c_str(), key.c_str(), value.c_str(), file_.c_str());
		//		else
		//			WritePrivateProfileString(section.c_str(), key.c_str(), NULL, file_.c_str());
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

class INIFileBundle {
	std::list<INIFile*> files_;
	INIFile *coreFile_;
	std::string basepath_;
public:
	INIFileBundle(std::string basepath, std::string coreFile) : coreFile_(NULL), basepath_(basepath) {
		importFile(basepath_, coreFile, true);
	}

	void importFile(std::string basepath, const std::string fname, bool corefile = false) {
		std::string filename = basepath + fname;
		INIFile *tmp = new INIFile(filename);
		if (corefile)
			coreFile_ = tmp;
		files_.push_front(tmp);
		INIFile::sectionList lst = tmp->getSection("includes");
		for(INIFile::sectionList::const_iterator cit = lst.begin(); cit != lst.end(); ++cit) {
			if (!hasFile(*cit))
				importFile(basepath, *cit);
		}
	}
	bool hasFile(const std::string file) const {
		for (std::list<INIFile*>::const_iterator cit = files_.begin(); cit != files_.end(); ++cit) {
			if (file == (*cit)->getFile())
				return true;
		}
		return false;
	}


	/**
	* Retrieves a list of section
	* @access public 
	* @returns INIFile::sectionList
	* @qualifier
	* @param unsigned int bufferLength
	*/
	INIFile::sectionList getSections(unsigned int bufferLength = BUFF_LEN) {
		INIFile::sectionList ret;
		for (std::list<INIFile*>::const_iterator cit = files_.begin(); cit != files_.end(); ++cit) {
			INIFile::sectionList tmp = (*cit)->getSections(bufferLength);
			ret.insert(ret.begin(), tmp.begin(), tmp.end());
		}
		return ret;
	}

	/**
	* Get all keys from a section as a list<string>
	* @param section The section to return all keys from
	* @return A list with all keys from the section
	*/
	INIFile::sectionList getSection(std::string section, unsigned int bufferLength = BUFF_LEN) {
		INIFile::sectionList ret;
		for (std::list<INIFile*>::const_iterator cit = files_.begin(); cit != files_.end(); ++cit) {
			INIFile::sectionList tmp = (*cit)->getSection(section, bufferLength);
			std::cout << "<<< Reading: " << section << " from: " << (*cit)->getFile() << std::endl;
			ret.insert(ret.begin(), tmp.begin(), tmp.end());
		}
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
		for (std::list<INIFile*>::const_iterator cit = files_.begin(); cit != files_.end(); ++cit) {
			std::string s = (*cit)->getString(section, key, defaultValue);
			if (s != defaultValue)
				return s;
		}
		return defaultValue;
	}

	void setString(std::string section, std::string key, std::string value) {
		if (coreFile_ != NULL)
			coreFile_->setString(section, key, value);
	}

	/**
	* Get an integer from the settings file
	* @param section Section to read from 
	* @param key Key to retrieve
	* @param defaultValue Default value to return if key is not found
	* @return The value or defaultValue if the key is not found
	*/
	int getInt(std::string section, std::string key, int defaultValue = 0) {
		for (std::list<INIFile*>::const_iterator cit = files_.begin(); cit != files_.end(); ++cit) {
			int s = (*cit)->getInt(section, key, defaultValue);
			if (s != defaultValue)
				return s;
		}
		return defaultValue;
	}
	void setInt(std::string section, std::string key, int value) {
		if (coreFile_ != NULL)
			coreFile_->setInt(section, key, value);
	}
};

class INISettings : public TSettings
{
private:
//	typedef std::map<std::string,std::string> saveKeyList;
//	typedef std::map<std::string,saveKeyList> saveSectionList;
	INIFileBundle settingsBundle;
	std::string basepath_;
public:
	INISettings(std::string basepath, std::string file) : settingsBundle(basepath, file)
	{
	}

	virtual ~INISettings(void)
	{
	}
	std::string getActiveType() {
		return "INI-file";
	}
	int getActiveTypeID() {
		return INISettings::getType();
	}
	static int getType() {
		return 1;
	}

	static bool hasSettings(std::string basepath, std::string file) {
		INIFile ini(basepath + "\\" + file);
		return ini.getInt(MAIN_SECTION_TITLE, MAIN_USEFILE, MAIN_USEFILE_DEFAULT) == 1;
	}

	sectionList getSections(unsigned int bufferLength = BUFF_LEN) {
		return settingsBundle.getSections(bufferLength);
	}

	/**
	* Get all keys from a section as a list<string>
	* @param section The section to return all keys from
	* @return A list with all keys from the section
	*/
	sectionList getSection(std::string section, unsigned int bufferLength = BUFF_LEN) {
		return settingsBundle.getSection(section, bufferLength);
	}
	/**
	* Get a string from the settings file
	* @param section Section to read from 
	* @param key Key to retrieve
	* @param defaultValue Default value to return if key is not found
	* @return The value or defaultValue if the key is not found
	*/
	std::string getString(std::string section, std::string key, std::string defaultValue = "") const {
		return settingsBundle.getString(section, key, defaultValue);
	}

	void setString(std::string section, std::string key, std::string value) {
		return settingsBundle.setString(section, key, value);
	}

	/**
	* Get an integer from the settings file
	* @param section Section to read from 
	* @param key Key to retrieve
	* @param defaultValue Default value to return if key is not found
	* @return The value or defaultValue if the key is not found
	*/
	int getInt(std::string section, std::string key, int defaultValue = 0) {
		return settingsBundle.getInt(section, key, defaultValue);
	}
	void setInt(std::string section, std::string key, int value) {
		return settingsBundle.setInt(section, key, value);
	}
};
