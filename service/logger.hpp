#pragma once

namespace nsclient {
	class logger {
	public:
		virtual void nsclient_log_error(std::wstring file, int line, std::wstring error) = 0;
	};
};