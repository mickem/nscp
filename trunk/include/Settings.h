/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#pragma once

#include <Singleton.h>
#include <string>
#include <map>
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
	std::string file_;
	std::string basepath_;
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
	void setFile(std::string basepath, std::string file, bool forceini = false) {
		file_ = file;
		basepath_ = basepath;
		if (forceini) {
			if (settingsManager)
				delete settingsManager;
			settingsManager = new INISettings(basepath, file);
			return;
		}
		if (REGSettings::hasSettings()) {
			if (settingsManager)
				delete settingsManager;
			settingsManager = new REGSettings();
		} else if (INISettings::hasSettings(basepath, file)) {
			if (settingsManager)
				delete settingsManager;
			settingsManager = new INISettings(basepath, file);
		} else {
			throw SettingsException("No settings method specified, cannot start");
		}
	}

#define UNLIKELY_VALUE_1 -1234
#define UNLIKELY_VALUE_2 -4321
	void read(int type = -1) {
		bool bNew = false;
		TSettings *sM = settingsManager;
		if ((type != -1)&&(type != settingsManager->getActiveTypeID())) {
			if (type == REGSettings::getType()) {
				sM = new REGSettings();
				bNew = true;
			} else if (type == INISettings::getType()) {
				sM = new INISettings(basepath_, file_);
				bNew = true;
			} else {
				throw SettingsException("Invalid settings subsystem specified");
			}
		}
		if (sM == NULL) {
			throw SettingsException("Invalid settings subsystem specified");
		}
		sectionList sections = sM->getSections();
		for (sectionList::const_iterator it=sections.begin();it!=sections.end();++it) {
			sectionList section = sM->getSection(*it);
			for (sectionList::const_iterator it2=section.begin();it2!=section.end();++it2) {
				std::string s = sM->getString((*it), (*it2));
				int i = strEx::stoi(s);
				std::string s2 = strEx::itos(i);
				std::cout << "importing: " << (*it) << "/" << (*it2) << "=" << s << std::endl;
				if (s == s2) {
					setInt((*it), (*it2), i);
				} else {
					setString((*it), (*it2), s);
				}

/*
				std::cout << "  Key: " << (*it2) << std::endl;
				int i = sM->getInt((*it), (*it2), UNLIKELY_VALUE_1);
				std::cout << "Int vaöl: " << i << std::endl;
				if (i == UNLIKELY_VALUE_1) {
					if (sM->getInt((*it), (*it2), UNLIKELY_VALUE_2)==UNLIKELY_VALUE_2) {
						std::cout << "Writing: " << (*it) << " - " << (*it2) << " - " << sM->getString((*it), (*it2)) << std::endl;
						setString((*it), (*it2), sM->getString((*it), (*it2)));
					} else
						setInt((*it), (*it2), i);
				} else if (i == 0) {
					std::string s = sM->getString((*it), (*it2));
					std::cout << "Size: " << s.size() << " |" << s << "| " << std::endl;
					if (s.size() == 0)
						setString((*it), (*it2), s);
					else
						setInt((*it), (*it2), i);
				} else
					setInt((*it), (*it2), i);
					*/
			}
		}
		if (bNew) {
			delete sM;
		}
	}
	void write(int type = -1) {
		bool bNew = false;
		TSettings *sM = settingsManager;
		if ((type != -1)&&(type != settingsManager->getActiveTypeID())) {
			if (type == REGSettings::getType()) {
				sM = new REGSettings();
				bNew = true;
			} else if (type == INISettings::getType()) {
				sM = new INISettings(basepath_, file_);
				bNew = true;
			} else {
				throw SettingsException("Invalid settings subsystem specified");
			}
		}
		if (sM == NULL) {
			throw SettingsException("Invalid settings subsystem specified");
		}
		if (bHasInternalData) {
			for (saveSectionList::const_iterator it=data_.begin();it!=data_.end();++it) {
				for (saveKeyList::const_iterator kit = it->second.begin(); kit != it->second.end(); ++kit) {
					if (kit->second.type == valueStruct::sType)
						sM->setString(it->first, kit->first, kit->second.sVal);
					else
						sM->setInt(it->first, kit->first, kit->second.iVal);
				}
			}
		}
		if (bNew) {
			delete sM;
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