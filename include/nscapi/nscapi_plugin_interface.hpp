#pragma once

#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/macros.hpp>

namespace nscapi {

	class plugin_wrapper {
	public:
		struct module_version {
			int major;
			int minor;
			int revision;
		};

		plugin_wrapper() {}
		int wrapReturnString(wchar_t *buffer, unsigned int bufLen, std::wstring str, int defaultReturnCode);
		int wrapModuleHelperInit(unsigned int id, nscapi::core_api::lpNSAPILoader f);
		NSCAPI::errorReturn wrapGetModuleName(wchar_t* buf, unsigned int buflen, std::wstring str);
		int wrapLoadModule(bool success);
		NSCAPI::errorReturn wrapGetModuleVersion(int *major, int *minor, int *revision, module_version version);
		NSCAPI::boolReturn wrapHasCommandHandler(bool has);
		NSCAPI::boolReturn wrapHasMessageHandler(bool has);
		NSCAPI::boolReturn wrapHasRoutingHandler(bool has);
		NSCAPI::boolReturn wrapHasNotificationHandler(bool has);
		NSCAPI::nagiosReturn wrapHandleNotification(NSCAPI::nagiosReturn retResult);
		int wrapUnloadModule(bool success);
		NSCAPI::nagiosReturn wrapHandleCommand(NSCAPI::nagiosReturn retResult, const std::string &reply, char **reply_buffer, unsigned int *size);
		NSCAPI::nagiosReturn wrapCommandLineExec(NSCAPI::nagiosReturn retResult, const std::string &reply, char **reply_buffer, unsigned int *size);
		NSCAPI::nagiosReturn wrapRouteMessage(NSCAPI::nagiosReturn retResult);
		void wrapDeleteBuffer(char**buffer);
	};

	class helper_singleton {
		core_wrapper* core_;
		plugin_wrapper *plugin_;
	public:
		helper_singleton();
		core_wrapper* get_core() const {
			return core_;
		}
		plugin_wrapper* get_plugin() const {
			return plugin_;
		}
	};

	extern helper_singleton* plugin_singleton;
}