#include <sysinfo.h>
#include <tchar.h>

namespace systemInfo {
	LANGID GetSystemDefaultUILanguage() {
		HMODULE hKernel = ::LoadLibrary(_TEXT("KERNEL32"));
		if (!hKernel) 
			throw SystemInfoException("Could not load kernel32.dll", GetLastError());
		tGetSystemDefaultUILanguage fGetSystemDefaultUILanguage;
		fGetSystemDefaultUILanguage = (tGetSystemDefaultUILanguage)::GetProcAddress(hKernel, _TEXT("GetSystemDefaultUILanguage"));
		if (!fGetSystemDefaultUILanguage)
			throw SystemInfoException("Could not load GetSystemDefaultUILanguage", GetLastError());
		return fGetSystemDefaultUILanguage();
	}
}