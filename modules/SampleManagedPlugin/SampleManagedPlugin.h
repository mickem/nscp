// SampleManagedPlugin.h

#pragma once

#include <vcclr.h>
#include <strEx.h>
#using <mscorlib.dll>
using namespace System;
using namespace System::Runtime::InteropServices;
using namespace System::Collections::Generic;

template <class target>
class SampleManagedPlugin {
	gcroot<target^> managedInstance;
private:
	std::wstring strcvt(String^ str) {
		pin_ptr<const wchar_t> wch = PtrToStringChars(str);
		std::wstring ret = wch;
		return ret;
	}
	String^ strcvt(std::wstring str) {
		return Marshal::PtrToStringUni((IntPtr)const_cast<wchar_t*>(str.c_str()));
	}
public:
	bool loadModule() {
		return managedInstance->loadModule();
	}
	bool unloadModule() {
		return managedInstance->unloadModule();
	}

	std::wstring getModuleName() {
		return strcvt(managedInstance->getModuleName());
	}
	std::wstring getModuleDescription() {
		return strcvt(managedInstance->getModuleDescription());
	}
	NSCModuleWrapper::module_version getModuleVersion() {
		gcroot<CsharpSamplePlugin::SamplePlugin::version^> v = managedInstance->getModuleVersion();
		NSCModuleWrapper::module_version version = {v->major, v->minor, v->revision};
		return version;
	}

	bool hasCommandHandler() {
		return managedInstance->hasCommandHandler();
	}
	bool hasMessageHandler() {
		return managedInstance->hasMessageHandler();
	}
	NSCAPI::nagiosReturn handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf) {
		String ^mCmd = strcvt(command.c_str());
		List<String^>^ args;
		for (int i=0;i<argLen;i++)
			args->Add(strcvt(char_args[i]));
		String ^mMsg;
		String ^mPerf;
		int ret = managedInstance->handleCommand(mCmd, args, mMsg, mPerf);
		message = strcvt(mMsg);
		perf = strcvt(mPerf);
		return ret;
	}
};

NSC_WRAPPERS_MAIN();

