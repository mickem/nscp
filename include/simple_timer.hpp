#pragma once

class simple_timer {
	unsigned long long start_time;
	bool log;
	std::wstring text;
public:
	simple_timer() {
		start();
	}
	simple_timer(std::wstring text, bool log) : text(text), log(log) {
		start();
	}
	~simple_timer() {
		if (log)
			std::cout << text << stop() << std::endl;;
	}

	void start() {
		start_time = getFT();
	}
	unsigned long long stop() {
		unsigned int  ret = getFT() - start_time;
		start();
		return ret/1000;
	}

private:
	unsigned long long getFT() {
		SYSTEMTIME systemTime;
		GetSystemTime( &systemTime );
		FILETIME fileTime;
		SystemTimeToFileTime( &systemTime, &fileTime );
		return  static_cast<unsigned long long>(fileTime.dwHighDateTime) << 32 | fileTime.dwLowDateTime;
	}

};