"""Reading a module's module.json, shared by the build's generators.

create_plugin_module.py turns one module.json into that module's dispatch glue;
create_command_module_map.py reads every built module's to tell the core which
module provides which command. Both go through here so they agree on what a
module.json says.
"""
import json
import os


def load_module_json(folder):
	# A module.json is UTF-8 - a description carrying an em dash or an ellipsis
	# is ordinary. Without the explicit encoding, `open()` reads in the
	# platform's ANSI codepage, which on a Windows builder is cp1252: an em
	# dash comes back as the three characters cp1252 makes of its UTF-8 bytes
	# and goes out re-encoded, so the doubled bytes are compiled into the
	# module's description and carried by everything downstream of it - the
	# registry, the REST API, `nscp test desc` and the extracted reference
	# docs. The Linux builder, where that default is already UTF-8, produced
	# the same file correctly, which is what made this look like a docs bug.
	with open(os.path.join(folder, 'module.json'), encoding='utf-8') as module_json:
		return json.loads(module_json.read())


def command_aliases(value):
	"""The aliases one `commands` entry registers besides its own name."""
	if type(value) is dict and 'alias' in value:
		if type(value['alias']) is list:
			return value['alias']
		return [value['alias']]
	return []


def command_names(data):
	"""Every command name a module registers from its module.json: each
	`commands` key except the `fallback` switch, and each of their aliases."""
	names = []
	for key, value in (data.get('commands') or {}).items():
		if key == 'fallback':
			continue
		names.append(key)
		names.extend(command_aliases(value))
	return names
