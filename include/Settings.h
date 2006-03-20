#pragma once

#include <Singleton.h>
#include <string>
#include <map>
#include <windows.h>
#include <INISettings.h>
#define BUFF_LEN 4096

class SettingsT
{
private:
	typedef struct {
		typedef enum { sType, iType} typeEnum;
		typeEnum type;
		std::string sVal;
		int iVal;
	} valueStruct;
	typedef std::map<std::string,valueStruct> saveKeyList;
	typedef std::map<std::string,saveKeyList> saveSectionList;
	saveSectionList data_;
	bool bHasInternalData;
	INISettings iniManager;
public:
	typedef std::list<std::string> sectionList;
	SettingsT(void) : bHasInternalData(false)
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
		iniManager.setFile(file);
	}

#define UNLIKELY_VALUE -1234
	void read() {
		sectionList sections = getSections();
		for (sectionList::const_iterator it=sections.begin();it!=sections.end();++it) {
			sectionList section = getSection(*it);
			for (sectionList::const_iterator it2=section.begin();it2!=section.end();++it2) {
				int i = getInt((*it), (*it2), UNLIKELY_VALUE);
				if (i == UNLIKELY_VALUE) {
					getString((*it), (*it2));
				}
			}
		}
	}

	sectionList getSections(unsigned int bufferLength = BUFF_LEN) {
		sectionList ret;
		ret = iniManager.getSections();
		if (bHasInternalData) {
			for (saveSectionList::const_iterator kit = data_.begin(); kit != data_.end(); ++kit) {
				ret.push_back(kit->first);
			}
		}
		ret.sort();
		ret.unique();
		return ret;
	}

	/**
	 * Get all keys from a section as a list<string>
	 * @param section The section to return all keys from
	 * @return A list with all keys from the section
	 */
	sectionList getSection(std::string section, unsigned int bufferLength = BUFF_LEN) {
		sectionList ret;
		ret = iniManager.getSection(section);
		if (bHasInternalData) {
			saveSectionList::const_iterator it = data_.find(section);
			if (it != data_.end()) {
				for (saveKeyList::const_iterator kit = it->second.begin(); kit != it->second.end(); ++kit) {
					ret.push_back(kit->first);
				}
			}
		}
		ret.sort();
		ret.unique();
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
		if (bHasInternalData) {
			saveSectionList::const_iterator it = data_.find(section);
			if (it != data_.end()) {
				saveKeyList::const_iterator kit = it->second.find(key);
				if (kit != it->second.end()) {
					if (kit->second.type == valueStruct::sType)
						return kit->second.sVal;
					else
						throw "whoops";
				}
			}
		}
		std::string ret = iniManager.getString(section, key, defaultValue);
		return ret;
	}

	void setString(std::string section, std::string key, std::string value) {
		bHasInternalData = true;
		(data_[section])[key].sVal = value;
		(data_[section])[key].type = valueStruct::sType;
	}

	/**
	 * Get an integer from the settings file
	 * @param section Section to read from 
	 * @param key Key to retrieve
	 * @param defaultValue Default value to return if key is not found
	 * @return The value or defaultValue if the key is not found
	 */
	int getInt(std::string section, std::string key, int defaultValue = 0) {
		return iniManager.getInt(section, key, defaultValue);
	}
	void setInt(std::string section, std::string key, int value) {
		bHasInternalData = true;
		(data_[section])[key].iVal = value;
		(data_[section])[key].type = valueStruct::iType;
	}
};

typedef Singleton<SettingsT> Settings;		// Implement the settings manager as a singleton