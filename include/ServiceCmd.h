#pragma once

namespace serviceControll {
	class SCException {
	public:
		std::string error_;
		SCException(std::string error) : error_(error) {
		}
	};
	void Install(LPCTSTR,LPCTSTR,LPCTSTR);
	void Uninstall(std::string);
	void Start(std::string);
	void Stop(std::string);
}