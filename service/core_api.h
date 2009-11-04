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

#include <NSCAPI.h>


//////////////////////////////////////////////////////////////////////////
// Various NSAPI callback functions (available for plug-ins to make calls back to the core.
// <b>NOTICE</b> No threading is allowed so technically every thread is responsible for marshaling things back. 
// Though I think this is not the case at the moment.
//

LPVOID NSAPILoader(TCHAR*buffer);
NSCAPI::errorReturn NSAPIGetApplicationName(TCHAR*buffer, unsigned int bufLen);
NSCAPI::errorReturn NSAPIGetBasePath(TCHAR*buffer, unsigned int bufLen);
NSCAPI::errorReturn NSAPIGetApplicationVersionStr(TCHAR*buffer, unsigned int bufLen);
NSCAPI::errorReturn NSAPIGetSettingsString(const TCHAR* section, const TCHAR* key, const TCHAR* defaultValue, TCHAR* buffer, unsigned int bufLen);
int NSAPIGetSettingsInt(const TCHAR* section, const TCHAR* key, int defaultValue);
void NSAPIMessage(int msgType, const TCHAR* file, const int line, const TCHAR* message);
void NSAPIStopServer(void);
NSCAPI::nagiosReturn NSAPIInject(const TCHAR* command, const unsigned int argLen, TCHAR **argument, TCHAR *returnMessageBuffer, unsigned int returnMessageBufferLen, TCHAR *returnPerfBuffer, unsigned int returnPerfBufferLen);
NSCAPI::errorReturn NSAPIGetSettingsSection(const TCHAR*, TCHAR***, unsigned int *);
NSCAPI::errorReturn NSAPIReleaseSettingsSectionBuffer(TCHAR*** aBuffer, unsigned int * bufLen);
NSCAPI::boolReturn NSAPICheckLogMessages(int messageType);
NSCAPI::errorReturn NSAPIEncrypt(unsigned int algorithm, const TCHAR* inBuffer, unsigned int inBufLen, TCHAR* outBuf, unsigned int *outBufLen);
NSCAPI::errorReturn NSAPIDecrypt(unsigned int algorithm, const TCHAR* inBuffer, unsigned int inBufLen, TCHAR* outBuf, unsigned int *outBufLen);
NSCAPI::errorReturn NSAPISetSettingsString(const TCHAR* section, const TCHAR* key, const TCHAR* value);
NSCAPI::errorReturn NSAPISetSettingsInt(const TCHAR* section, const TCHAR* key, int value);
NSCAPI::errorReturn NSAPIWriteSettings(int type);
NSCAPI::errorReturn NSAPIReadSettings(int type);
NSCAPI::errorReturn NSAPIRehash(int flag);
NSCAPI::errorReturn NSAPIDescribeCommand(const TCHAR*,TCHAR*,unsigned int);
NSCAPI::errorReturn NSAPIGetAllCommandNames(TCHAR***, unsigned int *);
NSCAPI::errorReturn NSAPIReleaseAllCommandNamessBuffer(TCHAR***, unsigned int *);
NSCAPI::errorReturn NSAPIRegisterCommand(const TCHAR*,const TCHAR*);
NSCAPI::errorReturn NSAPISettingsRegKey(const TCHAR*, const TCHAR*, int, const TCHAR*, const TCHAR*, const TCHAR*, int);
NSCAPI::errorReturn NSAPISettingsRegPath(const TCHAR*, const TCHAR*, const TCHAR*, int);
NSCAPI::errorReturn NSAPIGetPluginList(int*, NSCAPI::plugin_info*[]);
NSCAPI::errorReturn NSAPIReleasePluginList(int,NSCAPI::plugin_info*[]);
NSCAPI::errorReturn NSAPISettingsSave(void);
