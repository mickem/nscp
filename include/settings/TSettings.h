#pragma once

#include <string>
#include <windows.h>
#define BUFF_LEN 4096

class settings_base
{
public:
	typedef std::list<std::wstring> sectionList;
	settings_base(void)
	{
	}

	virtual ~settings_base(void)
	{
	}
	virtual std::wstring getActiveType() = 0;
	virtual sectionList getSections(unsigned int bufferLength = BUFF_LEN) = 0;
	virtual sectionList getSection(std::wstring section, unsigned int bufferLength = BUFF_LEN) = 0;
	virtual std::wstring getString(std::wstring section, std::wstring key, std::wstring defaultValue = _T("")) const = 0;
	virtual void setString(std::wstring section, std::wstring key, std::wstring value) = 0;
	virtual int getInt(std::wstring section, std::wstring key, int defaultValue = 0) = 0;
	virtual void setInt(std::wstring section, std::wstring key, int value) = 0;
	virtual int getActiveTypeID() = 0;
	virtual void setSection(std::wstring section, sectionList data) = 0;

};
