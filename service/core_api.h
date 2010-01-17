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

LPVOID NSAPILoader(wchar_t*buffer);
NSCAPI::errorReturn NSAPIGetApplicationName(wchar_t*buffer, unsigned int bufLen);
NSCAPI::errorReturn NSAPIGetBasePath(wchar_t*buffer, unsigned int bufLen);
NSCAPI::errorReturn NSAPIGetApplicationVersionStr(wchar_t*buffer, unsigned int bufLen);
NSCAPI::errorReturn NSAPIGetSettingsString(const wchar_t* section, const wchar_t* key, const wchar_t* defaultValue, wchar_t* buffer, unsigned int bufLen);
int NSAPIGetSettingsInt(const wchar_t* section, const wchar_t* key, int defaultValue);
void NSAPIMessage(int msgType, const wchar_t* file, const int line, const wchar_t* message);
void NSAPIStopServer(void);
NSCAPI::nagiosReturn NSAPIInject(const wchar_t* command, const unsigned int argLen, wchar_t **argument, wchar_t *returnMessageBuffer, unsigned int returnMessageBufferLen, wchar_t *returnPerfBuffer, unsigned int returnPerfBufferLen);
NSCAPI::errorReturn NSAPIGetSettingsSection(const wchar_t*, wchar_t***, unsigned int *);
NSCAPI::errorReturn NSAPIReleaseSettingsSectionBuffer(wchar_t*** aBuffer, unsigned int * bufLen);
NSCAPI::boolReturn NSAPICheckLogMessages(int messageType);
NSCAPI::errorReturn NSAPIEncrypt(unsigned int algorithm, const wchar_t* inBuffer, unsigned int inBufLen, wchar_t* outBuf, unsigned int *outBufLen);
NSCAPI::errorReturn NSAPIDecrypt(unsigned int algorithm, const wchar_t* inBuffer, unsigned int inBufLen, wchar_t* outBuf, unsigned int *outBufLen);
NSCAPI::errorReturn NSAPISetSettingsString(const wchar_t* section, const wchar_t* key, const wchar_t* value);
NSCAPI::errorReturn NSAPISetSettingsInt(const wchar_t* section, const wchar_t* key, int value);
NSCAPI::errorReturn NSAPIWriteSettings(int type);
NSCAPI::errorReturn NSAPIReadSettings(int type);
NSCAPI::errorReturn NSAPIRehash(int flag);
NSCAPI::errorReturn NSAPIDescribeCommand(const wchar_t*,wchar_t*,unsigned int);
NSCAPI::errorReturn NSAPIGetAllCommandNames(wchar_t***, unsigned int *);
NSCAPI::errorReturn NSAPIReleaseAllCommandNamessBuffer(wchar_t***, unsigned int *);
NSCAPI::errorReturn NSAPIRegisterCommand(unsigned int, const wchar_t*,const wchar_t*);
NSCAPI::errorReturn NSAPISettingsRegKey(const wchar_t*, const wchar_t*, int, const wchar_t*, const wchar_t*, const wchar_t*, int);
NSCAPI::errorReturn NSAPISettingsRegPath(const wchar_t*, const wchar_t*, const wchar_t*, int);
NSCAPI::errorReturn NSAPIGetPluginList(int*, NSCAPI::plugin_info*[]);
NSCAPI::errorReturn NSAPIReleasePluginList(int,NSCAPI::plugin_info*[]);
NSCAPI::errorReturn NSAPISettingsSave(void);
NSCAPI::errorReturn NSAPINotify(const wchar_t*, const wchar_t*, NSCAPI::nagiosReturn, const wchar_t*, const wchar_t*);
