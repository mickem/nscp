#include <win/psapi.hpp>
#include <win/windows.hpp>
#include <breakpad/exception_handler_win32.hpp>
#include <eh.h>
#include <exception>
#include <file_helpers.hpp>
#include <iostream>
#include <str/format.hpp>
#include <string>
#include <boost/date_time/posix_time/posix_time.hpp>

ExceptionManager *instance_ = NULL;

ExceptionManager::ExceptionManager() : restart_(false) {}

ExceptionManager::~ExceptionManager() {}

std::string modulePath() {
  unsigned int buf_len = 4096;
  TCHAR *buffer = new TCHAR[buf_len + 1];
  GetModuleFileName(NULL, buffer, buf_len);
  std::string path = utf8::cvt<std::string>(buffer);
  delete[] buffer;
  std::string::size_type pos = path.rfind('\\');
  return path.substr(0, pos);
}

void report_error(std::string err) { std::cout << "ERR: " << err << std::endl; }
void report_info(std::string err) { std::cout << "INF: " << err << std::endl; }

void run_proc(std::string command_line) {
  report_info("Running: " + command_line);
  STARTUPINFO startup_info = {0};
  startup_info.cb = sizeof(startup_info);
  PROCESS_INFORMATION process_info = {0};
  CreateProcessW(NULL, const_cast<wchar_t *>(utf8::cvt<std::wstring>(command_line).c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &startup_info, &process_info);
  CloseHandle(process_info.hProcess);
  CloseHandle(process_info.hThread);
}

void ExceptionManager::setup_path(std::string crash_path) { crash_path_ = crash_path; }

void ExceptionManager::setup_service_name(std::string service) { service_ = service; }

void ExceptionManager::setup_restart_flag() { restart_ = true; }

void ExceptionManager::setup_app(std::string application, std::string version, std::string date) {
  application_ = application;
  version_ = version;
  date_ = date;
}

ExceptionManager *ExceptionManager::instance() {
  if (instance_ == NULL) {
    instance_ = new ExceptionManager();
  }
  return instance_;
}

static const char *opDescription(const ULONG opcode) {
  switch (opcode) {
    case 0:
      return "read";
    case 1:
      return "write";
    case 8:
      return "user-mode data execution prevention (DEP) violation";
    default:
      return "unknown";
  }
}

static const char *seDescription(const DWORD &code) {
  switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
      return "EXCEPTION_ACCESS_VIOLATION";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
      return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_BREAKPOINT:
      return "EXCEPTION_BREAKPOINT";
    case EXCEPTION_DATATYPE_MISALIGNMENT:
      return "EXCEPTION_DATATYPE_MISALIGNMENT";
    case EXCEPTION_FLT_DENORMAL_OPERAND:
      return "EXCEPTION_FLT_DENORMAL_OPERAND";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
      return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_INEXACT_RESULT:
      return "EXCEPTION_FLT_INEXACT_RESULT";
    case EXCEPTION_FLT_INVALID_OPERATION:
      return "EXCEPTION_FLT_INVALID_OPERATION";
    case EXCEPTION_FLT_OVERFLOW:
      return "EXCEPTION_FLT_OVERFLOW";
    case EXCEPTION_FLT_STACK_CHECK:
      return "EXCEPTION_FLT_STACK_CHECK";
    case EXCEPTION_FLT_UNDERFLOW:
      return "EXCEPTION_FLT_UNDERFLOW";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
      return "EXCEPTION_ILLEGAL_INSTRUCTION";
    case EXCEPTION_IN_PAGE_ERROR:
      return "EXCEPTION_IN_PAGE_ERROR";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
      return "EXCEPTION_INT_DIVIDE_BY_ZERO";
    case EXCEPTION_INT_OVERFLOW:
      return "EXCEPTION_INT_OVERFLOW";
    case EXCEPTION_INVALID_DISPOSITION:
      return "EXCEPTION_INVALID_DISPOSITION";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
      return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
    case EXCEPTION_PRIV_INSTRUCTION:
      return "EXCEPTION_PRIV_INSTRUCTION";
    case EXCEPTION_SINGLE_STEP:
      return "EXCEPTION_SINGLE_STEP";
    case EXCEPTION_STACK_OVERFLOW:
      return "EXCEPTION_STACK_OVERFLOW";
    default:
      return "UNKNOWN EXCEPTION";
  }
}

LONG ExceptionManager::HandleException(EXCEPTION_POINTERS *exinfo) {
  ExceptionManager *self = ExceptionManager::instance();
  self->handle_exception(exinfo);
  return EXCEPTION_CONTINUE_SEARCH;
}

void ExceptionManager::handle_exception(EXCEPTION_POINTERS *exinfo) {
  boost::posix_time::ptime time(boost::posix_time::second_clock::local_time());
  std::string date = str::format::format_date(time, "%Y%m%d-%H%M%S");

  if (!crash_path_.empty()) {
    if (!boost::filesystem::is_directory(crash_path_)) {
      boost::filesystem::create_directories(crash_path_);
    }

    if (!file_helpers::checks::is_directory(crash_path_)) {
      report_error("Crash path does not exist: " + crash_path_);
      return;
    }
    std::string crash_file = crash_path_ + "\\" + date + ".crash";
    report_error("Crash path: " + crash_file);
    if (crash_file.length() >= MAX_PATH) {
      report_error("Path to long");
      return;
    }

    HMODULE hm;
    ::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, static_cast<LPCTSTR>(exinfo->ExceptionRecord->ExceptionAddress), &hm);
    MODULEINFO mi;
    ::GetModuleInformation(::GetCurrentProcess(), hm, &mi, sizeof(mi));
    wchar_t fn[MAX_PATH];
    ::GetModuleFileNameEx(::GetCurrentProcess(), hm, fn, MAX_PATH);

    DWORD code = exinfo->ExceptionRecord->ExceptionCode;
    std::ofstream oss(crash_file, std::ios::out);
    oss << "SE " << seDescription(code) << " at address 0x" << std::hex << exinfo->ExceptionRecord->ExceptionAddress << std::dec << " inside " << fn
        << " loaded at base address 0x" << std::hex << mi.lpBaseOfDll << "\n";

    if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR) {
      oss << "Invalid operation: " << opDescription(exinfo->ExceptionRecord->ExceptionInformation[0]) << " at address 0x" << std::hex
          << exinfo->ExceptionRecord->ExceptionInformation[1] << std::dec << "\n";
    }

    if (code == EXCEPTION_IN_PAGE_ERROR) {
      oss << "Underlying NTSTATUS code that resulted in the exception " << exinfo->ExceptionRecord->ExceptionInformation[2] << "\n";
    }
  }

  std::string path = modulePath() + "\\nscp.exe";
  if (path.length() >= MAX_PATH) {
    report_error("Path to long");
    return;
  }

  if (restart_) {
    run_proc("\"" + path + "\" service --restart");
  }
}

void ExceptionManager::StartMonitoring() { SetUnhandledExceptionFilter(HandleException); }