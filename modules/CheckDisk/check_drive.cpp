#include "check_drive.hpp"

#ifdef WIN32
#include <Windows.h>
#endif

#include <boost/bind.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <char_buffer.hpp>
#include <error.hpp>
#include <format.hpp>

#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

#include <parsers/filter/modern_filter.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/where/helpers.hpp>

namespace npo = nscapi::program_options;
namespace po = boost::program_options;


const int drive_type_total = 0x77;



std::string type_to_string(const int type) {
	if (type == DRIVE_FIXED)
		return "fixed";
	if (type == DRIVE_CDROM)
		return "cdrom";
	if (type == DRIVE_REMOVABLE)
		return "removable";
	if (type == DRIVE_REMOTE)
		return "remote";
	if (type == DRIVE_RAMDISK)
		return "ramdisk";
	if (type == DRIVE_UNKNOWN)
		return "unknown";
	if (type == DRIVE_NO_ROOT_DIR)
		return "no_root_dir";
	if (type == drive_type_total)
		return "total";
	return "unknown";
}


struct drive_container {
	std::string id;
	std::string letter;
	std::string name;
	drive_container() {}
	drive_container(std::string id, std::string letter, std::string name) : id(id), letter(letter), name(name) {}
};


struct filter_obj {
	drive_container drive;
	UINT drive_type;
	long long user_free;
	long long total_free;
	long long drive_size;
	bool has_size;
	bool has_type;
	bool unreadable;

	filter_obj() : drive_type(0), user_free(0), total_free(0), drive_size(0), has_size(false), has_type(false), unreadable(false) {}
	filter_obj(const drive_container drive) 
		: drive(drive)
		, drive_type(0)
		, user_free(0)
		, total_free(0)
		, drive_size(0)
		, has_size(false)
		, has_type(false)
		, unreadable(true)
	{};

	std::string get_drive(parsers::where::evaluation_context) const { return drive.letter; }
	std::string get_name(parsers::where::evaluation_context) const { return drive.name; }
	std::string get_id(parsers::where::evaluation_context) const { return drive.id; }
	std::string get_drive_or_id(parsers::where::evaluation_context) const { return drive.letter.empty()?drive.id:drive.letter; }
	std::string get_drive_or_name(parsers::where::evaluation_context) const { return drive.letter.empty()?drive.name:drive.letter; }

	long long get_user_free(parsers::where::evaluation_context context) { get_size(context); return user_free; }
	long long get_total_free(parsers::where::evaluation_context context) { get_size(context); return total_free; }
	long long get_drive_size(parsers::where::evaluation_context context) { get_size(context); return drive_size; }
	long long get_total_used(parsers::where::evaluation_context context) { get_size(context); return drive_size-total_free; }
	long long get_user_used(parsers::where::evaluation_context context) { get_size(context); return drive_size-user_free; }

	long long get_user_free_pct(parsers::where::evaluation_context context) { get_size(context); return drive_size==0?0:(user_free*100/drive_size); }
	long long get_total_free_pct(parsers::where::evaluation_context context) { get_size(context); return drive_size==0?0:(total_free*100/drive_size); }
	long long get_user_used_pct(parsers::where::evaluation_context context) { return 100-get_user_free_pct(context); }
	long long get_total_used_pct(parsers::where::evaluation_context context) { return 100-get_total_free_pct(context); }

	std::string get_user_free_human(parsers::where::evaluation_context context) {
		return format::format_byte_units(get_user_free(context));
	}
	std::string get_total_free_human(parsers::where::evaluation_context context) {
		return format::format_byte_units(get_total_free(context));
	}
	std::string get_drive_size_human(parsers::where::evaluation_context context) {
		return format::format_byte_units(get_drive_size(context));
	}
	std::string get_total_used_human(parsers::where::evaluation_context context) {
		return format::format_byte_units(get_total_used(context));
	}
	std::string get_user_used_human(parsers::where::evaluation_context context) {
		return format::format_byte_units(get_user_used(context));
	}

	std::string get_type_as_string(parsers::where::evaluation_context context) {
		return type_to_string(get_type(context));
	}

	std::wstring get_volume_or_letter_w() {
		if (!drive.id.empty())
			return utf8::cvt<std::wstring>(drive.id);
		return utf8::cvt<std::wstring>(drive.letter);

	}
	long long get_type(parsers::where::evaluation_context context) {
		if (has_type)
			return drive_type;
		drive_type = GetDriveType(get_volume_or_letter_w().c_str());
		has_type = true;
		return drive_type;
	}

	void get_size(parsers::where::evaluation_context context) {
		if (has_size)
			return;

		ULARGE_INTEGER freeBytesAvailableToCaller;
		ULARGE_INTEGER totalNumberOfBytes;
		ULARGE_INTEGER totalNumberOfFreeBytes;
		std::string error;
		std::wstring drv = get_volume_or_letter_w();
		if (drv.size() == 1)
			drv = drv + L":\\";
		if (!GetDiskFreeSpaceEx(drv.c_str(), &freeBytesAvailableToCaller, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
			DWORD err = GetLastError();
			if (err == ERROR_NOT_READY) {
				has_size = true;
				user_free = 0;
				total_free = 0;
				drive_size = 0;
				return;
			}
			context->error("Failed to get size for: " + drive.name + error::lookup::last_error(err));
			unreadable = err == ERROR_ACCESS_DENIED;
			has_size = true;
			return;
		}
		has_size = true;
		user_free = freeBytesAvailableToCaller.QuadPart;
		total_free = totalNumberOfFreeBytes.QuadPart;
		drive_size = totalNumberOfBytes.QuadPart;
	}

	void append(boost::shared_ptr<filter_obj> other) {
		user_free += other->user_free;
		total_free += other->total_free;
		drive_size += other->drive_size;

	}
	void make_total() {
		has_size = true;
		has_type = true;
		total_free = 0;
		user_free = 0;
		drive_size = 0;
		drive_type = drive_type_total;
	}
};

parsers::where::node_type calculate_total_used(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
	boost::tuple<long long, std::string> value = parsers::where::helpers::read_arguments(context, subject, "%");
	long long number = value.get<0>();
	std::string unit = value.get<1>();

	if (unit == "%") {
		number = (object->get_drive_size(context)*(number))/100;
	} else {
		number = format::decode_byte_units(number, unit);
	}
	return parsers::where::factory::create_int(number);
}

parsers::where::node_type calculate_user_used(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
	boost::tuple<long long, std::string> value = parsers::where::helpers::read_arguments(context, subject, "%");
	long long number = value.get<0>();
	std::string unit = value.get<1>();

	if (unit == "%") {
		number = (object->get_user_free(context)*number)/100;
	} else {
		number = format::decode_byte_units(number, unit);
	}
	return parsers::where::factory::create_int(number);
}
int do_convert_type(const std::string &keyword) {
	if (keyword == "fixed")
		return DRIVE_FIXED;
	if (keyword == "cdrom")
		return DRIVE_CDROM;
	if (keyword == "removable")
		return DRIVE_REMOVABLE;
	if (keyword == "remote")
		return DRIVE_REMOTE;
	if (keyword == "ramdisk")
		return DRIVE_RAMDISK;
	if (keyword == "unknown")
		return DRIVE_UNKNOWN;
	if (keyword == "no_root_dir")
		return DRIVE_NO_ROOT_DIR;
	if (keyword == "total")
		return drive_type_total;
	return -1;
}
parsers::where::node_type convert_type(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
	std::string keyword = subject->get_string_value(context);
	boost::to_lower(keyword);
	int type = do_convert_type(keyword);
	if (type == -1) {
		context->error("Failed to convert type: " + keyword);
		return parsers::where::factory::create_false();
	}
	return parsers::where::factory::create_int(type);
}

long long get_zero() {
	return 0;
}
typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {

	static const parsers::where::value_type type_custom_total_used = parsers::where::type_custom_int_1;
	static const parsers::where::value_type type_custom_total_free = parsers::where::type_custom_int_2;
	static const parsers::where::value_type type_custom_user_used = parsers::where::type_custom_int_3;
	static const parsers::where::value_type type_custom_user_free = parsers::where::type_custom_int_4;
	static const parsers::where::value_type type_custom_type = parsers::where::type_custom_int_9;

	filter_obj_handler() {
		registry_.add_string()
			("name", &filter_obj::get_name, "Descriptive name of drive")
			("id", &filter_obj::get_id, "Drive or id of drive")
			("drive", &filter_obj::get_drive, "Technical name of drive")
			("drive_or_id", &filter_obj::get_drive_or_id, "Drive letter if present if not use id")
			("drive_or_name", &filter_obj::get_drive_or_name, "Drive letter if present if not use name")
			;
		registry_.add_int()
			("free", type_custom_total_free, &filter_obj::get_total_free, "Shorthand for total_free (Number of free bytes)")
				.add_scaled_byte(boost::bind(&get_zero), &filter_obj::get_drive_size, "", " free")
				.add_percentage(&filter_obj::get_drive_size, "", " free %")
 			("total_free", type_custom_total_free, &filter_obj::get_total_free, "Number of free bytes")
				.add_scaled_byte(boost::bind(&get_zero), &filter_obj::get_drive_size, "", " free")
				.add_percentage(&filter_obj::get_drive_size, "", " free %")
 			("user_free", type_custom_user_free, &filter_obj::get_user_free, "Free space available to user (which runs NSClient++)")
				.add_scaled_byte(boost::bind(&get_zero), &filter_obj::get_drive_size, "", " user free")
				.add_percentage(&filter_obj::get_drive_size, "", " user free %")
 			("size", parsers::where::type_size, &filter_obj::get_drive_size, "Total size of drive")
 			("total_used", type_custom_total_used, &filter_obj::get_total_used, "Number of used bytes")
				.add_scaled_byte(boost::bind(&get_zero), &filter_obj::get_drive_size, "", " used")
				.add_percentage(&filter_obj::get_drive_size, "", " used %")
 			("used", type_custom_total_used, &filter_obj::get_total_used, "Number of used bytes")
				.add_scaled_byte(boost::bind(&get_zero), &filter_obj::get_drive_size, "", " used")
				.add_percentage(&filter_obj::get_drive_size, "", " used %")
			("user_used", type_custom_user_used, &filter_obj::get_user_used, "Number of used bytes (related to user)")
				.add_scaled_byte(boost::bind(&get_zero), &filter_obj::get_drive_size, "", " user used")
				.add_percentage(&filter_obj::get_drive_size, "", " user used %")
			("type", type_custom_type, &filter_obj::get_type, "Type of drive")
			("free_pct", &filter_obj::get_total_free_pct, "Shorthand for total_free_pct (% free space)")
			("total_free_pct", &filter_obj::get_total_free_pct, "% free space")
			("user_free_pct", type_custom_user_free, &filter_obj::get_user_free_pct, "% free space available to user")
			("used_pct", &filter_obj::get_total_used_pct, "Shorthand for total_used_pct (% used space)")
			("total_used_pct", &filter_obj::get_total_used_pct, "% used space")
			("user_used_pct", type_custom_user_used, &filter_obj::get_user_used_pct, "% used space available to user")
			;

		registry_.add_human_string()
			("free", &filter_obj::get_total_free_human, "")
			("total_free", &filter_obj::get_total_free_human, "")
			("user_free", &filter_obj::get_user_free_human, "")
			("size", &filter_obj::get_drive_size_human, "")
			("total_used", &filter_obj::get_total_used_human, "")
			("used", &filter_obj::get_total_used_human, "")
			("user_used", &filter_obj::get_user_used_human, "")
			("type", &filter_obj::get_type_as_string, "")
			;


		registry_.add_converter()
			(type_custom_total_free, &calculate_total_used)
			(type_custom_total_used, &calculate_total_used)
			(type_custom_user_free, &calculate_user_used)
			(type_custom_user_used, &calculate_user_used)
			(type_custom_type, &convert_type)
			;

	}
};

typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;



boost::shared_ptr<filter_obj> get_details(const drive_container &drive, bool ignore_errors) {
	return boost::make_shared<filter_obj>(drive);
}

class volume_helper {
	typedef HANDLE (WINAPI *typeFindFirstVolumeW)( __out_ecount(cchBufferLength) LPWSTR lpszVolumeName, __in DWORD cchBufferLength);
	typedef BOOL (WINAPI *typeFindNextVolumeW)( __inout HANDLE hFindVolume, __out_ecount(cchBufferLength) LPWSTR lpszVolumeName, __in DWORD cchBufferLength);
	typedef HANDLE (WINAPI *typeFindFirstVolumeMountPointW)( __in LPCWSTR lpszRootPathName, __out_ecount(cchBufferLength) LPWSTR lpszVolumeMountPoint, __in DWORD cchBufferLength );
	typedef BOOL (WINAPI *typeFindNextVolumeMountPointW)( __inout HANDLE hFindVolume, __out_ecount(cchBufferLength) LPWSTR lpszVolumeName, __in DWORD cchBufferLength);
	typedef BOOL (WINAPI *typeGetVolumeNameForVolumeMountPointW)( __in LPCWSTR lpszVolumeMountPoint, __out_ecount(cchBufferLength) LPWSTR lpszVolumeName, __in DWORD cchBufferLength );
	typedef BOOL (WINAPI *typeGetVolumeInformationByHandleW)(_In_ HANDLE hFile, _Out_opt_ LPWSTR lpVolumeNameBuffer, _In_ DWORD nVolumeNameSize, _Out_opt_ LPDWORD lpVolumeSerialNumber, 
			_Out_opt_ LPDWORD  lpMaximumComponentLength, _Out_opt_ LPDWORD lpFileSystemFlags, _Out_opt_ LPWSTR lpFileSystemNameBuffer, _In_ DWORD nFileSystemNameSize);
	typedef BOOL (WINAPI *typeGetVolumePathNamesForVolumeNameW)(_In_ LPCTSTR lpszVolumeName, _Out_ LPTSTR lpszVolumePathNames, _In_ DWORD cchBufferLength, _Out_ PDWORD lpcchReturnLength);


	typeFindFirstVolumeW ptrFindFirstVolumeW;
	typeFindNextVolumeW ptrFindNextVolumeW;
	typeFindFirstVolumeMountPointW ptrFindFirstVolumeMountPointW;
	typeFindNextVolumeMountPointW ptrFindNextVolumeMountPointW;
	typeGetVolumeNameForVolumeMountPointW ptrGetVolumeNameForVolumeMountPointW;
	typeGetVolumeInformationByHandleW ptrGetVolumeInformationByHandleW;
	typeGetVolumePathNamesForVolumeNameW ptrGetVolumePathNamesForVolumeNameW;
	HMODULE hLib;

public:
	typedef std::map<std::string,std::string> map_type;

public:
	volume_helper() 
		: ptrFindFirstVolumeW(NULL)
		, ptrFindNextVolumeW(NULL)
		, ptrFindFirstVolumeMountPointW(NULL)
		, ptrFindNextVolumeMountPointW(NULL)
		, ptrGetVolumeNameForVolumeMountPointW(NULL)
		, ptrGetVolumeInformationByHandleW(NULL)
		, ptrGetVolumePathNamesForVolumeNameW(NULL)
	{
		hLib = ::LoadLibrary(_TEXT("KERNEL32"));
		if (hLib) {
			ptrFindFirstVolumeW = (typeFindFirstVolumeW)::GetProcAddress(hLib, "FindFirstVolumeW");
			ptrFindNextVolumeW = (typeFindNextVolumeW)::GetProcAddress(hLib, "FindNextVolumeW");
			ptrFindFirstVolumeMountPointW = (typeFindFirstVolumeMountPointW)::GetProcAddress(hLib, "FindFirstVolumeMountPointW");
			ptrFindNextVolumeMountPointW = (typeFindNextVolumeMountPointW)::GetProcAddress(hLib, "FindNextVolumeMountPointW");
			ptrGetVolumeNameForVolumeMountPointW = (typeGetVolumeNameForVolumeMountPointW)::GetProcAddress(hLib, "GetVolumeNameForVolumeMountPointW");
			ptrGetVolumeInformationByHandleW = (typeGetVolumeInformationByHandleW)::GetProcAddress(hLib, "GetVolumeInformationByHandleW");
			ptrGetVolumePathNamesForVolumeNameW = (typeGetVolumePathNamesForVolumeNameW)::GetProcAddress(hLib, "GetVolumePathNamesForVolumeNameW");
		}
	}

	~volume_helper() {
	}

	HANDLE FindFirstVolume(std::wstring &volume) {
		if (ptrFindFirstVolumeW == NULL)
			return INVALID_HANDLE_VALUE;
		hlp::tchar_buffer buffer(1024);
		HANDLE h = ptrFindFirstVolumeW(buffer.get(), static_cast<DWORD>(buffer.size()));
		if (h != INVALID_HANDLE_VALUE)
			volume = buffer.get();
		return h;
	}
	BOOL FindNextVolume(HANDLE hVolume, std::wstring &volume) {
		if (ptrFindFirstVolumeW == NULL || hVolume == INVALID_HANDLE_VALUE)
			return FALSE;
		hlp::tchar_buffer buffer(1024);
		BOOL r = ptrFindNextVolumeW(hVolume, buffer.get(), static_cast<DWORD>(buffer.size()));
		if (r)
			volume = buffer.get();
		return r;
	}

	HANDLE FindFirstVolumeMountPoint(std::wstring &root, std::wstring &volume) {
		if (ptrFindFirstVolumeMountPointW == NULL)
			return INVALID_HANDLE_VALUE;
		hlp::tchar_buffer buffer(1024);
		HANDLE h = ptrFindFirstVolumeMountPointW(root.c_str(), buffer.get(), static_cast<DWORD>(buffer.size()));
		if (h != INVALID_HANDLE_VALUE)
			volume = buffer.get();
		return h;
	}
	BOOL FindNextVolumeMountPoint(HANDLE hVolume, std::wstring &volume) {
		if (ptrFindNextVolumeMountPointW == NULL || hVolume == INVALID_HANDLE_VALUE)
			return FALSE;
		hlp::tchar_buffer buffer(1024);
		BOOL r = ptrFindNextVolumeMountPointW(hVolume, buffer.get(), static_cast<DWORD>(buffer.size()));
		if (r)
			volume = buffer.get();
		return r;
	}

	void GetVolumeInformationByHandle(HANDLE hVolume, std::wstring &name, std::wstring &fs) {
		if (ptrGetVolumeInformationByHandleW == NULL || hVolume == INVALID_HANDLE_VALUE)
			return;
		hlp::tchar_buffer volumeName(1024);
		hlp::tchar_buffer fileSysName(1024);
		DWORD maximumComponentLength, fileSystemFlags;

		if (!ptrGetVolumeInformationByHandleW(hVolume, volumeName.get(), volumeName.size(), 
			NULL, &maximumComponentLength, &fileSystemFlags, fileSysName.get(), static_cast<DWORD>(fileSysName.size()))) {
				NSC_LOG_ERROR("Failed to get volume information: " + error::lookup::last_error());
		} else {
			name = volumeName.get();
			fs = fileSysName.get();
		}
	}

	bool getVolumeInformation(std::wstring volume, std::wstring &name, std::wstring &fs) {
		hlp::tchar_buffer volumeName(1024);
		hlp::tchar_buffer fileSysName(1024);
		DWORD maximumComponentLength, fileSystemFlags;

		if (!GetVolumeInformation(volume.c_str(), volumeName.get(), volumeName.size(), 
			NULL, &maximumComponentLength, &fileSystemFlags, fileSysName.get(), static_cast<DWORD>(fileSysName.size()))) {
				DWORD dwErr = GetLastError();
				if (dwErr == ERROR_PATH_NOT_FOUND)
					return false;
				if (dwErr != ERROR_NOT_READY)
					NSC_LOG_ERROR("Failed to get volume information " + utf8::cvt<std::string>(volume) + ": " + error::lookup::last_error());
		} else {
			name = volumeName.get();
			fs = fileSysName.get();
		}
		return true;
	}

	std::list<std::wstring> GetVolumePathNamesForVolumeName(std::wstring volume) {
		std::list<std::wstring> ret;
		if (ptrGetVolumePathNamesForVolumeNameW == NULL)
			return ret;
		hlp::tchar_buffer buffer(1024);
		DWORD returnLen = 0;
		if (!ptrGetVolumePathNamesForVolumeNameW(volume.c_str(), buffer.get(), buffer.size(), &returnLen)) {
			NSC_LOG_ERROR("Failed to get mountpoints: " + error::lookup::last_error());
			return ret;
		} else {
			DWORD last = 0;
			for (DWORD i=0;i<returnLen;i++) {
				if (buffer[i] == 0) {
					std::wstring item = buffer.get(last);
					if (!item.empty())
						ret.push_back(item);
					last = i+1;
				}
			}
			return ret;
		}
	}



	bool GetVolumeNameForVolumeMountPoint(std::wstring volumeMountPoint, std::wstring &volumeName) {
		hlp::tchar_buffer buffer(1024);
		if (ptrGetVolumeNameForVolumeMountPointW(volumeMountPoint.c_str(), buffer.get(), static_cast<DWORD>(buffer.size()))) {
			volumeName = buffer;
			return true;
		}
		return false;
	}
	std::wstring GetVolumeNameForVolumeMountPoint(std::wstring volumeMountPoint) {
		std::wstring volumeName;
		GetVolumeNameForVolumeMountPoint(volumeMountPoint, volumeName);
		return volumeName;
	}

	std::list<std::wstring> find_mount_points(std::wstring &root, std::wstring &name) {
		std::list<std::wstring> ret;
		std::wstring volume;
		HANDLE hVol = FindFirstVolumeMountPoint(root, volume);
		if (hVol == INVALID_HANDLE_VALUE) {
			DWORD dwErr = GetLastError();
			if (dwErr != ERROR_NO_MORE_FILES && dwErr != ERROR_ACCESS_DENIED)
				NSC_LOG_ERROR_STD("Failed to enumerate volumes " + utf8::cvt<std::string>(name) + ": " + error::lookup::last_error(dwErr));
			return ret;
		}
		BOOL bFlag = TRUE;
		while (bFlag) {
			ret.push_back(volume);
			bFlag = FindNextVolumeMountPoint(hVol, volume);
		}
		CloseHandle(hVol);
		return ret;
	}

	std::list<drive_container> get_volumes() {
		std::list<drive_container> ret;
		std::wstring volume;
		HANDLE hVol = FindFirstVolume(volume);
		if (hVol == INVALID_HANDLE_VALUE) {
			NSC_LOG_ERROR_STD("Failed to enumerate volumes");
			return ret;
		}
		BOOL bFlag = TRUE;
		while (bFlag) {
			std::wstring name, fs;
			bool is_valid = getVolumeInformation(volume, name, fs);

			bool found_mp = false;
			std::string title = utf8::cvt<std::string>(name);
			BOOST_FOREACH(const std::wstring &s, GetVolumePathNamesForVolumeName(volume)) {
				ret.push_back(drive_container(utf8::cvt<std::string>(volume), utf8::cvt<std::string>(s), title));
				found_mp = true;
			}
			if (!found_mp && is_valid)
				ret.push_back(drive_container(utf8::cvt<std::string>(volume), "", title));
			bFlag = FindNextVolume(hVol, volume);
		}
		FindVolumeClose(hVol);
		return ret;
	}

	std::wstring get_title(std::wstring volume) {
		std::wstring title, fs;
		getVolumeInformation(volume, title, fs);
		return title;
	}


};


void add_missing(std::list<drive_container> &drives, std::vector<std::string> &exclude_drives, const std::string volume, const std::string drive, const std::string title) {
	if (!drive.empty()) {
		if (std::find(exclude_drives.begin(), exclude_drives.end(), drive) == exclude_drives.end()) {
			drives.push_back(drive_container(volume, drive, title));
			exclude_drives.push_back(drive);
		}
	} else if (!volume.empty()) {
		if (std::find(exclude_drives.begin(), exclude_drives.end(), volume) == exclude_drives.end()) {
			drives.push_back(drive_container(volume, drive, title));
			exclude_drives.push_back(volume);
		}
	} else {
		drives.push_back(drive_container(volume, drive, title));
	}
}
void find_all_volumes(std::list<drive_container> &drives, std::vector<std::string> &exclude_drives, volume_helper helper) {
	BOOST_FOREACH(const drive_container &d, helper.get_volumes()) {
		add_missing(drives, exclude_drives, d.id, d.letter, d.name);
	}
}


void find_all_drives(std::list<drive_container> &drives, std::vector<std::string> &exclude_drives, volume_helper helper) {
	DWORD bufSize = GetLogicalDriveStrings(0, NULL) + 5;
	hlp::tchar_buffer buffer(bufSize);

	if (GetLogicalDriveStrings(bufSize, buffer.get()) > 0) {
		for (std::size_t i = 0; i < buffer.size();) {
			std::wstring drv = buffer.get(i);
			if (drv.empty())
				break;
			std::string drive = utf8::cvt<std::string>(drv);
			if (std::find(exclude_drives.begin(), exclude_drives.end(), drive) == exclude_drives.end()) {
				std::wstring volume = helper.GetVolumeNameForVolumeMountPoint(drv);
				std::string title = "";
				if (!volume.empty())
					title = utf8::cvt<std::string>(helper.get_title(volume));
				add_missing(drives, exclude_drives, utf8::cvt<std::string>(volume), drive, title);
			}
			i += drv.size()+1;
		}
	} else
		throw nscp_exception("Failed to get volume list: " + error::lookup::last_error());
}

std::list<drive_container> find_drives(std::vector<std::string> drives) {
	volume_helper helper;
	std::list<drive_container> ret;
	std::vector<std::string> found_drives;
	BOOST_FOREACH(const std::string &d, drives) {
		if (d == "all-volumes" || d == "volumes") {
			find_all_volumes(ret, found_drives, helper);
		} else if (d == "all-drives" || d == "drives") {
			find_all_drives(ret, found_drives, helper);
		} else if (d == "all" || d == "*") {
			find_all_drives(ret, found_drives, helper);
			find_all_volumes(ret, found_drives, helper);
		} else {
			ret.push_back(drive_container(d, d, ""));
		}
	}
	return ret;
}
void add_custom_options(po::options_description desc) {}

void check_drive::check(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	modern_filter::data_container data;
	modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
	std::vector<std::string> drives, excludes;
	bool ignore_unreadable = false, total = false;
	double magic;

	filter_type filter;
	filter_helper.add_options("used > 80%", "used > 90%", "", filter.get_filter_syntax(), "unknown");
	filter_helper.add_syntax("${status} ${problem_list}", filter.get_format_syntax(), "${drive_or_name}: ${used}/${size} used", "${drive_or_id}", "%(status): No drives found", "%(status) All %(count) drive(s) are ok");
	filter_helper.get_desc().add_options()
		("drive", po::value<std::vector<std::string>>(&drives), 
		"The drives to check.\nMultiple options can be used to check more then one drive or wildcards can be used to indicate multiple drives to check. Examples: drive=c, drive=d:, drive=*, drive=all-volumes, drive=all-drives")
		("ignore-unreadable", po::bool_switch(&ignore_unreadable)->implicit_value(true),
		"Ignore drives which are not reachable by the current user.\nFor instance Microsoft Office creates a drive which cannot be read by normal users.")
		("magic", po::value<double>(&magic), "Magic number for use with scaling drive sizes.")
		("exclude", po::value<std::vector<std::string>>(&excludes), "A list of drives not to check")
		("total", po::bool_switch(&total), "Include the total of all matching drives")
		;
	add_custom_options(filter_helper.get_desc());

	if (!filter_helper.parse_options())
		return;

	if (!filter_helper.build_filter(filter))
		return;

	if (drives.empty())
		drives.push_back("*");
	if (!total) {
		std::vector<std::string>::iterator it = std::find(drives.begin(), drives.end(), "total");
		if (it != drives.end()) {
			total = true;
			drives.erase(it);
		}
	}
	boost::shared_ptr<filter_obj> total_obj(new filter_obj(drive_container("total", "total", "total")));
	if (total)
		total_obj->make_total();

	BOOST_FOREACH(const drive_container &drive, find_drives(drives)) {
		if (std::find(excludes.begin(), excludes.end(), drive.letter)!=excludes.end()
			|| std::find(excludes.begin(), excludes.end(), drive.name)!=excludes.end())
			continue;
		boost::shared_ptr<filter_obj> obj = get_details(drive, ignore_unreadable);
		filter.match(obj);
		if (filter.has_errors())
			return nscapi::protobuf::functions::set_response_bad(*response, "Filter processing failed: " + filter.get_errors());
		if (total) {
			obj->get_size(filter.context);
			total_obj->append(obj);
		}
	}
	if (total) {
		filter.match(total_obj);
		if (filter.has_errors())
			return nscapi::protobuf::functions::set_response_bad(*response, "Filter processing failed: " + filter.get_errors());
	}

	modern_filter::perf_writer writer(response);
	filter_helper.post_process(filter, &writer);
}


