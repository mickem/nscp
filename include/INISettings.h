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
	std::wstring file_;
public:
	//typedef std::list<std::wstring> sectionList;

public:
	INIFile(std::wstring file) : file_(file) {}

	std::wstring getFile() const {
		return file_;
	}
	/**
	 * Retrieves a list of section
	 * @access public 
	 * @returns INIFile::sectionList
	 * @qualifier
	 * @param unsigned int bufferLength
	 */
	settings_base::sectionList getSections(unsigned int bufferLength = BUFF_LEN) {
		settings_base::sectionList ret;
		TCHAR* buffer = new TCHAR[bufferLength+1];
		unsigned int count = ::GetPrivateProfileSectionNames(buffer, BUFF_LEN, file_.c_str());
		if (count == bufferLength-2) {
			delete [] buffer;
			return getSections(bufferLength*10);
		}
		unsigned int last = 0;
		for (unsigned int i=0;i<count;i++) {
			if (buffer[i] == '\0') {
				std::wstring s = &buffer[last];
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
	settings_base::sectionList getSection(std::wstring section, unsigned int bufferLength = BUFF_LEN) {
		settings_base::sectionList ret;
		TCHAR* buffer = new TCHAR[bufferLength+1];
		unsigned int count = GetPrivateProfileSection(section.c_str(), buffer, bufferLength, file_.c_str());
		if (count == bufferLength-2) {
			delete [] buffer;
			return getSection(section, bufferLength*10);
		}
		unsigned int last = 0;
		for (unsigned int i=0;i<count;i++) {
			if (buffer[i] == '\0') {
				std::wstring s = &buffer[last];
				std::size_t p = s.find('=');
				if (p == std::wstring::npos)
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
	* Get all keys from a section as a list<string>
	* @param section The section to return all keys from
	* @return A list with all keys from the section
	*/
	//void settings_base::setSection(std::wstring,settings_base::sectionList)
	void setSection(std::wstring section, settings_base::sectionList data) {
		unsigned int length = 0;
		for (settings_base::sectionList::const_iterator cit = data.begin(); cit!=data.end();++cit) {
			length += (*cit).size() + 10;
		}
		TCHAR* buffer = new TCHAR[length+1];
		unsigned int index = 0;
		for (settings_base::sectionList::const_iterator cit = data.begin(); cit!=data.end();++cit) {
			wcsncpy_s(&buffer[index], length-index-1,(*cit).c_str(), (*cit).length()+1);
			index+=(*cit).length();
			buffer[index]=0;
			index++;
		}
		buffer[index]=0;
		buffer[index+1]=0;
		WritePrivateProfileSection(section.c_str(), buffer, file_.c_str());
		delete [] buffer;
	}
	/**
	* Get a string from the settings file
	* @param section Section to read from 
	* @param key Key to retrieve
	* @param defaultValue Default value to return if key is not found
	* @return The value or defaultValue if the key is not found
	*/
	std::wstring getString(std::wstring section, std::wstring key, std::wstring defaultValue = _T("")) const {
		TCHAR* buffer = new TCHAR[1024];
		GetPrivateProfileString(section.c_str(), key.c_str(), defaultValue.c_str(), buffer, 1023, file_.c_str());
		std::wstring ret = buffer;
		delete [] buffer;
		return ret;
	}

	void setString(std::wstring section, std::wstring key, std::wstring value) {
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
	int getInt(std::wstring section, std::wstring key, int defaultValue = 0) {
		return GetPrivateProfileInt(section.c_str(), key.c_str(), defaultValue, file_.c_str());
	}
	void setInt(std::wstring section, std::wstring key, int value) {
		WritePrivateProfileString(section.c_str(), key.c_str(), strEx::itos(value).c_str(), file_.c_str());
	}
};

class INIFileBundle {
	std::list<INIFile*> files_;
	INIFile *coreFile_;
	std::wstring basepath_;
public:
	INIFileBundle(std::wstring basepath, std::wstring coreFile) : coreFile_(NULL), basepath_(basepath) {
		importFile(basepath_, coreFile, true);
	}

	void importFile(std::wstring basepath, const std::wstring fname, bool corefile = false) {
		std::wstring filename = basepath + fname;
		INIFile *tmp = new INIFile(filename);
		if (corefile)
			coreFile_ = tmp;
		files_.push_front(tmp);
		settings_base::sectionList lst = tmp->getSection(_T("includes"));
		for(settings_base::sectionList::const_iterator cit = lst.begin(); cit != lst.end(); ++cit) {
			if (!hasFile(*cit))
				importFile(basepath, *cit);
		}
	}
	bool hasFile(const std::wstring file) const {
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
	settings_base::sectionList getSections(unsigned int bufferLength = BUFF_LEN) {
		settings_base::sectionList ret;
		for (std::list<INIFile*>::const_iterator cit = files_.begin(); cit != files_.end(); ++cit) {
			settings_base::sectionList tmp = (*cit)->getSections(bufferLength);
			ret.insert(ret.begin(), tmp.begin(), tmp.end());
		}
		return ret;
	}

	/**
	* Get all keys from a section as a list<string>
	* @param section The section to return all keys from
	* @return A list with all keys from the section
	*/
	settings_base::sectionList getSection(std::wstring section, unsigned int bufferLength = BUFF_LEN) {
		settings_base::sectionList ret;
		for (std::list<INIFile*>::const_iterator cit = files_.begin(); cit != files_.end(); ++cit) {
			settings_base::sectionList tmp = (*cit)->getSection(section, bufferLength);
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
	std::wstring getString(std::wstring section, std::wstring key, std::wstring defaultValue = _T("")) const {
		for (std::list<INIFile*>::const_iterator cit = files_.begin(); cit != files_.end(); ++cit) {
			std::wstring s = (*cit)->getString(section, key, defaultValue);
			if (s != defaultValue)
				return s;
		}
		return defaultValue;
	}

	void setSection(std::wstring section, settings_base::sectionList data) {
		if (coreFile_ != NULL)
			coreFile_->setSection(section, data);
	}
		
	void setString(std::wstring section, std::wstring key, std::wstring value) {
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
	int getInt(std::wstring section, std::wstring key, int defaultValue = 0) {
		for (std::list<INIFile*>::const_iterator cit = files_.begin(); cit != files_.end(); ++cit) {
			int s = (*cit)->getInt(section, key, defaultValue);
			if (s != defaultValue)
				return s;
		}
		return defaultValue;
	}
	void setInt(std::wstring section, std::wstring key, int value) {
		if (coreFile_ != NULL)
			coreFile_->setInt(section, key, value);
	}
};

class INISettings : public settings_base
{
private:
//	typedef std::map<std::wstring,std::wstring> saveKeyList;
//	typedef std::map<std::wstring,saveKeyList> saveSectionList;
	INIFileBundle settingsBundle;
	std::wstring basepath_;
public:
	INISettings(std::wstring basepath, std::wstring file) : settingsBundle(basepath, file)
	{
	}

	virtual ~INISettings(void)
	{
	}
	std::wstring getActiveType() {
		return _T("INI-file");
	}
	int getActiveTypeID() {
		return INISettings::getType();
	}
	static int getType() {
		return 1;
	}

	static bool hasSettings(std::wstring basepath, std::wstring file) {
		INIFile ini(basepath + _T("\\") + file);
		return ini.getInt(MAIN_SECTION_TITLE, MAIN_USEFILE, MAIN_USEFILE_DEFAULT) == 1;
	}

	sectionList getSections(unsigned int bufferLength = BUFF_LEN) {
		return settingsBundle.getSections(bufferLength);
	}
	void setSection(std::wstring section, settings_base::sectionList data) {
		return settingsBundle.setSection(section, data);
	}

	/**
	* Get all keys from a section as a list<string>
	* @param section The section to return all keys from
	* @return A list with all keys from the section
	*/
	sectionList getSection(std::wstring section, unsigned int bufferLength = BUFF_LEN) {
		return settingsBundle.getSection(section, bufferLength);
	}
	/**
	* Get a string from the settings file
	* @param section Section to read from 
	* @param key Key to retrieve
	* @param defaultValue Default value to return if key is not found
	* @return The value or defaultValue if the key is not found
	*/
	std::wstring getString(std::wstring section, std::wstring key, std::wstring defaultValue = _T("")) const {
		return settingsBundle.getString(section, key, defaultValue);
	}

	void setString(std::wstring section, std::wstring key, std::wstring value) {
		return settingsBundle.setString(section, key, value);
	}

	/**
	* Get an integer from the settings file
	* @param section Section to read from 
	* @param key Key to retrieve
	* @param defaultValue Default value to return if key is not found
	* @return The value or defaultValue if the key is not found
	*/
	int getInt(std::wstring section, std::wstring key, int defaultValue = 0) {
		return settingsBundle.getInt(section, key, defaultValue);
	}
	void setInt(std::wstring section, std::wstring key, int value) {
		return settingsBundle.setInt(section, key, value);
	}
};
