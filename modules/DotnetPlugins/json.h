// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

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