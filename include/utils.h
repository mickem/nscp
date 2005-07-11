#pragma once

#include <string>
#include <strEx.h>

void generate_crc32_table(void);
unsigned long calculate_crc32(const char *buffer, int buffer_size);

namespace socketHelpers {
	class allowedHosts {
	private:
		strEx::splitList allowedHosts_;
	public:
		void setAllowedHosts(strEx::splitList allowedHosts) {
			if ((!allowedHosts.empty()) && (allowedHosts.front() == "") )
				allowedHosts.pop_front();
			allowedHosts_ = allowedHosts;
		}
		bool inAllowedHosts(std::string s) {
			if (allowedHosts_.empty())
				return true;
			strEx::splitList::const_iterator cit;
			for (cit = allowedHosts_.begin();cit!=allowedHosts_.end();++cit) {
				if ( (*cit) == s)
					return true;
			}
			return false;
		}
	};
}







#define MAP_OPTIONS_BEGIN(args) \
	for (std::list<std::string>::const_iterator cit__=args.begin();cit__!=args.end();++cit__) { \
	std::pair<std::string,std::string> p__ = strEx::split(*cit__,"="); if (false) {}
#define MAP_OPTIONS_STR(value, obj) \
			else if (p__.first == value) { obj = p__.second; }
#define MAP_OPTIONS_STR_AND(value, obj, extra) \
			else if (p__.first == value) { obj = p__.second; extra;}
#define MAP_OPTIONS_BOOL_TRUE(value, obj) \
			else if (p__.first == value) { obj = true; }
#define MAP_OPTIONS_BOOL_FALSE(value, obj) \
			else if (p__.first == value) { obj = false; }
#define MAP_OPTIONS_BOOL_EX(value, obj, tStr, fStr) \
			else if ((p__.first == value)&&(p__.second == tStr)) { obj = true; } \
			else if ((p__.first == value)&&(p__.second == fStr)) { obj = false; }
#define MAP_OPTIONS_MISSING(arg, str) \
		else { arg = str + p__.first; return NSCAPI::returnUNKNOWN; }
#define MAP_OPTIONS_END() }

#define MAP_OPTIONS_MISSING_EX(opt, arg, str) \
		else { arg = str + opt.first; return NSCAPI::returnUNKNOWN; }

#define MAP_OPTIONS_SECONDARY_BEGIN(splt, arg) \
	else if (p__.first.find(splt) != std::string::npos) { \
	std::pair<std::string,std::string> arg = strEx::split(p__.first,splt); if (false) {}

#define MAP_OPTIONS_SECONDARY_END() }