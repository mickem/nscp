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

// Default Argument string (for consistency)
#define SHOW_ALL "ShowAll"
#define SHOW_FAIL "ShowFail"
#define NSCLIENT "nsclient"
#define IGNORE_PERFDATA "ignore-perf-data"
#define CHECK_ALL "CheckAll"
#define CHECK_ALL_OTHERS "CheckAllOthers"

#define CRASH_ARCHIVE_FOLDER_KEY	"crash-folder"
#define CRASH_ARCHIVE_FOLDER		"${crash-folder}"
#define CACHE_FOLDER_KEY		"cache-folder"
#define CACHE_FOLDER			"${cache-folder}"

#define NASTY_METACHARS         "|`&><'\"\\[]{}"        /* This may need to be modified for windows directory seperator */


