/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
#ifdef HAVE_JSON_SPIRIT
NSCAPI::errorReturn NSCAPIJson2Protobuf(const char* request_buffer, unsigned int request_buffer_len, char ** response_buffer, unsigned int *response_buffer_len);
NSCAPI::errorReturn NSCAPIProtobuf2Json(const char* object, const char* request_buffer, unsigned int request_buffer_len, char ** response_buffer, unsigned int *response_buffer_len);
#endif