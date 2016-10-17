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

#ifndef JSON_H
#define JSON_H

#include "block_allocator.h"

enum json_type {
	JSON_NULL,
	JSON_OBJECT,
	JSON_ARRAY,
	JSON_STRING,
	JSON_INT,
	JSON_FLOAT,
	JSON_BOOL,
};

struct json_value {
	json_value *parent;
	json_value *next_sibling;
	json_value *first_child;
	json_value *last_child;

	char *name;
	union {
		char *string_value;
		int int_value;
		float float_value;
	};

	json_type type;
};

json_value *json_parse(char *source, char **error_pos, char **error_desc, int *error_line, block_allocator *allocator);

#endif