#pragma once

#include <string>
#include <map>


class WMIException {
public:
	WMIException(std::string str, HRESULT code) {
		std::cout << str << std::endl;

	}
};
class WMIQuery
{
private:
	bool bInitialized;

public:
	WMIQuery(void);
	~WMIQuery(void);

	std::map<std::string,int> execute(std::string query);

	bool initialize();
	void unInitialize();
};
