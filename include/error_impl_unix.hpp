#pragma once
#include <cstdarg>
#include <unicode_char.hpp>
#include <string>

namespace error {
	class format {
	public:
		static std::string from_system(int dwError) {
			char buf [1024];
			return ::strerror_r(dwError, buf, sizeof (buf));
		}
		static std::string from_module(std::wstring module, unsigned long dwError) {
			return "ERROR TODO";
		}
		static std::string from_module(std::wstring module, unsigned long dwError, unsigned long *arguments) {
			return "ERROR TODO";
		}
		class message {
		public:
			static std::string from_module(std::wstring module, unsigned long dwError) {
				return "ERROR TODO";
			}

			static std::string from_module_x64(std::wstring module, unsigned long dwError, wchar_t* argList[], unsigned long argCount) {
				return "ERROR TODO";
			}
		private:

			static std::string from_module_wrapper(std::wstring module, unsigned long dwError, ...) {
				return "ERROR TODO";
			}

			static std::string __from_module(std::wstring module, unsigned long dwError, va_list *arguments) {
				return "ERROR TODO";
			}
		public:
			static std::string from_system(unsigned long dwError, unsigned long *arguments) {
				return "ERROR TODO";
			}
		};
	};
	class lookup {
	public:
		static std::string last_error(int dwLastError = -1) {
			return ::error::format::from_system(dwLastError);
		}
	};
}
