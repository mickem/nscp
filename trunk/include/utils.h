#pragma once

#include <string>
#include <strEx.h>

void generate_crc32_table(void);
unsigned long calculate_crc32(const char *buffer, int buffer_size);


#define MAP_OPTIONS_BEGIN(args) \
	for (std::list<std::string>::const_iterator cit__=args.begin();cit__!=args.end();++cit__) { \
	std::pair<std::string,std::string> p__ = strEx::split(*cit__,"="); if (false) {}

#define MAP_OPTIONS_SHOWALL(obj) \
			else if (p__.first == SHOW_ALL) { if (p__.second == "long") obj.show = checkHolders::showLong; else obj.show = checkHolders::showShort; } \
			else if (p__.first == SHOW_FAIL) { obj.show = checkHolders::showProblems; }

#define MAP_OPTIONS_DISK_ALL(obj, postfix, pfUpper, pfLower) \
			else if (p__.first == "MaxWarn" pfUpper) { obj.warn.max.upper = p__.second; } \
			else if (p__.first == "MaxCrit" pfUpper) { obj.crit.max.upper = p__.second; } \
			else if (p__.first == "MinWarn" pfUpper) { obj.warn.min.upper = p__.second; } \
			else if (p__.first == "MinCrit" pfUpper) { obj.crit.min.upper = p__.second; } \
			else if (p__.first == "MaxWarn" pfLower) { obj.warn.max.lower = p__.second; } \
			else if (p__.first == "MaxCrit" pfLower) { obj.crit.max.lower = p__.second; } \
			else if (p__.first == "MinWarn" pfLower) { obj.warn.min.lower = p__.second; } \
			else if (p__.first == "MinCrit" pfLower) { obj.crit.min.lower = p__.second; } \
			else if (p__.first == "MaxWarn" postfix) { obj.warn.max.lower = p__.second; } \
			else if (p__.first == "MaxCrit" postfix) { obj.crit.max.lower = p__.second; } \
			else if (p__.first == "MinWarn" postfix) { obj.warn.min.upper = p__.second; } \
			else if (p__.first == "MinCrit" postfix) { obj.crit.min.upper = p__.second; }

#define MAP_OPTIONS_NUMERIC_ALL(obj, postfix) \
			else if (p__.first == ("MaxWarn" postfix)) { obj.warn.max = p__.second; } \
			else if (p__.first == ("MaxCrit" postfix)) { obj.crit.max = p__.second; } \
			else if (p__.first == ("MinWarn" postfix)) { obj.warn.min = p__.second; } \
			else if (p__.first == ("MinCrit" postfix)) { obj.crit.min = p__.second; }

#define MAP_OPTIONS_PUSH_WTYPE(type, value, obj, list) \
			else if (p__.first == value) { type o; o.obj = p__.second; list.push_back(o); }
#define MAP_OPTIONS_PUSH(value, list) \
			else if (p__.first == value) { list.push_back(p__.second); }
#define MAP_OPTIONS_DO(action) \
			else if (p__.first == value) { action; }
#define MAP_OPTIONS_STR(value, obj) \
			else if (p__.first == value) { obj = p__.second; }
#define MAP_OPTIONS_STR2INT(value, obj) \
			else if (p__.first == value) { obj = atoi(p__.second.c_str()); }
#define MAP_OPTIONS_STR_AND(value, obj, extra) \
			else if (p__.first == value) { obj = p__.second; extra;}
#define MAP_OPTIONS_BOOL_TRUE(value, obj) \
			else if (p__.first == value) { obj = true; }
#define MAP_OPTIONS_BOOL_FALSE(value, obj) \
			else if (p__.first == value) { obj = false; }
#define MAP_OPTIONS_BOOL_VALUE(value, obj, tStr) \
			else if ((p__.first == value)&&(p__.second == tStr)) { obj = true; } 
#define MAP_OPTIONS_MODE(value, tStr, obj, oVal) \
			else if ((p__.first == value)&&(p__.second == tStr)) { obj = oVal; } 
#define MAP_OPTIONS_BOOL_EX(value, obj, tStr, fStr) \
			else if ((p__.first == value)&&(p__.second == tStr)) { obj = true; } \
			else if ((p__.first == value)&&(p__.second == fStr)) { obj = false; }
#define MAP_OPTIONS_MISSING(arg, str) \
			else { arg = str + p__.first; return NSCAPI::returnUNKNOWN; }
#define MAP_OPTIONS_FALLBACK(obj) \
			else { obj = p__.first;}
#define MAP_OPTIONS_FALLBACK_AND(obj, extra) \
			else { obj = p__.first; extra;}
#define MAP_OPTIONS_END() }

#define MAP_OPTIONS_MISSING_EX(opt, arg, str) \
		else { arg = str + opt.first; return NSCAPI::returnUNKNOWN; }

#define MAP_OPTIONS_SECONDARY_BEGIN(splt, arg) \
	else if (p__.first.find(splt) != std::string::npos) { \
	std::pair<std::string,std::string> arg = strEx::split(p__.first,splt); if (false) {}

#define MAP_OPTIONS_SECONDARY_END() }