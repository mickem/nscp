/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "check_drive.hpp"

#include <nsclient/nsclient_exception.hpp>

#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

#include <parsers/filter/modern_filter.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/where/helpers.hpp>

#include <char_buffer.hpp>
#include <error/error.hpp>
#include <str/format.hpp>

#include <boost/bind.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#ifdef WIN32
#include <Windows.h>
#include <winioctl.h>
#endif


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
	std::string letter_only;
	std::string name;
	bool is_mounted;
	typedef enum drive_flags {
		df_none = 0,
		df_removable = 0x1,
		df_hotplug = 0x2,
		df_mounted = 0x4,
		df_readable = 0x8,
		df_writable = 0x16,
		df_erasable = 0x32

	};

	unsigned long long type;
	drive_flags flags;
private:
	drive_container() : is_mounted(false), type(0), flags(df_none) {}
public:
	drive_container(std::string id, std::string letter, std::string name, bool is_mounted, unsigned long long type, drive_flags flags) : id(id), letter(letter), name(name), is_mounted(is_mounted), type(type), flags(flags) {
		letter_only = letter.substr(0, 1);
	}
	drive_container(const drive_container &other) : id(other.id), letter(other.letter), letter_only(other.letter_only), name(other.name), is_mounted(other.is_mounted), type(other.type), flags(other.flags) {}
	drive_container& operator=(const drive_container &other) {
		id = other.id;
		letter = other.letter;
		letter_only = other.letter_only;
		name = other.name;
		is_mounted = other.is_mounted;
		type = other.type;
		flags = other.flags;
		return *this;
	}
};

inline drive_container::drive_flags operator|=(drive_container::drive_flags &a, const drive_container::drive_flags b) {
	return a=static_cast<drive_container::drive_flags>(static_cast<int>(a) | static_cast<int>(b));
}

struct filter_obj {
	drive_container drive;
	UINT drive_type;
	long long user_free;
	long long total_free;
	long long drive_size;
	bool has_size;
	bool has_type;
	bool unreadable;

	filter_obj(const drive_container drive)
		: drive(drive)
		, drive_type(0)
		, user_free(0)
		, total_free(0)
		, drive_size(0)
		, has_size(false)
		, has_type(false)
		, unreadable(true) {};

	std::string get_drive(parsers::where::evaluation_context) const { return drive.letter; }
	std::string get_letter(parsers::where::evaluation_context) const {
		if (drive.letter.size() >= 2) {
			if (drive.letter[1] == ':') {
				return drive.letter.substr(0, 1);
			}
		}
		return "";
	}
	std::string get_name(parsers::where::evaluation_context) const { return drive.name; }
	std::string get_id(parsers::where::evaluation_context) const { return drive.id; }
	std::string get_drive_or_id(parsers::where::evaluation_context) const { return drive.letter.empty() ? drive.id : drive.letter; }
	std::string get_drive_or_name(parsers::where::evaluation_context) const { return drive.letter.empty() ? drive.name : drive.letter; }
	std::string get_flags(parsers::where::evaluation_context) const { 
		std::string ret;
		if ((drive.flags & drive_container::df_mounted) == drive_container::df_mounted)
			str::format::append_list(ret, "mounted");
		if ((drive.flags & drive_container::df_hotplug) == drive_container::df_hotplug)
			str::format::append_list(ret, "hotplug");
		if ((drive.flags & drive_container::df_removable) == drive_container::df_removable)
			str::format::append_list(ret, "removable");
		if ((drive.flags & drive_container::df_readable) == drive_container::df_readable)
			str::format::append_list(ret, "readable");
		if ((drive.flags & drive_container::df_writable) == drive_container::df_writable)
			str::format::append_list(ret, "writable");
		if ((drive.flags & drive_container::df_erasable) == drive_container::df_erasable)
			str::format::append_list(ret, "erasable");
		return ret;
	}

	long long get_user_free(parsers::where::evaluation_context context) { get_size(context); return user_free; }
	long long get_total_free(parsers::where::evaluation_context context) { get_size(context); return total_free; }
	long long get_drive_size(parsers::where::evaluation_context context) { get_size(context); return drive_size; }
	long long get_total_used(parsers::where::evaluation_context context) { get_size(context); return drive_size - total_free; }
	long long get_user_used(parsers::where::evaluation_context context) { get_size(context); return drive_size - user_free; }

	long long get_user_free_pct(parsers::where::evaluation_context context) { get_size(context); return drive_size == 0 ? 0 : (user_free * 100 / drive_size); }
	long long get_total_free_pct(parsers::where::evaluation_context context) { get_size(context); return drive_size == 0 ? 0 : (total_free * 100 / drive_size); }
	long long get_user_used_pct(parsers::where::evaluation_context context) { return 100 - get_user_free_pct(context); }
	long long get_total_used_pct(parsers::where::evaluation_context context) { return 100 - get_total_free_pct(context); }
	long long get_is_mounted(parsers::where::evaluation_context context) {
		return drive.is_mounted ? 1 : 0;
	}

	std::string get_user_free_human(parsers::where::evaluation_context context) {
		return str::format::format_byte_units(get_user_free(context));
	}
	std::string get_total_free_human(parsers::where::evaluation_context context) {
		return str::format::format_byte_units(get_total_free(context));
	}
	std::string get_drive_size_human(parsers::where::evaluation_context context) {
		return str::format::format_byte_units(get_drive_size(context));
	}
	std::string get_total_used_human(parsers::where::evaluation_context context) {
		return str::format::format_byte_units(get_total_used(context));
	}
	std::string get_user_used_human(parsers::where::evaluation_context context) {
		return str::format::format_byte_units(get_user_used(context));
	}

	std::string get_type_as_string(parsers::where::evaluation_context context) {
		return type_to_string(get_type(context));
	}

	long long get_removable(parsers::where::evaluation_context context) const {
		return (drive.flags & drive_container::df_removable) == drive_container::df_removable;
	}
	long long get_hotplug(parsers::where::evaluation_context context) const {
		return (drive.flags & drive_container::df_hotplug) == drive_container::df_hotplug;
	}
	long long get_mounted(parsers::where::evaluation_context context) const {
		return (drive.flags & drive_container::df_mounted) == drive_container::df_mounted;
	}
	long long get_readable(parsers::where::evaluation_context context) const {
		return (drive.flags & drive_container::df_readable) == drive_container::df_readable;
	}
	long long get_writable(parsers::where::evaluation_context context) const {
		return (drive.flags & drive_container::df_writable) == drive_container::df_writable;
	}
	long long get_erasable(parsers::where::evaluation_context context) const {
		return (drive.flags & drive_container::df_erasable) == drive_container::df_erasable;
	}
	long long get_media_type(parsers::where::evaluation_context context) const {
		return drive.type;
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
			context->error("Failed to get size for " + utf8::cvt<std::string>(drv) + ": " + error::lookup::last_error(err));
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
	parsers::where::helpers::read_arg_type value = parsers::where::helpers::read_arguments(context, subject, "%");
	double number = value.get<1>();
	std::string unit = value.get<2>();

	if (unit == "%") {
		number = (static_cast<double>(object->get_drive_size(context))*number) / 100.0;
	} else {
		number = str::format::decode_byte_units(number, unit);
	}
	return parsers::where::factory::create_int(number);
}

parsers::where::node_type calculate_user_used(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
	parsers::where::helpers::read_arg_type value = parsers::where::helpers::read_arguments(context, subject, "%");
	double number = value.get<1>();
	std::string unit = value.get<2>();

	if (unit == "%") {
		number = (static_cast<double>(object->get_user_free(context))*number) / 100.0;
	} else {
		number = str::format::decode_byte_units(number, unit);
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
			("letter", &filter_obj::get_letter, "Letter the drive is mountedd on")
			("flags", &filter_obj::get_flags, "String representation of flags")
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
			("mounted", parsers::where::type_int, &filter_obj::get_is_mounted, "Check if a drive is mounted")
			("removable", &filter_obj::get_removable, "1 (true) if drive is removable")
			("hotplug", &filter_obj::get_hotplug, "1 (true) if drive is hotplugable")
//			("mounted", &filter_obj::get_mounted, "1 (true) if drive is mounted")
			("readable", &filter_obj::get_readable, "1 (true) if drive is readable")
			("writable", &filter_obj::get_writable, "1 (true) if drive is writable")
			("erasable", &filter_obj::get_erasable, "1 (true) if drive is erasable")
			("media_type", &filter_obj::get_media_type, "Get the media type")
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

class volume_helper {
	typedef HANDLE(WINAPI *typeFindFirstVolumeW)(__out_ecount(cchBufferLength) LPWSTR lpszVolumeName, __in DWORD cchBufferLength);
	typedef BOOL(WINAPI *typeFindNextVolumeW)(__inout HANDLE hFindVolume, __out_ecount(cchBufferLength) LPWSTR lpszVolumeName, __in DWORD cchBufferLength);
	typedef HANDLE(WINAPI *typeFindFirstVolumeMountPointW)(__in LPCWSTR lpszRootPathName, __out_ecount(cchBufferLength) LPWSTR lpszVolumeMountPoint, __in DWORD cchBufferLength);
	typedef BOOL(WINAPI *typeFindNextVolumeMountPointW)(__inout HANDLE hFindVolume, __out_ecount(cchBufferLength) LPWSTR lpszVolumeName, __in DWORD cchBufferLength);
	typedef BOOL(WINAPI *typeGetVolumeNameForVolumeMountPointW)(__in LPCWSTR lpszVolumeMountPoint, __out_ecount(cchBufferLength) LPWSTR lpszVolumeName, __in DWORD cchBufferLength);
	typedef BOOL(WINAPI *typeGetVolumeInformationByHandleW)(_In_ HANDLE hFile, _Out_opt_ LPWSTR lpVolumeNameBuffer, _In_ DWORD nVolumeNameSize, _Out_opt_ LPDWORD lpVolumeSerialNumber,
		_Out_opt_ LPDWORD  lpMaximumComponentLength, _Out_opt_ LPDWORD lpFileSystemFlags, _Out_opt_ LPWSTR lpFileSystemNameBuffer, _In_ DWORD nFileSystemNameSize);
	typedef BOOL(WINAPI *typeGetVolumePathNamesForVolumeNameW)(_In_ LPCTSTR lpszVolumeName, _Out_ LPTSTR lpszVolumePathNames, _In_ DWORD cchBufferLength, _Out_ PDWORD lpcchReturnLength);

	typeFindFirstVolumeW ptrFindFirstVolumeW;
	typeFindNextVolumeW ptrFindNextVolumeW;
	typeFindFirstVolumeMountPointW ptrFindFirstVolumeMountPointW;
	typeFindNextVolumeMountPointW ptrFindNextVolumeMountPointW;
	typeGetVolumeNameForVolumeMountPointW ptrGetVolumeNameForVolumeMountPointW;
	typeGetVolumeInformationByHandleW ptrGetVolumeInformationByHandleW;
	typeGetVolumePathNamesForVolumeNameW ptrGetVolumePathNamesForVolumeNameW;
	HMODULE hLib;

public:
	typedef std::map<std::string, std::string> map_type;

public:
	volume_helper()
		: ptrFindFirstVolumeW(NULL)
		, ptrFindNextVolumeW(NULL)
		, ptrFindFirstVolumeMountPointW(NULL)
		, ptrFindNextVolumeMountPointW(NULL)
		, ptrGetVolumeNameForVolumeMountPointW(NULL)
		, ptrGetVolumeInformationByHandleW(NULL)
		, ptrGetVolumePathNamesForVolumeNameW(NULL) {
		hLib = ::LoadLibrary(L"KERNEL32");
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

	~volume_helper() {}

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

	bool getVolumeInformation(std::wstring volume, std::wstring &name, std::wstring &fs, unsigned long long &type, drive_container::drive_flags &flags) {
		hlp::tchar_buffer volumeName(1024);
		hlp::tchar_buffer fileSysName(1024);
		DWORD maximumComponentLength, fileSystemFlags;
		type = 0;
		std::wstring vfile = volume;
		if (vfile[vfile.size() - 1] == '\\')
			vfile = vfile.substr(0, vfile.size() - 1);

		HANDLE hDevice = CreateFile(vfile.c_str(), 0, 0, 0, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, 0);
		if (hDevice != INVALID_HANDLE_VALUE) {

			DWORD ReturnedSize;
			STORAGE_HOTPLUG_INFO Info = { 0 };

			if (DeviceIoControl(hDevice, IOCTL_STORAGE_GET_HOTPLUG_INFO, 0, 0, &Info, sizeof(Info), &ReturnedSize, NULL)) {
				if (Info.MediaRemovable)
					flags |= drive_container::df_removable;
				if (Info.DeviceHotplug)
					flags |= drive_container::df_hotplug;
			}

			hlp::buffer<TCHAR, GET_MEDIA_TYPES*> mediaType(2048);
			DWORD err = 0;
			while (DeviceIoControl(hDevice, IOCTL_STORAGE_GET_MEDIA_TYPES_EX, 0, 0, mediaType.get(), mediaType.size(), &ReturnedSize, NULL) == FALSE && (err = GetLastError()) == ERROR_INSUFFICIENT_BUFFER) {
				mediaType.resize(mediaType.size() * 2);
			}


			if (err == 0 && mediaType.get()->MediaInfoCount > 0) {

				DWORD Characteristics = 0;
				// Supports: Disk, CD, DVD
				type = mediaType.get()->DeviceType;
				if (mediaType.get()->DeviceType == FILE_DEVICE_DISK || mediaType.get()->DeviceType == FILE_DEVICE_CD_ROM || mediaType.get()->DeviceType == FILE_DEVICE_DVD) {
					if (Info.MediaRemovable) {
						Characteristics = mediaType.get()->MediaInfo[0].DeviceSpecific.RemovableDiskInfo.MediaCharacteristics;
					} else {
						Characteristics = mediaType.get()->MediaInfo[0].DeviceSpecific.DiskInfo.MediaCharacteristics;
					}

					if (Characteristics & MEDIA_CURRENTLY_MOUNTED)
						flags |= drive_container::df_mounted;
					if (Characteristics & (MEDIA_READ_ONLY | MEDIA_READ_WRITE))
						flags |= drive_container::df_readable;
					if (((Characteristics & MEDIA_READ_WRITE) != 0 || (Characteristics & MEDIA_WRITE_ONCE) != 0) && (Characteristics & MEDIA_WRITE_PROTECTED) == 0 && (Characteristics & MEDIA_READ_ONLY) == 0)
						flags |= drive_container::df_writable;
					if (Characteristics & MEDIA_ERASEABLE)
						flags |= drive_container::df_erasable;
				}
			}

			CloseHandle(hDevice);
		}




		if (!GetVolumeInformation(volume.c_str(), volumeName.get(), volumeName.size(),
			NULL, &maximumComponentLength, &fileSystemFlags, fileSysName.get(), static_cast<DWORD>(fileSysName.size()))) {
			DWORD dwErr = GetLastError();
			if (dwErr == ERROR_PATH_NOT_FOUND)
				return false;
			if (dwErr != ERROR_NOT_READY)
				name = L"Failed to get volume information " + volume + L": " + utf8::cvt<std::wstring>(error::lookup::last_error());
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
			for (DWORD i = 0; i < returnLen; i++) {
				if (buffer[i] == 0) {
					std::wstring item = buffer.get(last);
					if (!item.empty())
						ret.push_back(item);
					last = i + 1;
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
			unsigned long long type;
			drive_container::drive_flags flags = drive_container::df_none;
			bool is_valid = getVolumeInformation(volume, name, fs, type, flags);

			bool found_mp = false;
			std::string title = utf8::cvt<std::string>(name);
			BOOST_FOREACH(const std::wstring &s, GetVolumePathNamesForVolumeName(volume)) {
				ret.push_back(drive_container(utf8::cvt<std::string>(volume), utf8::cvt<std::string>(s), title, true, type, flags));
				found_mp = true;
			}
			if (!found_mp && is_valid)
				ret.push_back(drive_container(utf8::cvt<std::string>(volume), "", title, false, type, flags));
			bFlag = FindNextVolume(hVol, volume);
		}
		FindVolumeClose(hVol);
		return ret;
	}
};

void add_missing(std::list<drive_container> &drives, std::vector<std::string> &exclude_drives, const drive_container &drive) {
	if (!drive.letter.empty()) {
		if (std::find(exclude_drives.begin(), exclude_drives.end(), drive.letter) == exclude_drives.end()) {
			drives.push_back(drive);
			exclude_drives.push_back(drive.letter);
		}
	} else if (!drive.id.empty()) {
		if (std::find(exclude_drives.begin(), exclude_drives.end(), drive.id) == exclude_drives.end()) {
			drives.push_back(drive);
			exclude_drives.push_back(drive.id);
		}
	} else {
		drives.push_back(drive);
	}
}
void find_all_volumes(std::list<drive_container> &drives, std::vector<std::string> &exclude_drives, volume_helper helper) {
	BOOST_FOREACH(const drive_container &d, helper.get_volumes()) {
		add_missing(drives, exclude_drives, d);
	}
}

drive_container get_dc_from_string(std::wstring folder, volume_helper &helper) {
	std::wstring volume = helper.GetVolumeNameForVolumeMountPoint(folder);
	unsigned long long type = 0;
	std::string title = "";
	drive_container::drive_flags flags = drive_container::df_none;
	if (!volume.empty()) {
		std::wstring wtitle, wfs;
		helper.getVolumeInformation(volume, wtitle, wfs, type, flags);
		title = utf8::cvt<std::string>(wtitle);
	}
	return drive_container(utf8::cvt<std::string>(volume), utf8::cvt<std::string>(folder), title, true, type, flags);
}
void find_all_drives(std::list<drive_container> &drives, std::vector<std::string> &exclude_drives, volume_helper &helper) {
	DWORD bufSize = GetLogicalDriveStrings(0, NULL) + 5;
	hlp::tchar_buffer buffer(bufSize);

	if (GetLogicalDriveStrings(bufSize, buffer.get()) > 0) {
		for (std::size_t i = 0; i < buffer.size();) {
			std::wstring drv = buffer.get(i);
			if (drv.empty())
				break;
			std::string drive = utf8::cvt<std::string>(drv);
			if (std::find(exclude_drives.begin(), exclude_drives.end(), drive) == exclude_drives.end()) {
				add_missing(drives, exclude_drives, get_dc_from_string(drv, helper));
			}
			i += drv.size()+1;
		}
	} else
		throw nsclient::nsclient_exception("Failed to get volume list: " + error::lookup::last_error());
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
			find_all_volumes(ret, found_drives, helper);
			find_all_drives(ret, found_drives, helper);
		} else {
			std::wstring drive = utf8::cvt<std::wstring>(d);
			if (d.length() == 1)
				drive = drive + L":";
			ret.push_back(get_dc_from_string(drive, helper));
		}
	}
	return ret;
}
void add_custom_options(po::options_description desc) {}

void check_drive::check(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	modern_filter::data_container data;
	modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
	std::vector<std::string> drives, excludes;
	bool ignore_unreadable = false, total = false, only_mounted = false;;
	double magic;

	filter_type filter;
	filter_helper.add_options("used > 80%", "used > 90%", "mounted = 1", filter.get_filter_syntax(), "unknown");
	filter_helper.add_syntax("${status} ${problem_list}", "${drive_or_name}: ${used}/${size} used", "${drive_or_id}", "%(status): No drives found", "%(status) All %(count) drive(s) are ok");
	filter_helper.get_desc().add_options()
		("drive", po::value<std::vector<std::string>>(&drives),
			"The drives to check.\nMultiple options can be used to check more then one drive or wildcards can be used to indicate multiple drives to check. Examples: drive=c, drive=d:, drive=*, drive=all-volumes, drive=all-drives")
		("ignore-unreadable", po::bool_switch(&ignore_unreadable)->implicit_value(true),
			"DEPRECATED (manually set filter instead) Ignore drives which are not reachable by the current user.\nFor instance Microsoft Office creates a drive which cannot be read by normal users.")
		("mounted", po::bool_switch(&only_mounted)->implicit_value(true),
			"DEPRECATED (this is now default) Show only mounted rives i.e. drives which have a mount point.")
		("magic", po::value<double>(&magic), "Magic number for use with scaling drive sizes.")
		("exclude", po::value<std::vector<std::string>>(&excludes), "A list of drives not to check")
		("total", po::bool_switch(&total), "Include the total of all matching drives")
		;
	add_custom_options(filter_helper.get_desc());

	if (!filter_helper.parse_options())
		return;

	if (only_mounted) {
		filter_helper.append_all_filters("and", "( mounted = 1  or media_type = 0 )");
	}
	if (ignore_unreadable) {
		filter_helper.append_all_filters("and", " ( mounted = 1 and readable = 1 or media_type = 0)");
	}

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
	std::list<std::string> buffer;
	BOOST_FOREACH(std::string e, excludes) {
		if (e.size() == 1) {
			buffer.push_back(boost::algorithm::to_upper_copy(e));
		}
		if (e.size() == 2 && e[1] == ':') {
			buffer.push_back(boost::algorithm::to_upper_copy(e.substr(0, 1)));
		}
	}
	if (!buffer.empty())
		excludes.insert(excludes.end(), buffer.begin(), buffer.end());
	drive_container total_dc("total", "total", "total", true, 0, drive_container::df_none);
	boost::shared_ptr<filter_obj> total_obj(new filter_obj(total_dc));
	if (total)
		total_obj->make_total();

	BOOST_FOREACH(const drive_container &drive, find_drives(drives)) {
		if (std::find(excludes.begin(), excludes.end(), drive.letter) != excludes.end()
			|| std::find(excludes.begin(), excludes.end(), drive.name) != excludes.end()
			|| std::find(excludes.begin(), excludes.end(), drive.letter_only) != excludes.end())
			continue;
		boost::shared_ptr<filter_obj> obj(new filter_obj(drive));
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

	filter_helper.post_process(filter);
}
