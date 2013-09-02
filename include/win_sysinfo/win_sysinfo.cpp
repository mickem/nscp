#include <windows.h>

#include <win_sysinfo/win_sysinfo.hpp>

#include <boost/scoped_array.hpp>
#include <error.hpp>


typedef enum _SYSTEM_INFORMATION_CLASS
{
	SystemBasicInformation, // q: SYSTEM_BASIC_INFORMATION
	SystemProcessorInformation, // q: SYSTEM_PROCESSOR_INFORMATION
	SystemPerformanceInformation, // q: SYSTEM_PERFORMANCE_INFORMATION
	SystemTimeOfDayInformation, // q: SYSTEM_TIMEOFDAY_INFORMATION
	SystemPathInformation, // not implemented
	SystemProcessInformation, // q: SYSTEM_PROCESS_INFORMATION
	SystemCallCountInformation, // q: SYSTEM_CALL_COUNT_INFORMATION
	SystemDeviceInformation, // q: SYSTEM_DEVICE_INFORMATION
	SystemProcessorPerformanceInformation, // q: SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION
	SystemFlagsInformation, // q: SYSTEM_FLAGS_INFORMATION
	SystemCallTimeInformation, // 10, not implemented
	SystemModuleInformation, // q: RTL_PROCESS_MODULES
	SystemLocksInformation,
	SystemStackTraceInformation,
	SystemPagedPoolInformation, // not implemented
	SystemNonPagedPoolInformation, // not implemented
	SystemHandleInformation, // q: SYSTEM_HANDLE_INFORMATION
	SystemObjectInformation, // q: SYSTEM_OBJECTTYPE_INFORMATION mixed with SYSTEM_OBJECT_INFORMATION
	SystemPageFileInformation, // q: SYSTEM_PAGEFILE_INFORMATION
	SystemVdmInstemulInformation, // q
	SystemVdmBopInformation, // 20, not implemented
	SystemFileCacheInformation, // q: SYSTEM_FILECACHE_INFORMATION; s (requires SeIncreaseQuotaPrivilege) (info for WorkingSetTypeSystemCache)
	SystemPoolTagInformation, // q: SYSTEM_POOLTAG_INFORMATION
	SystemInterruptInformation, // q: SYSTEM_INTERRUPT_INFORMATION
	SystemDpcBehaviorInformation, // q: SYSTEM_DPC_BEHAVIOR_INFORMATION; s: SYSTEM_DPC_BEHAVIOR_INFORMATION (requires SeLoadDriverPrivilege)
	SystemFullMemoryInformation, // not implemented
	SystemLoadGdiDriverInformation, // s (kernel-mode only)
	SystemUnloadGdiDriverInformation, // s (kernel-mode only)
	SystemTimeAdjustmentInformation, // q: SYSTEM_QUERY_TIME_ADJUST_INFORMATION; s: SYSTEM_SET_TIME_ADJUST_INFORMATION (requires SeSystemtimePrivilege)
	SystemSummaryMemoryInformation, // not implemented
	SystemMirrorMemoryInformation, // 30, s (requires license value "Kernel-MemoryMirroringSupported") (requires SeShutdownPrivilege)
	SystemPerformanceTraceInformation, // s
	SystemObsolete0, // not implemented
	SystemExceptionInformation, // q: SYSTEM_EXCEPTION_INFORMATION
	SystemCrashDumpStateInformation, // s (requires SeDebugPrivilege)
	SystemKernelDebuggerInformation, // q: SYSTEM_KERNEL_DEBUGGER_INFORMATION
	SystemContextSwitchInformation, // q: SYSTEM_CONTEXT_SWITCH_INFORMATION
	SystemRegistryQuotaInformation, // q: SYSTEM_REGISTRY_QUOTA_INFORMATION; s (requires SeIncreaseQuotaPrivilege)
	SystemExtendServiceTableInformation, // s (requires SeLoadDriverPrivilege) // loads win32k only
	SystemPrioritySeperation, // s (requires SeTcbPrivilege)
	SystemVerifierAddDriverInformation, // 40, s (requires SeDebugPrivilege)
	SystemVerifierRemoveDriverInformation, // s (requires SeDebugPrivilege)
	SystemProcessorIdleInformation, // q: SYSTEM_PROCESSOR_IDLE_INFORMATION
	SystemLegacyDriverInformation, // q: SYSTEM_LEGACY_DRIVER_INFORMATION
	SystemCurrentTimeZoneInformation, // q
	SystemLookasideInformation, // q: SYSTEM_LOOKASIDE_INFORMATION
	SystemTimeSlipNotification, // s (requires SeSystemtimePrivilege)
	SystemSessionCreate, // not implemented
	SystemSessionDetach, // not implemented
	SystemSessionInformation, // not implemented
	SystemRangeStartInformation, // 50, q
	SystemVerifierInformation, // q: SYSTEM_VERIFIER_INFORMATION; s (requires SeDebugPrivilege)
	SystemVerifierThunkExtend, // s (kernel-mode only)
	SystemSessionProcessInformation, // q: SYSTEM_SESSION_PROCESS_INFORMATION
	SystemLoadGdiDriverInSystemSpace, // s (kernel-mode only) (same as SystemLoadGdiDriverInformation)
	SystemNumaProcessorMap, // q
	SystemPrefetcherInformation, // q: PREFETCHER_INFORMATION; s: PREFETCHER_INFORMATION // PfSnQueryPrefetcherInformation
	SystemExtendedProcessInformation, // q: SYSTEM_PROCESS_INFORMATION
	SystemRecommendedSharedDataAlignment, // q
	SystemComPlusPackage, // q; s
	SystemNumaAvailableMemory, // 60
	SystemProcessorPowerInformation, // q: SYSTEM_PROCESSOR_POWER_INFORMATION
	SystemEmulationBasicInformation, // q
	SystemEmulationProcessorInformation,
	SystemExtendedHandleInformation, // q: SYSTEM_HANDLE_INFORMATION_EX
	SystemLostDelayedWriteInformation, // q: ULONG
	SystemBigPoolInformation, // q: SYSTEM_BIGPOOL_INFORMATION
	SystemSessionPoolTagInformation, // q: SYSTEM_SESSION_POOLTAG_INFORMATION
	SystemSessionMappedViewInformation, // q: SYSTEM_SESSION_MAPPED_VIEW_INFORMATION
	SystemHotpatchInformation, // q; s
	SystemObjectSecurityMode, // 70, q
	SystemWatchdogTimerHandler, // s (kernel-mode only)
	SystemWatchdogTimerInformation, // q (kernel-mode only); s (kernel-mode only)
	SystemLogicalProcessorInformation, // q: SYSTEM_LOGICAL_PROCESSOR_INFORMATION
	SystemWow64SharedInformationObsolete, // not implemented
	SystemRegisterFirmwareTableInformationHandler, // s (kernel-mode only)
	SystemFirmwareTableInformation, // not implemented
	SystemModuleInformationEx, // q: RTL_PROCESS_MODULE_INFORMATION_EX
	SystemVerifierTriageInformation, // not implemented
	SystemSuperfetchInformation, // q: SUPERFETCH_INFORMATION; s: SUPERFETCH_INFORMATION // PfQuerySuperfetchInformation
	SystemMemoryListInformation, // 80, q: SYSTEM_MEMORY_LIST_INFORMATION; s: SYSTEM_MEMORY_LIST_COMMAND (requires SeProfileSingleProcessPrivilege)
	SystemFileCacheInformationEx, // q: SYSTEM_FILECACHE_INFORMATION; s (requires SeIncreaseQuotaPrivilege) (same as SystemFileCacheInformation)
	SystemThreadPriorityClientIdInformation, // s: SYSTEM_THREAD_CID_PRIORITY_INFORMATION (requires SeIncreaseBasePriorityPrivilege)
	SystemProcessorIdleCycleTimeInformation, // q: SYSTEM_PROCESSOR_IDLE_CYCLE_TIME_INFORMATION[]
	SystemVerifierCancellationInformation, // not implemented // name:wow64:whNT32QuerySystemVerifierCancellationInformation
	SystemProcessorPowerInformationEx, // not implemented
	SystemRefTraceInformation, // q; s // ObQueryRefTraceInformation
	SystemSpecialPoolInformation, // q; s (requires SeDebugPrivilege) // MmSpecialPoolTag, then MmSpecialPoolCatchOverruns != 0
	SystemProcessIdInformation, // q: SYSTEM_PROCESS_ID_INFORMATION
	SystemErrorPortInformation, // s (requires SeTcbPrivilege)
	SystemBootEnvironmentInformation, // 90, q: SYSTEM_BOOT_ENVIRONMENT_INFORMATION
	SystemHypervisorInformation, // q; s (kernel-mode only)
	SystemVerifierInformationEx, // q; s
	SystemTimeZoneInformation, // s (requires SeTimeZonePrivilege)
	SystemImageFileExecutionOptionsInformation, // s: SYSTEM_IMAGE_FILE_EXECUTION_OPTIONS_INFORMATION (requires SeTcbPrivilege)
	SystemCoverageInformation, // q; s // name:wow64:whNT32QuerySystemCoverageInformation; ExpCovQueryInformation
	SystemPrefetchPatchInformation, // not implemented
	SystemVerifierFaultsInformation, // s (requires SeDebugPrivilege)
	SystemSystemPartitionInformation, // q: SYSTEM_SYSTEM_PARTITION_INFORMATION
	SystemSystemDiskInformation, // q: SYSTEM_SYSTEM_DISK_INFORMATION
	SystemProcessorPerformanceDistribution, // 100, q: SYSTEM_PROCESSOR_PERFORMANCE_DISTRIBUTION
	SystemNumaProximityNodeInformation, // q
	SystemDynamicTimeZoneInformation, // q; s (requires SeTimeZonePrivilege)
	SystemCodeIntegrityInformation, // q // SeCodeIntegrityQueryInformation
	SystemProcessorMicrocodeUpdateInformation, // s
	SystemProcessorBrandString, // q // HaliQuerySystemInformation -> HalpGetProcessorBrandString, info class 23
	SystemVirtualAddressInformation, // q: SYSTEM_VA_LIST_INFORMATION[]; s: SYSTEM_VA_LIST_INFORMATION[] (requires SeIncreaseQuotaPrivilege) // MmQuerySystemVaInformation
	SystemLogicalProcessorAndGroupInformation, // q: SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX // since WIN7 // KeQueryLogicalProcessorRelationship
	SystemProcessorCycleTimeInformation, // q: SYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION[]
	SystemStoreInformation, // q; s // SmQueryStoreInformation
	SystemRegistryAppendString, // 110, s: SYSTEM_REGISTRY_APPEND_STRING_PARAMETERS
	SystemAitSamplingValue, // s: ULONG (requires SeProfileSingleProcessPrivilege)
	SystemVhdBootInformation, // q: SYSTEM_VHD_BOOT_INFORMATION
	SystemCpuQuotaInformation, // q; s // PsQueryCpuQuotaInformation
	SystemNativeBasicInformation, // not implemented
	SystemSpare1, // not implemented
	SystemLowPriorityIoInformation, // q: SYSTEM_LOW_PRIORITY_IO_INFORMATION
	SystemTpmBootEntropyInformation, // q: TPM_BOOT_ENTROPY_NT_RESULT // ExQueryTpmBootEntropyInformation
	SystemVerifierCountersInformation, // q: SYSTEM_VERIFIER_COUNTERS_INFORMATION
	SystemPagedPoolInformationEx, // q: SYSTEM_FILECACHE_INFORMATION; s (requires SeIncreaseQuotaPrivilege) (info for WorkingSetTypePagedPool)
	SystemSystemPtesInformationEx, // 120, q: SYSTEM_FILECACHE_INFORMATION; s (requires SeIncreaseQuotaPrivilege) (info for WorkingSetTypeSystemPtes)
	SystemNodeDistanceInformation, // q
	SystemAcpiAuditInformation, // q: SYSTEM_ACPI_AUDIT_INFORMATION // HaliQuerySystemInformation -> HalpAuditQueryResults, info class 26
	SystemBasicPerformanceInformation, // q: SYSTEM_BASIC_PERFORMANCE_INFORMATION // name:wow64:whNtQuerySystemInformation_SystemBasicPerformanceInformation
	SystemQueryPerformanceCounterInformation, // q: SYSTEM_QUERY_PERFORMANCE_COUNTER_INFORMATION // since WIN7 SP1
	MaxSystemInfoClass
} SYSTEM_INFORMATION_CLASS;

typedef struct _SYSTEM_BASIC_INFORMATION
{
	ULONG Reserved;
	ULONG TimerResolution;
	ULONG PageSize;
	ULONG NumberOfPhysicalPages;
	ULONG LowestPhysicalPageNumber;
	ULONG HighestPhysicalPageNumber;
	ULONG AllocationGranularity;
	ULONG_PTR MinimumUserModeAddress;
	ULONG_PTR MaximumUserModeAddress;
	ULONG_PTR ActiveProcessorsAffinityMask;
	CCHAR NumberOfProcessors;
} SYSTEM_BASIC_INFORMATION, *PSYSTEM_BASIC_INFORMATION;

typedef struct _SYSTEM_PROCESSOR_INFORMATION
{
	USHORT ProcessorArchitecture;
	USHORT ProcessorLevel;
	USHORT ProcessorRevision;
	USHORT Reserved;
	ULONG ProcessorFeatureBits;
} SYSTEM_PROCESSOR_INFORMATION, *PSYSTEM_PROCESSOR_INFORMATION;

typedef struct _SYSTEM_PERFORMANCE_INFORMATION
{
	LARGE_INTEGER IdleProcessTime;
	LARGE_INTEGER IoReadTransferCount;
	LARGE_INTEGER IoWriteTransferCount;
	LARGE_INTEGER IoOtherTransferCount;
	ULONG IoReadOperationCount;
	ULONG IoWriteOperationCount;
	ULONG IoOtherOperationCount;
	ULONG AvailablePages;
	ULONG CommittedPages;
	ULONG CommitLimit;
	ULONG PeakCommitment;
	ULONG PageFaultCount;
	ULONG CopyOnWriteCount;
	ULONG TransitionCount;
	ULONG CacheTransitionCount;
	ULONG DemandZeroCount;
	ULONG PageReadCount;
	ULONG PageReadIoCount;
	ULONG CacheReadCount;
	ULONG CacheIoCount;
	ULONG DirtyPagesWriteCount;
	ULONG DirtyWriteIoCount;
	ULONG MappedPagesWriteCount;
	ULONG MappedWriteIoCount;
	ULONG PagedPoolPages;
	ULONG NonPagedPoolPages;
	ULONG PagedPoolAllocs;
	ULONG PagedPoolFrees;
	ULONG NonPagedPoolAllocs;
	ULONG NonPagedPoolFrees;
	ULONG FreeSystemPtes;
	ULONG ResidentSystemCodePage;
	ULONG TotalSystemDriverPages;
	ULONG TotalSystemCodePages;
	ULONG NonPagedPoolLookasideHits;
	ULONG PagedPoolLookasideHits;
	ULONG AvailablePagedPoolPages;
	ULONG ResidentSystemCachePage;
	ULONG ResidentPagedPoolPage;
	ULONG ResidentSystemDriverPage;
	ULONG CcFastReadNoWait;
	ULONG CcFastReadWait;
	ULONG CcFastReadResourceMiss;
	ULONG CcFastReadNotPossible;
	ULONG CcFastMdlReadNoWait;
	ULONG CcFastMdlReadWait;
	ULONG CcFastMdlReadResourceMiss;
	ULONG CcFastMdlReadNotPossible;
	ULONG CcMapDataNoWait;
	ULONG CcMapDataWait;
	ULONG CcMapDataNoWaitMiss;
	ULONG CcMapDataWaitMiss;
	ULONG CcPinMappedDataCount;
	ULONG CcPinReadNoWait;
	ULONG CcPinReadWait;
	ULONG CcPinReadNoWaitMiss;
	ULONG CcPinReadWaitMiss;
	ULONG CcCopyReadNoWait;
	ULONG CcCopyReadWait;
	ULONG CcCopyReadNoWaitMiss;
	ULONG CcCopyReadWaitMiss;
	ULONG CcMdlReadNoWait;
	ULONG CcMdlReadWait;
	ULONG CcMdlReadNoWaitMiss;
	ULONG CcMdlReadWaitMiss;
	ULONG CcReadAheadIos;
	ULONG CcLazyWriteIos;
	ULONG CcLazyWritePages;
	ULONG CcDataFlushes;
	ULONG CcDataPages;
	ULONG ContextSwitches;
	ULONG FirstLevelTbFills;
	ULONG SecondLevelTbFills;
	ULONG SystemCalls;
} SYSTEM_PERFORMANCE_INFORMATION, *PSYSTEM_PERFORMANCE_INFORMATION;

typedef struct _SYSTEM_TIMEOFDAY_INFORMATION
{
	LARGE_INTEGER BootTime;
	LARGE_INTEGER CurrentTime;
	LARGE_INTEGER TimeZoneBias;
	ULONG TimeZoneId;
	ULONG Reserved;
	ULONGLONG BootTimeBias;
	ULONGLONG SleepTimeBias;
} SYSTEM_TIMEOFDAY_INFORMATION, *PSYSTEM_TIMEOFDAY_INFORMATION;

typedef struct _CLIENT_ID
{
	HANDLE UniqueProcess;
	HANDLE UniqueThread;
} CLIENT_ID, *PCLIENT_ID;

typedef LONG KPRIORITY;

typedef enum _KWAIT_REASON
{
	Executive,
	FreePage,
	PageIn,
	PoolAllocation,
	DelayExecution,
	Suspended,
	UserRequest,
	WrExecutive,
	WrFreePage,
	WrPageIn,
	WrPoolAllocation,
	WrDelayExecution,
	WrSuspended,
	WrUserRequest,
	WrEventPair,
	WrQueue,
	WrLpcReceive,
	WrLpcReply,
	WrVirtualMemory,
	WrPageOut,
	WrRendezvous,
	WrKeyedEvent,
	WrTerminated,
	WrProcessInSwap,
	WrCpuRateControl,
	WrCalloutStack,
	WrKernel,
	WrResource,
	WrPushLock,
	WrMutex,
	WrQuantumEnd,
	WrDispatchInt,
	WrPreempted,
	WrYieldExecution,
	WrFastMutex,
	WrGuardedMutex,
	WrRundown,
	MaximumWaitReason
} KWAIT_REASON, *PKWAIT_REASON;

typedef struct _SYSTEM_THREAD_INFORMATION
{
	LARGE_INTEGER KernelTime;
	LARGE_INTEGER UserTime;
	LARGE_INTEGER CreateTime;
	ULONG WaitTime;
	PVOID StartAddress;
	CLIENT_ID ClientId;
	KPRIORITY Priority;
	LONG BasePriority;
	ULONG ContextSwitches;
	ULONG ThreadState;
	KWAIT_REASON WaitReason;
} SYSTEM_THREAD_INFORMATION, *PSYSTEM_THREAD_INFORMATION;

typedef struct _TEB *PTEB;

// private
typedef struct _SYSTEM_EXTENDED_THREAD_INFORMATION
{
	SYSTEM_THREAD_INFORMATION ThreadInfo;
	PVOID StackBase;
	PVOID StackLimit;
	PVOID Win32StartAddress;
	PTEB TebBase;
	ULONG_PTR Reserved2;
	ULONG_PTR Reserved3;
	ULONG_PTR Reserved4;
} SYSTEM_EXTENDED_THREAD_INFORMATION, *PSYSTEM_EXTENDED_THREAD_INFORMATION;

typedef struct _UNICODE_STRING
{
	USHORT Length;
	USHORT MaximumLength;
	__field_bcount_part(MaximumLength, Length) PWCH Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

typedef const UNICODE_STRING *PCUNICODE_STRING;

typedef struct _SYSTEM_PROCESS_INFORMATION
{
	ULONG NextEntryOffset;
	ULONG NumberOfThreads;
	LARGE_INTEGER WorkingSetPrivateSize; // since VISTA
	ULONG HardFaultCount; // since WIN7
	ULONG NumberOfThreadsHighWatermark; // since WIN7
	ULONGLONG CycleTime; // since WIN7
	LARGE_INTEGER CreateTime;
	LARGE_INTEGER UserTime;
	LARGE_INTEGER KernelTime;
	UNICODE_STRING ImageName;
	KPRIORITY BasePriority;
	HANDLE UniqueProcessId;
	HANDLE InheritedFromUniqueProcessId;
	ULONG HandleCount;
	ULONG SessionId;
	ULONG_PTR UniqueProcessKey; // since VISTA (requires SystemExtendedProcessInformation)
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
	SIZE_T PrivatePageCount;
	LARGE_INTEGER ReadOperationCount;
	LARGE_INTEGER WriteOperationCount;
	LARGE_INTEGER OtherOperationCount;
	LARGE_INTEGER ReadTransferCount;
	LARGE_INTEGER WriteTransferCount;
	LARGE_INTEGER OtherTransferCount;
	SYSTEM_THREAD_INFORMATION Threads[1];
} SYSTEM_PROCESS_INFORMATION, *PSYSTEM_PROCESS_INFORMATION;

typedef struct _SYSTEM_CALL_COUNT_INFORMATION
{
	ULONG Length;
	ULONG NumberOfTables;
} SYSTEM_CALL_COUNT_INFORMATION, *PSYSTEM_CALL_COUNT_INFORMATION;

typedef struct _SYSTEM_DEVICE_INFORMATION
{
	ULONG NumberOfDisks;
	ULONG NumberOfFloppies;
	ULONG NumberOfCdRoms;
	ULONG NumberOfTapes;
	ULONG NumberOfSerialPorts;
	ULONG NumberOfParallelPorts;
} SYSTEM_DEVICE_INFORMATION, *PSYSTEM_DEVICE_INFORMATION;

typedef struct _SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION
{
	LARGE_INTEGER IdleTime;
	LARGE_INTEGER KernelTime;
	LARGE_INTEGER UserTime;
	LARGE_INTEGER DpcTime;
	LARGE_INTEGER InterruptTime;
	ULONG InterruptCount;
} SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION, *PSYSTEM_PROCESSOR_PERFORMANCE_INFORMATION;

typedef struct _SYSTEM_FLAGS_INFORMATION
{
	ULONG Flags; // NtGlobalFlag
} SYSTEM_FLAGS_INFORMATION, *PSYSTEM_FLAGS_INFORMATION;

typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO
{
	USHORT UniqueProcessId;
	USHORT CreatorBackTraceIndex;
	UCHAR ObjectTypeIndex;
	UCHAR HandleAttributes;
	USHORT HandleValue;
	PVOID Object;
	ULONG GrantedAccess;
} SYSTEM_HANDLE_TABLE_ENTRY_INFO, *PSYSTEM_HANDLE_TABLE_ENTRY_INFO;

typedef struct _SYSTEM_HANDLE_INFORMATION
{
	ULONG NumberOfHandles;
	SYSTEM_HANDLE_TABLE_ENTRY_INFO Handles[1];
} SYSTEM_HANDLE_INFORMATION, *PSYSTEM_HANDLE_INFORMATION;

typedef struct _SYSTEM_OBJECTTYPE_INFORMATION
{
	ULONG NextEntryOffset;
	ULONG NumberOfObjects;
	ULONG NumberOfHandles;
	ULONG TypeIndex;
	ULONG InvalidAttributes;
	GENERIC_MAPPING GenericMapping;
	ULONG ValidAccessMask;
	ULONG PoolType;
	BOOLEAN SecurityRequired;
	BOOLEAN WaitableObject;
	UNICODE_STRING TypeName;
} SYSTEM_OBJECTTYPE_INFORMATION, *PSYSTEM_OBJECTTYPE_INFORMATION;

typedef struct _SYSTEM_OBJECT_INFORMATION
{
	ULONG NextEntryOffset;
	PVOID Object;
	HANDLE CreatorUniqueProcess;
	USHORT CreatorBackTraceIndex;
	USHORT Flags;
	LONG PointerCount;
	LONG HandleCount;
	ULONG PagedPoolCharge;
	ULONG NonPagedPoolCharge;
	HANDLE ExclusiveProcessId;
	PVOID SecurityDescriptor;
	UNICODE_STRING NameInfo;
} SYSTEM_OBJECT_INFORMATION, *PSYSTEM_OBJECT_INFORMATION;

typedef struct _SYSTEM_PAGEFILE_INFORMATION
{
	ULONG NextEntryOffset;
	ULONG TotalSize;
	ULONG TotalInUse;
	ULONG PeakUsage;
	UNICODE_STRING PageFileName;
} SYSTEM_PAGEFILE_INFORMATION, *PSYSTEM_PAGEFILE_INFORMATION;

#define MM_WORKING_SET_MAX_HARD_ENABLE 0x1
#define MM_WORKING_SET_MAX_HARD_DISABLE 0x2
#define MM_WORKING_SET_MIN_HARD_ENABLE 0x4
#define MM_WORKING_SET_MIN_HARD_DISABLE 0x8

typedef struct _SYSTEM_FILECACHE_INFORMATION
{
	SIZE_T CurrentSize;
	SIZE_T PeakSize;
	ULONG PageFaultCount;
	SIZE_T MinimumWorkingSet;
	SIZE_T MaximumWorkingSet;
	SIZE_T CurrentSizeIncludingTransitionInPages;
	SIZE_T PeakSizeIncludingTransitionInPages;
	ULONG TransitionRePurposeCount;
	ULONG Flags;
} SYSTEM_FILECACHE_INFORMATION, *PSYSTEM_FILECACHE_INFORMATION;

// Can be used instead of SYSTEM_FILECACHE_INFORMATION
typedef struct _SYSTEM_BASIC_WORKING_SET_INFORMATION
{
	SIZE_T CurrentSize;
	SIZE_T PeakSize;
	ULONG PageFaultCount;
} SYSTEM_BASIC_WORKING_SET_INFORMATION, *PSYSTEM_BASIC_WORKING_SET_INFORMATION;

typedef struct _SYSTEM_POOLTAG
{
	union
	{
		UCHAR Tag[4];
		ULONG TagUlong;
	};
	ULONG PagedAllocs;
	ULONG PagedFrees;
	SIZE_T PagedUsed;
	ULONG NonPagedAllocs;
	ULONG NonPagedFrees;
	SIZE_T NonPagedUsed;
} SYSTEM_POOLTAG, *PSYSTEM_POOLTAG;

typedef struct _SYSTEM_POOLTAG_INFORMATION
{
	ULONG Count;
	SYSTEM_POOLTAG TagInfo[1];
} SYSTEM_POOLTAG_INFORMATION, *PSYSTEM_POOLTAG_INFORMATION;

typedef struct _SYSTEM_INTERRUPT_INFORMATION
{
	ULONG ContextSwitches;
	ULONG DpcCount;
	ULONG DpcRate;
	ULONG TimeIncrement;
	ULONG DpcBypassCount;
	ULONG ApcBypassCount;
} SYSTEM_INTERRUPT_INFORMATION, *PSYSTEM_INTERRUPT_INFORMATION;

typedef struct _SYSTEM_DPC_BEHAVIOR_INFORMATION
{
	ULONG Spare;
	ULONG DpcQueueDepth;
	ULONG MinimumDpcRate;
	ULONG AdjustDpcThreshold;
	ULONG IdealDpcRate;
} SYSTEM_DPC_BEHAVIOR_INFORMATION, *PSYSTEM_DPC_BEHAVIOR_INFORMATION;

typedef struct _SYSTEM_QUERY_TIME_ADJUST_INFORMATION
{
	ULONG TimeAdjustment;
	ULONG TimeIncrement;
	BOOLEAN Enable;
} SYSTEM_QUERY_TIME_ADJUST_INFORMATION, *PSYSTEM_QUERY_TIME_ADJUST_INFORMATION;

typedef struct _SYSTEM_SET_TIME_ADJUST_INFORMATION
{
	ULONG TimeAdjustment;
	BOOLEAN Enable;
} SYSTEM_SET_TIME_ADJUST_INFORMATION, *PSYSTEM_SET_TIME_ADJUST_INFORMATION;

typedef struct _SYSTEM_EXCEPTION_INFORMATION
{
	ULONG AlignmentFixupCount;
	ULONG ExceptionDispatchCount;
	ULONG FloatingEmulationCount;
	ULONG ByteWordEmulationCount;
} SYSTEM_EXCEPTION_INFORMATION, *PSYSTEM_EXCEPTION_INFORMATION;

typedef struct _SYSTEM_KERNEL_DEBUGGER_INFORMATION
{
	BOOLEAN KernelDebuggerEnabled;
	BOOLEAN KernelDebuggerNotPresent;
} SYSTEM_KERNEL_DEBUGGER_INFORMATION, *PSYSTEM_KERNEL_DEBUGGER_INFORMATION;

typedef struct _SYSTEM_CONTEXT_SWITCH_INFORMATION
{
	ULONG ContextSwitches;
	ULONG FindAny;
	ULONG FindLast;
	ULONG FindIdeal;
	ULONG IdleAny;
	ULONG IdleCurrent;
	ULONG IdleLast;
	ULONG IdleIdeal;
	ULONG PreemptAny;
	ULONG PreemptCurrent;
	ULONG PreemptLast;
	ULONG SwitchToIdle;
} SYSTEM_CONTEXT_SWITCH_INFORMATION, *PSYSTEM_CONTEXT_SWITCH_INFORMATION;

typedef struct _SYSTEM_REGISTRY_QUOTA_INFORMATION
{
	ULONG RegistryQuotaAllowed;
	ULONG RegistryQuotaUsed;
	SIZE_T PagedPoolSize;
} SYSTEM_REGISTRY_QUOTA_INFORMATION, *PSYSTEM_REGISTRY_QUOTA_INFORMATION;

typedef struct _SYSTEM_PROCESSOR_IDLE_INFORMATION
{
	ULONGLONG IdleTime;
	ULONGLONG C1Time;
	ULONGLONG C2Time;
	ULONGLONG C3Time;
	ULONG C1Transitions;
	ULONG C2Transitions;
	ULONG C3Transitions;
	ULONG Padding;
} SYSTEM_PROCESSOR_IDLE_INFORMATION, *PSYSTEM_PROCESSOR_IDLE_INFORMATION;

typedef struct _SYSTEM_LEGACY_DRIVER_INFORMATION
{
	ULONG VetoType;
	UNICODE_STRING VetoList;
} SYSTEM_LEGACY_DRIVER_INFORMATION, *PSYSTEM_LEGACY_DRIVER_INFORMATION;

typedef struct _SYSTEM_LOOKASIDE_INFORMATION
{
	USHORT CurrentDepth;
	USHORT MaximumDepth;
	ULONG TotalAllocates;
	ULONG AllocateMisses;
	ULONG TotalFrees;
	ULONG FreeMisses;
	ULONG Type;
	ULONG Tag;
	ULONG Size;
} SYSTEM_LOOKASIDE_INFORMATION, *PSYSTEM_LOOKASIDE_INFORMATION;

typedef struct _SYSTEM_VERIFIER_INFORMATION
{
	ULONG NextEntryOffset;
	ULONG Level;
	UNICODE_STRING DriverName;

	ULONG RaiseIrqls;
	ULONG AcquireSpinLocks;
	ULONG SynchronizeExecutions;
	ULONG AllocationsAttempted;

	ULONG AllocationsSucceeded;
	ULONG AllocationsSucceededSpecialPool;
	ULONG AllocationsWithNoTag;
	ULONG TrimRequests;

	ULONG Trims;
	ULONG AllocationsFailed;
	ULONG AllocationsFailedDeliberately;
	ULONG Loads;

	ULONG Unloads;
	ULONG UnTrackedPool;
	ULONG CurrentPagedPoolAllocations;
	ULONG CurrentNonPagedPoolAllocations;

	ULONG PeakPagedPoolAllocations;
	ULONG PeakNonPagedPoolAllocations;

	SIZE_T PagedPoolUsageInBytes;
	SIZE_T NonPagedPoolUsageInBytes;
	SIZE_T PeakPagedPoolUsageInBytes;
	SIZE_T PeakNonPagedPoolUsageInBytes;
} SYSTEM_VERIFIER_INFORMATION, *PSYSTEM_VERIFIER_INFORMATION;

typedef struct _SYSTEM_SESSION_PROCESS_INFORMATION
{
	ULONG SessionId;
	ULONG SizeOfBuf;
	PVOID Buffer;
} SYSTEM_SESSION_PROCESS_INFORMATION, *PSYSTEM_SESSION_PROCESS_INFORMATION;

typedef struct _SYSTEM_PROCESSOR_POWER_INFORMATION
{
	UCHAR CurrentFrequency;
	UCHAR ThermalLimitFrequency;
	UCHAR ConstantThrottleFrequency;
	UCHAR DegradedThrottleFrequency;
	UCHAR LastBusyFrequency;
	UCHAR LastC3Frequency;
	UCHAR LastAdjustedBusyFrequency;
	UCHAR ProcessorMinThrottle;
	UCHAR ProcessorMaxThrottle;
	ULONG NumberOfFrequencies;
	ULONG PromotionCount;
	ULONG DemotionCount;
	ULONG ErrorCount;
	ULONG RetryCount;
	ULONGLONG CurrentFrequencyTime;
	ULONGLONG CurrentProcessorTime;
	ULONGLONG CurrentProcessorIdleTime;
	ULONGLONG LastProcessorTime;
	ULONGLONG LastProcessorIdleTime;
} SYSTEM_PROCESSOR_POWER_INFORMATION, *PSYSTEM_PROCESSOR_POWER_INFORMATION;

typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX
{
	PVOID Object;
	ULONG_PTR UniqueProcessId;
	ULONG_PTR HandleValue;
	ULONG GrantedAccess;
	USHORT CreatorBackTraceIndex;
	USHORT ObjectTypeIndex;
	ULONG HandleAttributes;
	ULONG Reserved;
} SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX, *PSYSTEM_HANDLE_TABLE_ENTRY_INFO_EX;

typedef struct _SYSTEM_HANDLE_INFORMATION_EX
{
	ULONG_PTR NumberOfHandles;
	ULONG_PTR Reserved;
	SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX Handles[1];
} SYSTEM_HANDLE_INFORMATION_EX, *PSYSTEM_HANDLE_INFORMATION_EX;

typedef struct _SYSTEM_BIGPOOL_ENTRY
{
	union
	{
		PVOID VirtualAddress;
		ULONG_PTR NonPaged : 1;
	};
	SIZE_T SizeInBytes;
	union
	{
		UCHAR Tag[4];
		ULONG TagUlong;
	};
} SYSTEM_BIGPOOL_ENTRY, *PSYSTEM_BIGPOOL_ENTRY;

typedef struct _SYSTEM_BIGPOOL_INFORMATION
{
	ULONG Count;
	SYSTEM_BIGPOOL_ENTRY AllocatedInfo[1];
} SYSTEM_BIGPOOL_INFORMATION, *PSYSTEM_BIGPOOL_INFORMATION;

typedef struct _SYSTEM_POOL_ENTRY
{
	BOOLEAN Allocated;
	BOOLEAN Spare0;
	USHORT AllocatorBackTraceIndex;
	ULONG Size;
	union
	{
		UCHAR Tag[4];
		ULONG TagUlong;
		PVOID ProcessChargedQuota;
	};
} SYSTEM_POOL_ENTRY, *PSYSTEM_POOL_ENTRY;

typedef struct _SYSTEM_POOL_INFORMATION
{
	SIZE_T TotalSize;
	PVOID FirstEntry;
	USHORT EntryOverhead;
	BOOLEAN PoolTagPresent;
	BOOLEAN Spare0;
	ULONG NumberOfEntries;
	SYSTEM_POOL_ENTRY Entries[1];
} SYSTEM_POOL_INFORMATION, *PSYSTEM_POOL_INFORMATION;

typedef struct _SYSTEM_SESSION_POOLTAG_INFORMATION
{
	SIZE_T NextEntryOffset;
	ULONG SessionId;
	ULONG Count;
	SYSTEM_POOLTAG TagInfo[1];
} SYSTEM_SESSION_POOLTAG_INFORMATION, *PSYSTEM_SESSION_POOLTAG_INFORMATION;

typedef struct _SYSTEM_SESSION_MAPPED_VIEW_INFORMATION
{
	SIZE_T NextEntryOffset;
	ULONG SessionId;
	ULONG ViewFailures;
	SIZE_T NumberOfBytesAvailable;
	SIZE_T NumberOfBytesAvailableContiguous;
} SYSTEM_SESSION_MAPPED_VIEW_INFORMATION, *PSYSTEM_SESSION_MAPPED_VIEW_INFORMATION;

// private
typedef struct _SYSTEM_MEMORY_LIST_INFORMATION
{
	ULONG_PTR ZeroPageCount;
	ULONG_PTR FreePageCount;
	ULONG_PTR ModifiedPageCount;
	ULONG_PTR ModifiedNoWritePageCount;
	ULONG_PTR BadPageCount;
	ULONG_PTR PageCountByPriority[8];
	ULONG_PTR RepurposedPagesByPriority[8];
	ULONG_PTR ModifiedPageCountPageFile;
} SYSTEM_MEMORY_LIST_INFORMATION, *PSYSTEM_MEMORY_LIST_INFORMATION;

// private
typedef enum _SYSTEM_MEMORY_LIST_COMMAND
{
	MemoryCaptureAccessedBits,
	MemoryCaptureAndResetAccessedBits,
	MemoryEmptyWorkingSets,
	MemoryFlushModifiedList,
	MemoryPurgeStandbyList,
	MemoryPurgeLowPriorityStandbyList,
	MemoryCommandMax
} SYSTEM_MEMORY_LIST_COMMAND;

// private
typedef struct _SYSTEM_THREAD_CID_PRIORITY_INFORMATION
{
	CLIENT_ID ClientId;
	KPRIORITY Priority;
} SYSTEM_THREAD_CID_PRIORITY_INFORMATION, *PSYSTEM_THREAD_CID_PRIORITY_INFORMATION;

// private
typedef struct _SYSTEM_PROCESSOR_IDLE_CYCLE_TIME_INFORMATION
{
	ULONGLONG CycleTime;
} SYSTEM_PROCESSOR_IDLE_CYCLE_TIME_INFORMATION, *PSYSTEM_PROCESSOR_IDLE_CYCLE_TIME_INFORMATION;

// private
typedef struct _SYSTEM_REF_TRACE_INFORMATION
{
	BOOLEAN TraceEnable;
	BOOLEAN TracePermanent;
	UNICODE_STRING TraceProcessName;
	UNICODE_STRING TracePoolTags;
} SYSTEM_REF_TRACE_INFORMATION, *PSYSTEM_REF_TRACE_INFORMATION;

// private
typedef struct _SYSTEM_PROCESS_ID_INFORMATION
{
	HANDLE ProcessId;
	UNICODE_STRING ImageName;
} SYSTEM_PROCESS_ID_INFORMATION, *PSYSTEM_PROCESS_ID_INFORMATION;

// begin_private

// typedef enum _FIRMWARE_TYPE
// {
// 	FirmwareTypeUnknown,
// 	FirmwareTypeBios,
// 	FirmwareTypeEfi,
// 	FirmwareTypeMax
// } FIRMWARE_TYPE;

typedef struct _SYSTEM_BOOT_ENVIRONMENT_INFORMATION
{
	GUID BootIdentifier;
	FIRMWARE_TYPE FirmwareType;
} SYSTEM_BOOT_ENVIRONMENT_INFORMATION, PSYSTEM_BOOT_ENVIRONMENT_INFORMATION;

// end_private

// private
typedef struct _SYSTEM_IMAGE_FILE_EXECUTION_OPTIONS_INFORMATION
{
	ULONG FlagsToEnable;
	ULONG FlagsToDisable;
} SYSTEM_IMAGE_FILE_EXECUTION_OPTIONS_INFORMATION, *PSYSTEM_IMAGE_FILE_EXECUTION_OPTIONS_INFORMATION;

// private
typedef struct _SYSTEM_SYSTEM_PARTITION_INFORMATION
{
	UNICODE_STRING SystemPartition;
} SYSTEM_SYSTEM_PARTITION_INFORMATION, *PSYSTEM_SYSTEM_PARTITION_INFORMATION;

// private
typedef struct _SYSTEM_SYSTEM_DISK_INFORMATION
{
	UNICODE_STRING SystemDisk;
} SYSTEM_SYSTEM_DISK_INFORMATION, *PSYSTEM_SYSTEM_DISK_INFORMATION;

// private
typedef struct _SYSTEM_PROCESSOR_PERFORMANCE_HITCOUNT
{
	ULONG Hits;
	UCHAR PercentFrequency;
} SYSTEM_PROCESSOR_PERFORMANCE_HITCOUNT, *PSYSTEM_PROCESSOR_PERFORMANCE_HITCOUNT;

// private
typedef struct _SYSTEM_PROCESSOR_PERFORMANCE_STATE_DISTRIBUTION
{
	ULONG ProcessorNumber;
	ULONG StateCount;
	SYSTEM_PROCESSOR_PERFORMANCE_HITCOUNT States[1];
} SYSTEM_PROCESSOR_PERFORMANCE_STATE_DISTRIBUTION, *PSYSTEM_PROCESSOR_PERFORMANCE_STATE_DISTRIBUTION;

// private
typedef struct _SYSTEM_PROCESSOR_PERFORMANCE_DISTRIBUTION
{
	ULONG ProcessorCount;
	ULONG Offsets[1];
} SYSTEM_PROCESSOR_PERFORMANCE_DISTRIBUTION, *PSYSTEM_PROCESSOR_PERFORMANCE_DISTRIBUTION;

// private
typedef enum _SYSTEM_VA_TYPE
{
	SystemVaTypeAll,
	SystemVaTypeNonPagedPool,
	SystemVaTypePagedPool,
	SystemVaTypeSystemCache,
	SystemVaTypeSystemPtes,
	SystemVaTypeSessionSpace,
	SystemVaTypeMax
} SYSTEM_VA_TYPE, *PSYSTEM_VA_TYPE;

// private
typedef struct _SYSTEM_VA_LIST_INFORMATION
{
	SIZE_T VirtualSize;
	SIZE_T VirtualPeak;
	SIZE_T VirtualLimit;
	SIZE_T AllocationFailures;
} SYSTEM_VA_LIST_INFORMATION, *PSYSTEM_VA_LIST_INFORMATION;

// private
typedef struct _SYSTEM_REGISTRY_APPEND_STRING_PARAMETERS
{
	HANDLE KeyHandle;
	PUNICODE_STRING ValueNamePointer;
	PULONG RequiredLengthPointer;
	PUCHAR Buffer;
	ULONG BufferLength;
	ULONG Type;
	PUCHAR AppendBuffer;
	ULONG AppendBufferLength;
	BOOLEAN CreateIfDoesntExist;
	BOOLEAN TruncateExistingValue;
} SYSTEM_REGISTRY_APPEND_STRING_PARAMETERS, *PSYSTEM_REGISTRY_APPEND_STRING_PARAMETERS;

// msdn
typedef struct _SYSTEM_VHD_BOOT_INFORMATION
{
	BOOLEAN OsDiskIsVhd;
	ULONG OsVhdFilePathOffset;
	WCHAR OsVhdParentVolume[ANYSIZE_ARRAY];
} SYSTEM_VHD_BOOT_INFORMATION, *PSYSTEM_VHD_BOOT_INFORMATION;

// private
typedef struct _SYSTEM_LOW_PRIORITY_IO_INFORMATION
{
	ULONG LowPriReadOperations;
	ULONG LowPriWriteOperations;
	ULONG KernelBumpedToNormalOperations;
	ULONG LowPriPagingReadOperations;
	ULONG KernelPagingReadsBumpedToNormal;
	ULONG LowPriPagingWriteOperations;
	ULONG KernelPagingWritesBumpedToNormal;
	ULONG BoostedIrpCount;
	ULONG BoostedPagingIrpCount;
	ULONG BlanketBoostCount;
} SYSTEM_LOW_PRIORITY_IO_INFORMATION, *PSYSTEM_LOW_PRIORITY_IO_INFORMATION;

// symbols
typedef enum _TPM_BOOT_ENTROPY_RESULT_CODE
{
	TpmBootEntropyStructureUninitialized,
	TpmBootEntropyDisabledByPolicy,
	TpmBootEntropyNoTpmFound,
	TpmBootEntropyTpmError,
	TpmBootEntropySuccess
} TPM_BOOT_ENTROPY_RESULT_CODE;

// Contents of KeLoaderBlock->Extension->TpmBootEntropyResult (TPM_BOOT_ENTROPY_LDR_RESULT).
// EntropyData is truncated to 40 bytes.

// private
typedef struct _TPM_BOOT_ENTROPY_NT_RESULT
{
	ULONGLONG Policy;
	TPM_BOOT_ENTROPY_RESULT_CODE ResultCode;
	NTSTATUS ResultStatus;
	ULONGLONG Time;
	ULONG EntropyLength;
	UCHAR EntropyData[40];
} TPM_BOOT_ENTROPY_NT_RESULT, *PTPM_BOOT_ENTROPY_NT_RESULT;

// private
typedef struct _SYSTEM_VERIFIER_COUNTERS_INFORMATION
{
	SYSTEM_VERIFIER_INFORMATION Legacy;
	ULONG RaiseIrqls;
	ULONG AcquireSpinLocks;
	ULONG SynchronizeExecutions;
	ULONG AllocationsWithNoTag;
	ULONG AllocationsFailed;
	ULONG AllocationsFailedDeliberately;
	SIZE_T LockedBytes;
	SIZE_T PeakLockedBytes;
	SIZE_T MappedLockedBytes;
	SIZE_T PeakMappedLockedBytes;
	SIZE_T MappedIoSpaceBytes;
	SIZE_T PeakMappedIoSpaceBytes;
	SIZE_T PagesForMdlBytes;
	SIZE_T PeakPagesForMdlBytes;
	SIZE_T ContiguousMemoryBytes;
	SIZE_T PeakContiguousMemoryBytes;
} SYSTEM_VERIFIER_COUNTERS_INFORMATION, *PSYSTEM_VERIFIER_COUNTERS_INFORMATION;

// private
typedef struct _SYSTEM_ACPI_AUDIT_INFORMATION
{
	ULONG RsdpCount;
	ULONG SameRsdt : 1;
	ULONG SlicPresent : 1;
	ULONG SlicDifferent : 1;
} SYSTEM_ACPI_AUDIT_INFORMATION, *PSYSTEM_ACPI_AUDIT_INFORMATION;

// private
typedef struct _SYSTEM_BASIC_PERFORMANCE_INFORMATION
{
	SIZE_T AvailablePages;
	SIZE_T CommittedPages;
	SIZE_T CommitLimit;
	SIZE_T PeakCommitment;
} SYSTEM_BASIC_PERFORMANCE_INFORMATION, *PSYSTEM_BASIC_PERFORMANCE_INFORMATION;

// begin_msdn

typedef struct _QUERY_PERFORMANCE_COUNTER_FLAGS
{
	union
	{
		struct
		{
			ULONG KernelTransition : 1;
			ULONG Reserved : 31;
		};
		ULONG ul;
	};
} QUERY_PERFORMANCE_COUNTER_FLAGS;

typedef struct _SYSTEM_QUERY_PERFORMANCE_COUNTER_INFORMATION
{
	ULONG Version;
	QUERY_PERFORMANCE_COUNTER_FLAGS Flags;
	QUERY_PERFORMANCE_COUNTER_FLAGS ValidFlags;
} SYSTEM_QUERY_PERFORMANCE_COUNTER_INFORMATION, *PSYSTEM_QUERY_PERFORMANCE_COUNTER_INFORMATION;

// end_msdn

// NTSTATUS WINAPI NtQuerySystemInformation(
// 	_In_       SYSTEM_INFORMATION_CLASS SystemInformationClass,
// 	_Inout_    PVOID SystemInformation,
// 	_In_       ULONG SystemInformationLength,
// 	_Out_opt_  PULONG ReturnLength
// 	);
// rev
NTSYSCALLAPI
	NTSTATUS
	NTAPI
	NtQuerySystemInformationEx(
	__in SYSTEM_INFORMATION_CLASS SystemInformationClass,
	__in_bcount(QueryInformationLength) PVOID QueryInformation,
	__in ULONG QueryInformationLength,
	__out_bcount_opt(SystemInformationLength) PVOID SystemInformation,
	__in ULONG SystemInformationLength,
	__out_opt PULONG ReturnLength
	);

NTSYSCALLAPI
	NTSTATUS
	NTAPI
	NtSetSystemInformation(
	__in SYSTEM_INFORMATION_CLASS SystemInformationClass,
	__in_bcount_opt(SystemInformationLength) PVOID SystemInformation,
	__in ULONG SystemInformationLength
	);

// SysDbg APIs

SYSTEM_BASIC_INFORMATION g_systemBasicInformation;
unsigned long g_windowsVersion;
// ACCESS_MASK ProcessQueryAccess;
// ACCESS_MASK ProcessAllAccess;
// ACCESS_MASK ThreadQueryAccess;
// ACCESS_MASK ThreadSetAccess;
// ACCESS_MASK ThreadAllAccess;
//RTL_OSVERSIONINFOEXW g_versionInfo;
OSVERSIONINFO g_versionInfo;

typedef LONG (*tNtQuerySystemInformation)(SYSTEM_INFORMATION_CLASS SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength);
tNtQuerySystemInformation pNtQuerySystemInformation = NULL;
LONG NtQuerySystemInformation(SYSTEM_INFORMATION_CLASS SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength) {
		if (pNtQuerySystemInformation == NULL) {
			HMODULE mod = LoadLibrary(_T("Ntdll"));
			pNtQuerySystemInformation = reinterpret_cast<tNtQuerySystemInformation>(GetProcAddress(mod, "NtQuerySystemInformation"));
		}
		return pNtQuerySystemInformation(SystemInformationClass, SystemInformation, SystemInformationLength, ReturnLength);
}

namespace windows {
	void QuerySystemInformation() {
		NtQuerySystemInformation(SystemBasicInformation, &g_systemBasicInformation, sizeof(SYSTEM_BASIC_INFORMATION), NULL);
	}

	void GetVersion() {
		ULONG majorVersion;
		ULONG minorVersion;

		g_versionInfo.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOEXW);


		g_versionInfo.dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
		if (::GetVersionEx(&g_versionInfo) != 0) {
			g_windowsVersion = WINDOWS_NEW;
			return;
		}
		majorVersion = g_versionInfo.dwMajorVersion;
		minorVersion = g_versionInfo.dwMinorVersion;

		if (majorVersion == 5 && minorVersion < 1 || majorVersion < 5)
		{
			g_windowsVersion = WINDOWS_ANCIENT;
		}
		/* Windows XP */
		else if (majorVersion == 5 && minorVersion == 1)
		{
			g_windowsVersion = WINDOWS_XP;
		}
		/* Windows Server 2003 */
		else if (majorVersion == 5 && minorVersion == 2)
		{
			g_windowsVersion = WINDOWS_SERVER_2003;
		}
		/* Windows Vista, Windows Server 2008 */
		else if (majorVersion == 6 && minorVersion == 0)
		{
			g_windowsVersion = WINDOWS_VISTA;
		}
		/* Windows 7, Windows Server 2008 R2 */
		else if (majorVersion == 6 && minorVersion == 1)
		{
			g_windowsVersion = WINDOWS_7;
		}
		/* Windows 8 */
		else if (majorVersion == 6 && minorVersion == 2)
		{
			g_windowsVersion = WINDOWS_8;
		}
		else if (majorVersion == 6 && minorVersion > 2 || majorVersion > 6)
		{
			g_windowsVersion = WINDOWS_NEW;
		}

		// 	if (WINDOWS_HAS_LIMITED_ACCESS)
		// 	{
		// 		ProcessQueryAccess = PROCESS_QUERY_LIMITED_INFORMATION;
		// 		ProcessAllAccess = STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | 0x1fff;
		// 		ThreadQueryAccess = THREAD_QUERY_LIMITED_INFORMATION;
		// 		ThreadSetAccess = THREAD_SET_LIMITED_INFORMATION;
		// 		ThreadAllAccess = STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | 0xfff;
		// 	}
		// 	else
		// 	{
		// 		ProcessQueryAccess = PROCESS_QUERY_INFORMATION;
		// 		ProcessAllAccess = STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | 0xfff;
		// 		ThreadQueryAccess = THREAD_QUERY_INFORMATION;
		// 		ThreadSetAccess = THREAD_SET_INFORMATION;
		// 		ThreadAllAccess = STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | 0x3ff;
		// 	}
	}

	bool g_hasVersion = false;
	bool g_hasBasicInfo = false;


	boost::scoped_array<unsigned long long> g_CPUIdleTimeOld;
	boost::scoped_array<unsigned long long> g_CPUTotalTimeOld;
	boost::scoped_array<unsigned long long> g_CPUKernelTimeOld;

	void init_old_buffer(boost::scoped_array<unsigned long long> &array, const std::size_t size) {
		if (!array) {
			array.reset(new unsigned long long[size]);
			for (int i=0;i<size;i++) {
				array[i] = 0;
			}
		}
	}
	unsigned long system_info::get_version() {
		if (!g_hasVersion) {
			GetVersion();
			g_hasVersion = true;
		}
		return g_windowsVersion;
	}

	long system_info::get_numberOfProcessorscores() {
		if (!g_hasBasicInfo) {
			QuerySystemInformation();
			g_hasBasicInfo = true;
		}
		return g_systemBasicInformation.NumberOfProcessors;
	}
	system_info::cpu_load system_info::get_cpu_load() {
		int cores = get_numberOfProcessorscores();
		init_old_buffer(g_CPUIdleTimeOld, cores);
		init_old_buffer(g_CPUTotalTimeOld, cores);
		init_old_buffer(g_CPUKernelTimeOld, cores);

		boost::scoped_array<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION> buffer(new SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION[cores]);
		if (NtQuerySystemInformation(SystemProcessorPerformanceInformation, &buffer[0], sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * cores, NULL) != 0) {
			throw nscp_exception("Whoops");
		}

		cpu_load result;
		result.cores = cores;
		result.core.resize(cores);
		result.total.idle = result.total.kernel = result.total.total = 0.0;

		for (int i=0; i < cores; i++) {
			unsigned long long CPUIdleTime = buffer[i].IdleTime.QuadPart;
			unsigned long long CPUKernelTime = buffer[i].KernelTime.QuadPart - buffer[i].IdleTime.QuadPart;
			unsigned long long CPUTotalTime = buffer[i].KernelTime.QuadPart + buffer[i].UserTime.QuadPart;

			unsigned long long CPUIdleTimeDiff = CPUIdleTime - g_CPUIdleTimeOld[i];
			unsigned long long CPUKernelTimeDiff = CPUKernelTime - g_CPUKernelTimeOld[i];
			unsigned long long CPUTotalTimeDiff = CPUTotalTime - g_CPUTotalTimeOld[i];

			if (CPUTotalTimeDiff != 0) {
				result.core[i].core = i;
				result.core[i].idle = static_cast<double>(((CPUIdleTimeDiff * 100) / CPUTotalTimeDiff));
				result.core[i].kernel = static_cast<double>(((CPUKernelTimeDiff * 100) / CPUTotalTimeDiff));
				result.core[i].total = 100.0 - result.core[i].idle;
				result.total.idle += result.core[i].idle;
				result.total.kernel += result.core[i].kernel;
				result.total.total += result.core[i].total;
			} else {
				result.core[i].idle = 0;
				result.core[i].kernel = 0;
				result.core[i].total = 0;
			}
			g_CPUTotalTimeOld[i] = CPUTotalTime;
			g_CPUIdleTimeOld[i] = CPUIdleTime;
			g_CPUKernelTimeOld[i] = CPUKernelTime;
		}
		result.total.idle /= result.cores;
		result.total.kernel /= result.cores;
		result.total.total /= result.cores;
		return result;
	}


	class CheckMemory {
	public:
		CheckMemory() : hKernel32(NULL), FEGlobalMemoryStatusEx(NULL), FEGlobalMemoryStatus(NULL)
		{
			hKernel32 = ::LoadLibrary(_TEXT("Kernel32"));
			if (hKernel32)  
			{
				FEGlobalMemoryStatusEx = (PFGlobalMemoryStatusEx)::GetProcAddress(hKernel32, "GlobalMemoryStatusEx");
				FEGlobalMemoryStatus = (PFGlobalMemoryStatus)::GetProcAddress(hKernel32, "GlobalMemoryStatus");
			}
		}
		virtual ~CheckMemory() {
			if (hKernel32) FreeLibrary(hKernel32);
		}

		system_info::memory_usage getMemoryStatus() {
			system_info::memory_usage ret;
			if (FEGlobalMemoryStatusEx != NULL) {
				MEMORYSTATUSEX buffer;
				buffer.dwLength = sizeof(buffer);
				if (!FEGlobalMemoryStatusEx(&buffer))
					throw nscp_exception("GlobalMemoryStatusEx failed: " + error::lookup::last_error());
				ret.physical.total = buffer.ullTotalPhys;
				ret.physical.avail = buffer.ullAvailPhys;
				ret.virtual_memory.total = buffer.ullTotalVirtual;
				ret.virtual_memory.avail = buffer.ullAvailVirtual;
				ret.pagefile.total = buffer.ullTotalPageFile;
				ret.pagefile.avail = buffer.ullAvailPageFile;
				return ret;
			} else if (FEGlobalMemoryStatus != NULL) {
				MEMORYSTATUS buffer;
				buffer.dwLength = sizeof(buffer);
				if (!FEGlobalMemoryStatus(&buffer))
					throw nscp_exception("GlobalMemoryStatus failed: " + error::lookup::last_error());
				ret.physical.total = buffer.dwTotalPhys;
				ret.physical.avail = buffer.dwAvailPhys;
				ret.virtual_memory.total = buffer.dwTotalVirtual;
				ret.virtual_memory.avail = buffer.dwAvailVirtual;
				ret.pagefile.total = buffer.dwTotalPageFile;
				ret.pagefile.avail = buffer.dwAvailPageFile;
				return ret;
			} else {
				throw nscp_exception("Failed to check memory: No method found");
			}
		}
	private:
		typedef BOOL (WINAPI *PFGlobalMemoryStatusEx)(LPMEMORYSTATUSEX lpBuffer);
		typedef BOOL (WINAPI *PFGlobalMemoryStatus)(LPMEMORYSTATUS lpBuffer);
		HMODULE hKernel32;
		PFGlobalMemoryStatusEx	FEGlobalMemoryStatusEx;
		PFGlobalMemoryStatus	FEGlobalMemoryStatus;
	};


	CheckMemory g_memory_checker;
	system_info::memory_usage system_info::get_memory() {
		return g_memory_checker.getMemoryStatus();
	}


	//////////////////////////////////////////////////////////////////////////
	namespace winapi {

		typedef BOOL (*tEnumServicesStatusEx)(SC_HANDLE hSCManager, SC_ENUM_TYPE InfoLevel, DWORD dwServiceType, DWORD dwServiceState, LPBYTE lpServices, DWORD cbBufSize, LPDWORD pcbBytesNeeded, LPDWORD lpServicesReturned, LPDWORD lpResumeHandle, LPCTSTR pszGroupName);
		typedef BOOL (*tQueryServiceConfig2)(SC_HANDLE hService, DWORD dwInfoLevel, LPBYTE lpBuffer, DWORD cbBufSize, LPDWORD pcbBytesNeeded);
		typedef BOOL (*tQueryServiceStatusEx)(SC_HANDLE hService, SC_STATUS_TYPE InfoLevel, LPBYTE lpBuffer, DWORD cbBufSize, LPDWORD pcbBytesNeeded);
		typedef INT (*tVDMEnumTaskWOWEx)(DWORD dwProcessId, tTASKENUMPROCEX fp, LPARAM lparam); 
		typedef LONG (*tNtQueryInformationProcess)(HANDLE ProcessHandle, DWORD ProcessInformationClass, PVOID ProcessInformation, DWORD ProcessInformationLength, PDWORD ReturnLength);
		typedef BOOL (*tIsWow64Process) (HANDLE ProcessHandle, PBOOL);
		typedef DWORD (*tGetProcessImageFileName)(HANDLE hProcess, LPWSTR lpImageFileName, DWORD nSize);


		tEnumServicesStatusEx pEnumServicesStatusEx = NULL;
		tQueryServiceConfig2 pQueryServiceConfig2 = NULL;
		tQueryServiceStatusEx pQueryServiceStatusEx = NULL;
		tVDMEnumTaskWOWEx pVDMEnumTaskWOWEx = NULL;
		tNtQueryInformationProcess pNtQueryInformationProcess = NULL;
		tIsWow64Process pIsWow64Process = NULL;
		tGetProcessImageFileName pGetProcessImageFileName = NULL;

		BOOL EnumServicesStatusEx(SC_HANDLE hSCManager, SC_ENUM_TYPE InfoLevel, DWORD dwServiceType, DWORD dwServiceState, LPBYTE lpServices, DWORD cbBufSize, LPDWORD pcbBytesNeeded, LPDWORD lpServicesReturned, LPDWORD lpResumeHandle, LPCTSTR pszGroupName) {
			if (pEnumServicesStatusEx == NULL) {
				HMODULE hMod = ::LoadLibrary(_TEXT("Advapi32.dll"));
				if (hMod == NULL)
					throw nscp_exception("Failed to load: Advapi32: " + error::lookup::last_error());
				pEnumServicesStatusEx = reinterpret_cast<tEnumServicesStatusEx>(GetProcAddress(hMod, "EnumServicesStatusExW"));
				if (pEnumServicesStatusEx == NULL)
					throw nscp_exception("Failed to load: EnumServicesStatusEx: " + error::lookup::last_error());
			}
			return pEnumServicesStatusEx(hSCManager, InfoLevel, dwServiceType, dwServiceState, lpServices, cbBufSize, pcbBytesNeeded, lpServicesReturned, lpResumeHandle, pszGroupName);
		}
		BOOL QueryServiceConfig2(SC_HANDLE hService, DWORD dwInfoLevel, LPBYTE lpBuffer, DWORD cbBufSize, LPDWORD pcbBytesNeeded) {
			if (pEnumServicesStatusEx == NULL) {
				HMODULE hMod = ::LoadLibrary(_TEXT("Advapi32.dll"));
				if (hMod == NULL)
					throw nscp_exception("Failed to load: Advapi32: " + error::lookup::last_error());
				pQueryServiceConfig2 = reinterpret_cast<tQueryServiceConfig2>(GetProcAddress(hMod, "QueryServiceConfig2W"));
				if (pQueryServiceConfig2 == NULL)
					throw nscp_exception("Failed to load: QueryServiceConfig2: " + error::lookup::last_error());
			}
			return pQueryServiceConfig2(hService, dwInfoLevel, lpBuffer, cbBufSize, pcbBytesNeeded);
		}
		BOOL QueryServiceStatusEx(SC_HANDLE hService, SC_STATUS_TYPE InfoLevel, LPBYTE lpBuffer, DWORD cbBufSize, LPDWORD pcbBytesNeeded) {
			if (pQueryServiceStatusEx == NULL) {
				HMODULE hMod = ::LoadLibrary(_TEXT("Advapi32.dll"));
				if (hMod == NULL)
					throw nscp_exception("Failed to load: Advapi32: " + error::lookup::last_error());
				pQueryServiceStatusEx = reinterpret_cast<tQueryServiceStatusEx>(GetProcAddress(hMod, "QueryServiceStatusEx"));
				if (pQueryServiceStatusEx == NULL)
					throw nscp_exception("Failed to load: QueryServiceStatusEx: " + error::lookup::last_error());
			}
			return pQueryServiceConfig2(hService, InfoLevel, lpBuffer, cbBufSize, pcbBytesNeeded);
		}


		bool IsWow64(HANDLE hProcess, bool def) {
			if (pIsWow64Process == NULL)
				pIsWow64Process = reinterpret_cast<tIsWow64Process>(GetProcAddress(GetModuleHandle(TEXT("kernel32")),"IsWow64Process"));
			if (pIsWow64Process == NULL)
				return def;
			BOOL bIsWow64 = FALSE;
			if (!pIsWow64Process(hProcess,&bIsWow64))
				return def;
			return bIsWow64?true:false;
		}

		DWORD GetProcessImageFileName(HANDLE hProcess, LPWSTR lpImageFileName, DWORD nSize) {
			if (pGetProcessImageFileName == NULL)
				pGetProcessImageFileName = reinterpret_cast<tGetProcessImageFileName>(GetProcAddress(GetModuleHandleA("PSAPI"), "GetProcessImageFileNameW"));
			if (pGetProcessImageFileName == NULL)
				throw nscp_exception("Failed to load GetProcessImageFileName: " + error::lookup::last_error());
			return pGetProcessImageFileName(hProcess, lpImageFileName, nSize);
		}
		LONG NtQueryInformationProcess(HANDLE ProcessHandle, DWORD ProcessInformationClass, PVOID ProcessInformation, DWORD ProcessInformationLength, PDWORD ReturnLength) {
			if (pNtQueryInformationProcess == NULL)
				pNtQueryInformationProcess = reinterpret_cast<tNtQueryInformationProcess>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationProcess"));
			if (pNtQueryInformationProcess == NULL)
				throw nscp_exception("Failed to load NtQueryInformationProcess");
			return pNtQueryInformationProcess(ProcessHandle, ProcessInformationClass, ProcessInformation, ProcessInformationLength, ReturnLength);
		}
		INT VDMEnumTaskWOWEx(DWORD dwProcessId, tTASKENUMPROCEX fp, LPARAM lparam) {
			if (pVDMEnumTaskWOWEx == NULL)
				pVDMEnumTaskWOWEx = reinterpret_cast<tVDMEnumTaskWOWEx>(GetProcAddress(GetModuleHandleA("VDMDBG"), "VDMEnumTaskWOWEx"));
			if (pVDMEnumTaskWOWEx == NULL)
				throw nscp_exception("Failed to load NtQueryInformationProcess");
			return pVDMEnumTaskWOWEx(dwProcessId, fp, lparam);
		}

	}
}



