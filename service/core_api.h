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

#pragma once

#include <NSCAPI.h>

//////////////////////////////////////////////////////////////////////////
// Various NSAPI callback functions (available for plug-ins to make calls back to the core.
// <b>NOTICE</b> No threading is allowed so technically every thread is responsible for marshaling things back.
// Though I think this is not the case at the moment.
//

nscapi::core_api::FUNPTR NSAPILoader(const char *buffer);
NSCAPI::errorReturn NSAPIGetApplicationName(char *buffer, unsigned int bufLen);
NSCAPI::errorReturn NSAPIGetApplicationVersionStr(char *buffer, unsigned int bufLen);
void NSAPIMessage(const char* data, unsigned int count);
void NSAPISimpleMessage(const char* module, int loglevel, const char* file, int line, const char* message);
NSCAPI::nagiosReturn NSAPIInject(const char *request_buffer, const unsigned int request_buffer_len, char **response_buffer, unsigned int *response_buffer_len);
NSCAPI::nagiosReturn NSAPIExecCommand(const char* target, const char *request_buffer, const unsigned int request_buffer_len, char **response_buffer, unsigned int *response_buffer_len);
NSCAPI::boolReturn NSAPICheckLogMessages(int messageType);
NSCAPI::errorReturn NSAPINotify(const char* channel, const char* buffer, unsigned int buffer_len, char ** result_buffer, unsigned int *result_buffer_len);
void NSAPIDestroyBuffer(char**);
NSCAPI::errorReturn NSAPIExpandPath(const char*, char*, unsigned int);
NSCAPI::errorReturn NSAPIReload(const char *);
NSCAPI::log_level::level NSAPIGetLoglevel();
NSCAPI::errorReturn NSAPISettingsQuery(const char *request_buffer, const unsigned int request_buffer_len, char **response_buffer, unsigned int *response_buffer_len);
NSCAPI::errorReturn NSAPIRegistryQuery(const char *request_buffer, const unsigned int request_buffer_len, char **response_buffer, unsigned int *response_buffer_len);
NSCAPI::errorReturn NSCAPIJson2Protobuf(const char* request_buffer, unsigned int request_buffer_len, char ** response_buffer, unsigned int *response_buffer_len);
NSCAPI::errorReturn NSCAPIProtobuf2Json(const char* object, const char* request_buffer, unsigned int request_buffer_len, char ** response_buffer, unsigned int *response_buffer_len);
NSCAPI::errorReturn NSCAPIEmitEvent(const char *request_buffer, const unsigned int request_buffer_len);
NSCAPI::errorReturn NSCAPIStorageQuery(const char *request_buffer, const unsigned int request_buffer_len, char **response_buffer, unsigned int *response_buffer_len);
