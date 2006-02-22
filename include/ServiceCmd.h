#pragma once

#include <sstream>

namespace serviceControll {
	class SCException {
	public:
		std::string error_;
		SCException(std::string error) : error_(error) {
		}
		SCException(std::string error, int code) : error_(error) {
			std::stringstream ss;
			ss << ": ";
			ss << code;
			error += ss.str();
		}
	};
	void Install(LPCTSTR,LPCTSTR,LPCTSTR,DWORD=SERVICE_WIN32_OWN_PROCESS);
	void ModifyServiceType(LPCTSTR szName, DWORD dwServiceType);
	void Uninstall(std::string);
	void Start(std::string);
	void Stop(std::string);
	void SetDescription(std::string,std::string);
	DWORD GetServiceType(LPCTSTR szName);
}