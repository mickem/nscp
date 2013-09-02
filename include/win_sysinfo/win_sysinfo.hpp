#pragma once

#include <vector>

#include <boost/foreach.hpp>

#define WINDOWS_ANCIENT 0
#define WINDOWS_XP 51
#define WINDOWS_SERVER_2003 52
#define WINDOWS_VISTA 60
#define WINDOWS_7 61
#define WINDOWS_8 62
#define WINDOWS_NEW MAXLONG

#define WINDOWS_HAS_CONSOLE_HOST (WindowsVersion >= WINDOWS_7)
#define WINDOWS_HAS_CYCLE_TIME (WindowsVersion >= WINDOWS_VISTA)
#define WINDOWS_HAS_IFILEDIALOG (WindowsVersion >= WINDOWS_VISTA)
#define WINDOWS_HAS_IMAGE_FILE_NAME_BY_PROCESS_ID (WindowsVersion >= WINDOWS_VISTA)
#define WINDOWS_HAS_IMMERSIVE (WindowsVersion >= WINDOWS_8)
#define WINDOWS_HAS_LIMITED_ACCESS (WindowsVersion >= WINDOWS_VISTA)
#define WINDOWS_HAS_PSSUSPENDRESUMEPROCESS (WindowsVersion >= WINDOWS_VISTA)
#define WINDOWS_HAS_SERVICE_TAGS (WindowsVersion >= WINDOWS_VISTA)
#define WINDOWS_HAS_UAC (WindowsVersion >= WINDOWS_VISTA)


namespace windows {
	struct system_info {

		struct load_entry {
			double idle;
			double total;
			double kernel;
			int core;
			load_entry() : idle(0.0), total(0.0), kernel(0.0), core(-1) {}
			void add(const load_entry &other) {
				idle += other.idle;
				total += other.total;
				kernel += other.kernel;
			}
			void normalize(double value) {
				idle /= value;
				total /= value;
				kernel /= value;
			}
		};

		struct cpu_load {
			unsigned long cores;
			std::vector<load_entry> core;
			load_entry total;
			cpu_load() : cores(0) {}
			void add(const cpu_load &n) {
				total.add(n.total);
				cores = max(cores, n.cores);
				core.resize(cores);
				for (unsigned long i=0;i<n.cores; ++i) {
					core[i].add(n.core[i]);
				}
			}
			void normalize(double value) {
				total.normalize(value);
				BOOST_FOREACH(load_entry &c, core) {
					c.normalize(value);
				}
			}
		};

		struct memory_entry {
			unsigned long long total;
			unsigned long long avail;
		};
		struct memory_usage {
			memory_entry physical;
			memory_entry pagefile;
			memory_entry virtual_memory;
		};



		static unsigned long get_version();
		static long get_numberOfProcessorscores();

		static cpu_load get_cpu_load();
		static memory_usage get_memory();

	};

	namespace winapi {


		typedef struct _UNICODE_STRING {
			USHORT Length;
			USHORT MaximumLength;
			PWSTR Buffer;
		} UNICODE_STRING, *PUNICODE_STRING;

		struct NSCP_SERVICE_DELAYED_AUTO_START_INFO {
			BOOL fDelayedAutostart;
		};

		typedef enum _PROCESSINFOCLASS
		{
			ProcessBasicInformation, // 0, q: PROCESS_BASIC_INFORMATION, PROCESS_EXTENDED_BASIC_INFORMATION
			ProcessQuotaLimits, // qs: QUOTA_LIMITS, QUOTA_LIMITS_EX
			ProcessIoCounters, // q: IO_COUNTERS
			ProcessVmCounters, // q: VM_COUNTERS, VM_COUNTERS_EX
			ProcessTimes, // q: KERNEL_USER_TIMES
			ProcessBasePriority, // s: KPRIORITY
			ProcessRaisePriority, // s: ULONG
			ProcessDebugPort, // q: HANDLE
			ProcessExceptionPort, // s: HANDLE
			ProcessAccessToken, // s: PROCESS_ACCESS_TOKEN
			ProcessLdtInformation, // 10
			ProcessLdtSize,
			ProcessDefaultHardErrorMode, // qs: ULONG
			ProcessIoPortHandlers, // (kernel-mode only)
			ProcessPooledUsageAndLimits, // q: POOLED_USAGE_AND_LIMITS
			ProcessWorkingSetWatch, // q: PROCESS_WS_WATCH_INFORMATION[]; s: void
			ProcessUserModeIOPL,
			ProcessEnableAlignmentFaultFixup, // s: BOOLEAN
			ProcessPriorityClass, // qs: PROCESS_PRIORITY_CLASS
			ProcessWx86Information,
			ProcessHandleCount, // 20, q: ULONG, PROCESS_HANDLE_INFORMATION
			ProcessAffinityMask, // s: KAFFINITY
			ProcessPriorityBoost, // qs: ULONG
			ProcessDeviceMap, // qs: PROCESS_DEVICEMAP_INFORMATION, PROCESS_DEVICEMAP_INFORMATION_EX
			ProcessSessionInformation, // q: PROCESS_SESSION_INFORMATION
			ProcessForegroundInformation, // s: PROCESS_FOREGROUND_BACKGROUND
			ProcessWow64Information, // q: ULONG_PTR
			ProcessImageFileName, // q: UNICODE_STRING
			ProcessLUIDDeviceMapsEnabled, // q: ULONG
			ProcessBreakOnTermination, // qs: ULONG
			ProcessDebugObjectHandle, // 30, q: HANDLE
			ProcessDebugFlags, // qs: ULONG
			ProcessHandleTracing, // q: PROCESS_HANDLE_TRACING_QUERY; s: size 0 disables, otherwise enables
			ProcessIoPriority, // qs: ULONG
			ProcessExecuteFlags, // qs: ULONG
			ProcessResourceManagement,
			ProcessCookie, // q: ULONG
			ProcessImageInformation, // q: SECTION_IMAGE_INFORMATION
			ProcessCycleTime, // q: PROCESS_CYCLE_TIME_INFORMATION
			ProcessPagePriority, // q: ULONG
			ProcessInstrumentationCallback, // 40
			ProcessThreadStackAllocation, // qs: PROCESS_STACK_ALLOCATION_INFORMATION
			ProcessWorkingSetWatchEx, // q: PROCESS_WS_WATCH_INFORMATION_EX[]
			ProcessImageFileNameWin32, // q: UNICODE_STRING
			ProcessImageFileMapping, // q: HANDLE (input)
			ProcessAffinityUpdateMode, // qs: PROCESS_AFFINITY_UPDATE_MODE
			ProcessMemoryAllocationMode, // qs: PROCESS_MEMORY_ALLOCATION_MODE
			ProcessGroupInformation, // q: USHORT[]
			ProcessTokenVirtualizationEnabled, // s: ULONG
			ProcessConsoleHostProcess, // q: ULONG_PTR
			ProcessWindowInformation, // 50, q: PROCESS_WINDOW_INFORMATION
			ProcessHandleInformation, // q: PROCESS_HANDLE_SNAPSHOT_INFORMATION // since WIN8
			ProcessMitigationPolicy, // s: PROCESS_MITIGATION_POLICY_INFORMATION
			ProcessDynamicFunctionTableInformation,
			ProcessHandleCheckingMode,
			ProcessKeepAliveCount, // q: PROCESS_KEEPALIVE_COUNT_INFORMATION
			ProcessRevokeFileHandles, // s: PROCESS_REVOKE_FILE_HANDLES_INFORMATION
			MaxProcessInfoClass
		} PROCESSINFOCLASS;

		typedef struct _PROCESS_BASIC_INFORMATION
		{
			LONG ExitStatus;
			LPVOID PebBaseAddress;
			ULONG_PTR AffinityMask;
			LONG BasePriority;
			HANDLE UniqueProcessId;
			HANDLE InheritedFromUniqueProcessId;
		} PROCESS_BASIC_INFORMATION, *PPROCESS_BASIC_INFORMATION;

		typedef struct _PROCESS_EXTENDED_BASIC_INFORMATION
		{
			SIZE_T Size; // set to sizeof structure on input
			PROCESS_BASIC_INFORMATION BasicInfo;
			union
			{
				ULONG Flags;
				struct
				{
					ULONG IsProtectedProcess : 1;
					ULONG IsWow64Process : 1;
					ULONG IsProcessDeleting : 1;
					ULONG IsCrossSessionCreate : 1;
					ULONG SpareBits : 28;
				};
			};
		} PROCESS_EXTENDED_BASIC_INFORMATION, *PPROCESS_EXTENDED_BASIC_INFORMATION;

		typedef struct _VM_COUNTERS
		{
			SIZE_T PeakVirtualSize;
			SIZE_T VirtualSize;
			ULONG PageFaultCount;
			SIZE_T PeakWorkingSetSize;
			SIZE_T WorkingSetSize;
			SIZE_T QuotaPeakPagedPoolUsage;
			SIZE_T QuotaPagedPoolUsage;
			SIZE_T QuotaPeakNonPagedPoolUsage;
			SIZE_T QuotaNonPagedPoolUsage;
			SIZE_T PagefileUsage;
			SIZE_T PeakPagefileUsage;
		} VM_COUNTERS, *PVM_COUNTERS;

		typedef struct _KERNEL_USER_TIMES
		{
			LARGE_INTEGER CreateTime;
			LARGE_INTEGER ExitTime;
			LARGE_INTEGER KernelTime;
			LARGE_INTEGER UserTime;
		} KERNEL_USER_TIMES, *PKERNEL_USER_TIMES;

		// private
		typedef struct _PROCESS_CYCLE_TIME_INFORMATION
		{
			ULONGLONG AccumulatedCycles;
			ULONGLONG CurrentCycleCount;
		} PROCESS_CYCLE_TIME_INFORMATION, *PPROCESS_CYCLE_TIME_INFORMATION;


		typedef BOOL (*tTASKENUMPROCEX)(DWORD dwThreadId, WORD hMod16, WORD hTask16, PSZ pszModName, PSZ pszFileName, LPARAM lpUserDefined );

		BOOL EnumServicesStatusEx(SC_HANDLE hSCManager, SC_ENUM_TYPE InfoLevel, DWORD dwServiceType, DWORD dwServiceState, LPBYTE lpServices, DWORD cbBufSize, LPDWORD pcbBytesNeeded, LPDWORD lpServicesReturned, LPDWORD lpResumeHandle, LPCTSTR pszGroupName);
		BOOL QueryServiceConfig2(SC_HANDLE hService, DWORD dwInfoLevel, LPBYTE lpBuffer, DWORD cbBufSize, LPDWORD pcbBytesNeeded);
		BOOL QueryServiceStatusEx(SC_HANDLE hService, SC_STATUS_TYPE InfoLevel, LPBYTE lpBuffer, DWORD cbBufSize, LPDWORD pcbBytesNeeded);
		bool IsWow64(HANDLE hProcess, bool def = false);
		DWORD GetProcessImageFileName(HANDLE hProcess, LPWSTR lpImageFileName, DWORD nSize);
		LONG NtQueryInformationProcess(HANDLE ProcessHandle, DWORD ProcessInformationClass, PVOID ProcessInformation, DWORD ProcessInformationLength, PDWORD ReturnLength);
		INT VDMEnumTaskWOWEx(DWORD dwProcessId, tTASKENUMPROCEX fp, LPARAM lparam);
	};

};