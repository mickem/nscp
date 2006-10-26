#pragma once

#include <string>
#include <windows.h>
#define BUFF_LEN 4096

class TSettings
{
public:
	typedef std::list<std::string> sectionList;
	TSettings(void)
	{
	}

	virtual ~TSettings(void)
	{
	}
	virtual std::string getActiveType() = 0;
	virtual sectionList getSections(unsigned int bufferLength = BUFF_LEN) = 0;
	virtual sectionList getSection(std::string section, unsigned int bufferLength = BUFF_LEN) = 0;
	virtual std::string getString(std::string section, std::string key, std::string defaultValue = "") const = 0;
	virtual void setString(std::string section, std::string key, std::string value) = 0;
	virtual int getInt(std::string section, std::string key, int defaultValue = 0) = 0;
	virtual void setInt(std::string section, std::string key, int value) = 0;
	virtual int getActiveTypeID() = 0;
};
