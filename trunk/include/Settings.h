#pragma once

#include <Singleton.h>
#include <string>
#include <map>
#include <windows.h>
#include <INISettings.h>
#include <REGSettings.h>
#define BUFF_LEN 4096

class SettingsException {
private:
	std::string err;
public:
	SettingsException(std::string str) : err(str) {}
	std::string getMessage() {
		return err;
	}

};

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
	TSettings *settingsManager;

public:
	typedef std::list<std::string> sectionList;
	SettingsT(void) : bHasInternalData(false), settingsManager(NULL)
	{
	}

	virtual ~SettingsT(void)
	{
		if (settingsManager)
			delete settingsManager;
	}
	std::string getActiveType() {
		if (!settingsManager) {
			return "";
		} return settingsManager->getActiveType();
	}

	/**
	 * Set the file to read from
	 * @param file A INI-file to use as settings repository
	 */
	void setFile(std::string file, bool forceini = false) {
		if (forceini) {
			if (settingsManager)
				delete settingsManager;
			settingsManager = new INISettings(file);
			return;
		}
		if (REGSettings::hasSettings()) {
			if (settingsManager)
				delete settingsManager;
			settingsManager = new REGSettings();
		} else if (INISettings::hasSettings(file)) {
			if (settingsManager)
				delete settingsManager;
			settingsManager = new INISettings(file);
		} else {
			throw SettingsException("No settings method specified, cannot start");
		}
	}

#define UNLIKELY_VALUE_1 -1234
#define UNLIKELY_VALUE_2 -4321
	void read() {
		sectionList sections = getSections();
		for (sectionList::const_iterator it=sections.begin();it!=sections.end();++it) {
			sectionList section = getSection(*it);
			for (sectionList::const_iterator it2=section.begin();it2!=section.end();++it2) {
				int i = getInt((*it), (*it2), UNLIKELY_VALUE_1);
				if (i == UNLIKELY_VALUE_1) {
					if (getInt((*it), (*it2), UNLIKELY_VALUE_2)==UNLIKELY_VALUE_2)
						getString((*it), (*it2));
				}
			}
		}
	}
	void write() {
		if (bHasInternalData) {
			for (saveSectionList::const_iterator it=data_.begin();it!=data_.end();++it) {
				for (saveKeyList::const_iterator kit = it->second.begin(); kit != it->second.end(); ++kit) {
					if (kit->second.type == valueStruct::sType)
						setString(it->first, kit->first, kit->second.sVal);
					else
						setInt(it->first, kit->first, kit->second.iVal);
				}
			}
		}
	}

	sectionList getSections(unsigned int bufferLength = BUFF_LEN) {
		if (!settingsManager)
			throw SettingsException("No settings manager found have you configured.");
		sectionList ret;
		ret = settingsManager->getSections();
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
		if (!settingsManager)
			throw SettingsException("No settings manager found have you configured.");
		sectionList ret;
		ret = settingsManager->getSection(section);
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
		if (!settingsManager)
			throw SettingsException("No settings manager found have you configured.");
		if (bHasInternalData) {
			saveSectionList::const_iterator it = data_.find(section);
			if (it != data_.end()) {
				saveKeyList::const_iterator kit = it->second.find(key);
				if (kit != it->second.end()) {
					if (kit->second.type == valueStruct::sType)
						return kit->second.sVal;
					else
						return strEx::itos(kit->second.iVal);
				}
			}
		}
		std::string ret = settingsManager->getString(section, key, defaultValue);
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
		if (!settingsManager)
			throw SettingsException("No settings manager found have you configured.");
		if (bHasInternalData) {
			saveSectionList::const_iterator it = data_.find(section);
			if (it != data_.end()) {
				saveKeyList::const_iterator kit = it->second.find(key);
				if (kit != it->second.end()) {
					if (kit->second.type == valueStruct::sType)
						return strEx::stoi(kit->second.sVal);
					else
						return kit->second.iVal;
				}
			}
		}
		return settingsManager->getInt(section, key, defaultValue);
	}
	void setInt(std::string section, std::string key, int value) {
		bHasInternalData = true;
		(data_[section])[key].iVal = value;
		(data_[section])[key].type = valueStruct::iType;
	}
};

typedef Singleton<SettingsT> Settings;		// Implement the settings manager as a singleton