#include <config.h>
#include <tchar.h>

wchar_t *kCrashReportProductName = SZAPPNAME;
wchar_t *kCrashReportProductVersion = STRPRODUCTVER;
wchar_t *kCrashReportService = SZSERVICENAME;

bool kCrashReportAlwaysUpload = false;
wchar_t *kCrashReportThrottlingRegKey = L"Software\\Google\\Breakpad\\Throttling";
int kCrashReportAttempts         = 3;
int kCrashReportResendPeriodMs   = (1 * 60 * 60 * 1000);
int kCrashReportsIntervalSeconds = (24 * 60  * 60);
