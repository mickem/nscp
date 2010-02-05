/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#pragma once


#include <unicode_char.hpp>

namespace NSCAPI {

#ifdef DEBUG
	typedef enum {
		returnCRIT = 2,
		returnOK = 0,
		returnWARN = 1,
		returnUNKNOWN = 3,
		returnInvalidBufferLen = -2,
		returnIgnored = -1
	} nagiosReturn;
	typedef enum {
		istrue = 1, 
		isfalse = 0
	} boolReturn;
	typedef enum {
		isSuccess = 1, 
		hasFailed = 0,
		isInvalidBufferLen = -2
	} errorReturn;
	typedef enum {
		key_string = 100,
		key_integer = 200,
		key_bool = 300,
	} settings_type;

	typedef enum {
		normalStart = 0,
		dontStart = 1,
	} moduleLoadMode;
#else
	const int normalStart = 0;
	const int dontStart = 1;
	const int returnOK = 0;
	const int returnWARN = 1;
	const int returnCRIT = 2;
	const int returnUNKNOWN = 3;
	const int returnInvalidBufferLen = -2;
	const int returnIgnored = -1;
	const int istrue = 1; 
	const int isfalse = 0;
	const int isSuccess = 1; 
	const int hasFailed = 0;
	const int isInvalidBufferLen = -2;
	const int key_string = 100;
	const int key_integer = 200;
	const int key_bool = 300;

	typedef int nagiosReturn;
	typedef int boolReturn;
	typedef int errorReturn;
	typedef int settings_type;
	typedef int moduleLoadMode;
#endif

	const int encryption_xor = 1;

	// Settings types
	const int settings_default = 0;
	const int settings_registry = 1;
	const int settings_inifile = 2;

	// Various message Types
	const int log = 1;				// Log message
	const int error = -1;			// Error (non critical)
	const int critical = -10;		// Critical error
	const int warning = 2;			// Warning
	const int debug = 10;			// Debug message

	typedef int messageTypes;		// Message type

	struct plugin_info {
		wchar_t *dll;
		wchar_t *name;
		wchar_t *description;
		wchar_t *version;
	};
	typedef plugin_info* plugin_info_list;


};

namespace NSCModuleHelper {
	typedef void* (*lpNSAPILoader)(wchar_t*);
}