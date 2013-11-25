#include "check_drive.hpp"

#ifdef WIN32
#include <Windows.h>
#endif

#include <boost/bind.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <strEx.h>
#include <char_buffer.hpp>
#include <error.hpp>

#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_plugin_interface.hpp>

#include <parsers/filter/modern_filter.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace npo = nscapi::program_options;
namespace po = boost::program_options;

struct filter_obj {
	std::string drive;
	std::string name;
	UINT drive_type;
	long long user_free;
	long long total_free;
	long long drive_size;
	bool has_size;
	bool has_type;
	bool unreadable;

	filter_obj() : drive_type(0), user_free(0), total_free(0), drive_size(0), has_size(false), has_type(false), unreadable(false) {}
	filter_obj(std::string drive, std::string name) 
		: drive(drive)
		, name(name)
		, drive_type(0)
		, user_free(0)
		, total_free(0)
		, drive_size(0)
		, has_size(false)
		, has_type(false)
		, unreadable(true)
	{};

	std::string get_drive(parsers::where::evaluation_context) const { return drive; }
	std::string get_name() const { return name; }

	long long get_user_free(parsers::where::evaluation_context context) { get_size(context); return user_free; }
	long long get_total_free(parsers::where::evaluation_context context) { get_size(context); return total_free; }
	long long get_drive_size(parsers::where::evaluation_context context) { get_size(context); return drive_size; }
	long long get_total_used(parsers::where::evaluation_context context) { get_size(context); return drive_size-total_free; }
	long long get_user_used(parsers::where::evaluation_context context) { get_size(context); return drive_size-user_free; }

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

	long long  get_type(parsers::where::evaluation_context context) {
		if (has_type)
			return drive_type;
		std::wstring drv = utf8::cvt<std::wstring>(drive);
		drive_type = GetDriveType(drv.c_str());
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
		std::wstring drv = utf8::cvt<std::wstring>(drive);
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
			context->error("Failed to get size for: " + name + error::lookup::last_error(err));
			unreadable = err == ERROR_ACCESS_DENIED;
			has_size = true;
			return;
		}
		has_size = true;
		user_free = freeBytesAvailableToCaller.QuadPart;
		total_free = totalNumberOfFreeBytes.QuadPart;
		drive_size = totalNumberOfBytes.QuadPart;
	}
};


parsers::where::node_type calculate_free(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
	std::list<parsers::where::node_type> list = subject->get_list_value(context);
	if (list.size() != 2) {
		context->error("Invalid list value");
		return parsers::where::factory::create_false();
	}
	std::list<parsers::where::node_type>::const_iterator cit = list.begin();
	parsers::where::node_type amount = *cit;
	++cit;
	parsers::where::node_type unit = *cit;

	long long percentage = amount->get_int_value(context);
	long long value = (object->get_drive_size(context)*percentage)/100;
	return parsers::where::factory::create_int(value);
}

parsers::where::node_type calculate_used(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
	std::list<parsers::where::node_type> list = subject->get_list_value(context);
	if (list.size() != 2) {
		context->error("Invalid list value");
		return parsers::where::factory::create_false();
	}
	std::list<parsers::where::node_type>::const_iterator cit = list.begin();
	parsers::where::node_type amount = *cit;
	++cit;
	parsers::where::node_type unit = *cit;

	long long percentage = amount->get_int_value(context);
	long long value = (object->get_drive_size(context)*(100-percentage))/100;
	return parsers::where::factory::create_int(value);
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
			("name", boost::bind(&filter_obj::get_name, _1), "Descriptive name of drive")
			("drive", &filter_obj::get_drive, "Technical name of drive")
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
 			("size", &filter_obj::get_drive_size, "Total size of drive")
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
			;

		registry_.add_human_string()
			("free", &filter_obj::get_total_free_human, "")
			("total_free", &filter_obj::get_total_free_human, "")
			("user_free", &filter_obj::get_user_free_human, "")
			("size", &filter_obj::get_drive_size_human, "")
			("total_used", &filter_obj::get_total_used_human, "")
			("used", &filter_obj::get_total_used_human, "")
			("user_used", &filter_obj::get_user_used_human, "")
			;


		registry_.add_converter()
			(type_custom_total_free, &calculate_free)
			(type_custom_total_used, &calculate_used)
			(type_custom_type, &convert_type)
			;

	}
};

typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;


struct drive_container {
	std::string drive;
	std::string name;
	drive_container(std::string drive, std::string name) : drive(drive), name(name) {}
};


boost::shared_ptr<filter_obj> get_details(const drive_container &drive, bool ignore_errors) {
	return boost::make_shared<filter_obj>(drive.drive, drive.name);
}


class volume_helper {
	typedef HANDLE (WINAPI *typeFindFirstVolumeW)( __out_ecount(cchBufferLength) LPWSTR lpszVolumeName, __in DWORD cchBufferLength);
	typedef BOOL (WINAPI *typeFindNextVolumeW)( __inout HANDLE hFindVolume, __out_ecount(cchBufferLength) LPWSTR lpszVolumeName, __in DWORD cchBufferLength);
	typedef HANDLE (WINAPI *typeFindFirstVolumeMountPointW)( __in LPCWSTR lpszRootPathName, __out_ecount(cchBufferLength) LPWSTR lpszVolumeMountPoint, __in DWORD cchBufferLength );
	typedef BOOL (WINAPI *typeGetVolumeNameForVolumeMountPointW)( __in LPCWSTR lpszVolumeMountPoint, __out_ecount(cchBufferLength) LPWSTR lpszVolumeName, __in DWORD cchBufferLength );
	typeFindFirstVolumeW ptrFindFirstVolumeW;
	typeFindNextVolumeW ptrFindNextVolumeW;
	typeFindFirstVolumeMountPointW ptrFindFirstVolumeMountPointW;
	typeGetVolumeNameForVolumeMountPointW ptrGetVolumeNameForVolumeMountPointW;
	HMODULE hLib;

public:
	typedef std::map<std::string,std::string> map_type;

public:
	volume_helper() : ptrFindFirstVolumeW(NULL) {
		hLib = ::LoadLibrary(_TEXT("KERNEL32"));
		if (hLib) {
			// Find PSAPI functions
			ptrFindFirstVolumeW = (typeFindFirstVolumeW)::GetProcAddress(hLib, "FindFirstVolumeW");
			ptrFindNextVolumeW = (typeFindNextVolumeW)::GetProcAddress(hLib, "FindNextVolumeW");
			ptrFindFirstVolumeMountPointW = (typeFindFirstVolumeMountPointW)::GetProcAddress(hLib, "FindFirstVolumeMountPointW");
			ptrGetVolumeNameForVolumeMountPointW = (typeGetVolumeNameForVolumeMountPointW)::GetProcAddress(hLib, "GetVolumeNameForVolumeMountPointW");
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

	void getVolumeInformation(std::wstring volume, std::wstring &name) {
		hlp::tchar_buffer volumeName(1024);
		hlp::tchar_buffer fileSysName(1024);
		DWORD maximumComponentLength, fileSystemFlags;

		if (!GetVolumeInformation(volume.c_str(), volumeName.get(), volumeName.size(), 
			NULL, &maximumComponentLength, &fileSystemFlags, fileSysName.get(), static_cast<DWORD>(fileSysName.size()))) {
				NSC_LOG_ERROR_WA("Failed to get volume information: ", volume);
		} else {
			name = volumeName.get();
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

	map_type get_volumes(map_type alias) {
		map_type ret;
		std::wstring volume;
		HANDLE hVol = FindFirstVolume(volume);
		if (hVol == INVALID_HANDLE_VALUE) {
			NSC_LOG_ERROR_STD("Failed to enumerate volumes");
			return ret;
		}
		BOOL bFlag = TRUE;
		while (bFlag) {
			map_type::iterator it = alias.find(utf8::cvt<std::string>(volume));
			if (it != alias.end())
				ret[utf8::cvt<std::string>(volume)] = (*it).second;
			else
				ret[utf8::cvt<std::string>(volume)] = utf8::cvt<std::string>(get_title(volume));
			bFlag = FindNextVolume(hVol, volume);
		}
		return ret;
	}

	std::wstring get_title(std::wstring volume) {
		std::wstring title;
		getVolumeInformation(volume, title);
		return title;
	}


};

void find_all_volumes(std::list<drive_container> &drives) {
	volume_helper helper;
	DWORD bufSize = GetLogicalDriveStrings(0, NULL)+5;
	TCHAR *buffer = new TCHAR[bufSize+10];
	if (GetLogicalDriveStrings(bufSize, buffer) > 0) {
		while (buffer[0] != 0) {
			std::wstring drv = buffer;
			drives.push_back(drive_container(utf8::cvt<std::string>(drv), utf8::cvt<std::string>(helper.GetVolumeNameForVolumeMountPoint(drv))));
			buffer = &buffer[drv.size()];
			buffer++;
		}
	} else
		throw nscp_exception("Failed to get volume list: " + error::lookup::last_error());
}
void find_all_drives(std::list<drive_container> &drives) {
	DWORD dwDrives = GetLogicalDrives();
	int idx = 0;
	while (dwDrives != 0) {
		if (dwDrives & 0x1) {
			std::string drv;
			drv += static_cast<char>('A' + idx); drv += ":\\";
			drives.push_back(drive_container(drv, ""));
		}
		idx++;
		dwDrives >>= 1;
	}
}

std::list<drive_container> find_drives(std::vector<std::string> drives) {
	std::list<drive_container> ret;
	BOOST_FOREACH(const std::string &d, drives) {
		if (d == "all-volumes") {
			find_all_volumes(ret);
		} else if (d == "all-drives") {
			find_all_drives(ret);
		} else if (d == "all" || d == "*") {
			find_all_volumes(ret);
		} else {
			ret.push_back(drive_container(d, ""));
		}
	}
	return ret;
}
void add_custom_options(po::options_description desc) {}

void check_drive::check(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	modern_filter::data_container data;
	modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
	std::vector<std::string> drives, excludes;
	bool ignore_unreadable = false;
	double magic;

	filter_type filter;
	filter_helper.add_options(filter.get_filter_syntax(), "All drives ok");
	filter_helper.add_syntax("${problem_list}", filter.get_format_syntax(), "${drive}: ${used}/${size} used", "${drive}");
	filter_helper.get_desc().add_options()
		("drive", po::value<std::vector<std::string>>(&drives), 
		"The drives to check.\nMultiple options can be used to check more then one drive or wildcards can be used to indicate multiple drives to check. Examples: drive=c, drive=d:, drive=*, drive=all-volumes, drive=all-drives")
		("ignore-unreadable", po::bool_switch(&ignore_unreadable)->implicit_value(true),
		"Ignore drives which are not reachable by the current user.\nFor instance Microsoft Office creates a drive which cannot be read by normal users.")
		("magic", po::value<double>(&magic), "Magic number for use with scaling drive sizes.")
		("exclude", po::value<std::vector<std::string>>(&excludes), 
		"A list of drives not to check")
		;
	add_custom_options(filter_helper.get_desc());

	if (!filter_helper.parse_options())
		return;

	if (filter_helper.empty())
		filter_helper.set_default("used > 80%", "used > 90%");

	if (!filter_helper.build_filter(filter))
		return;

	if (drives.empty())
		drives.push_back("*");

	BOOST_FOREACH(const drive_container &drive, find_drives(drives)) {
		if (std::find(excludes.begin(), excludes.end(), drive.drive)!=excludes.end()
			|| std::find(excludes.begin(), excludes.end(), drive.name)!=excludes.end())
			continue;
		boost::shared_ptr<filter_obj> obj = get_details(drive, ignore_unreadable);
		boost::tuple<bool,bool> ret = filter.match(obj);
		if (ret.get<1>()) {
			break;
		}
		if (filter.has_errors())
			return nscapi::protobuf::functions::set_response_bad(*response, "Filter processing failed (see log for details)");
	}

	modern_filter::perf_writer writer(response);
	filter_helper.post_process(filter, &writer);
}


