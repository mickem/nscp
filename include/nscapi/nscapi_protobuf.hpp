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

#include <nscapi/dll_defines_protobuf.hpp>

#ifdef WIN32
#pragma warning(push)
#pragma warning(disable:4913)
#pragma warning(disable:4512)
#pragma warning(disable:4244)
#pragma warning(disable:4127)
#pragma warning(disable:4251)
#pragma warning(disable:4275)
#include <protobuf/plugin.pb.h>
#pragma warning(pop)
#else
#include <protobuf/plugin.pb.h>
#endif
