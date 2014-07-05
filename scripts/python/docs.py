#!/usr/bin/env python
# -*- coding: utf-8 -*-
from NSCP import Settings, Registry, Core, log, log_debug, log_error, status
import plugin_pb2
from optparse import OptionParser
from sets import Set
import os
import csv
import traceback
import StringIO
import string
from jinja2 import Template, Environment
import hashlib
helper = None

module_template = u""".. default-domain:: nscp

.. module:: {{module.key}}
    :synopsis: {{module.info.description}}

{{(':module:`' + module.key + '` --- ' + module.info.title)|rst_heading('=', True)}}
{{module.info.description}}

{% if module.queries -%}
**Queries (Overview)**:

A list of all avalible queries (check commands)
{% set table = [] -%}
{% for key,query in module.queries|dictsort  -%}
	{% if query.info.description.startswith('Alternative name for:') -%}
		{% set command = query.info.description[22:] -%}
		{% do table.append([query.key|rst_link('query'), command|rst_link('query')]) -%}
	{%- elif query.info.description.startswith('Alias for:') -%}
		{% set command = query.info.description[11:] -%}
		{% do table.append([query.key|rst_link('query'), command|rst_link('query')]) -%}
	{%- else -%}
		{% do table.append([query.key|rst_link('query'), query.info.description|firstline]) -%}
	{%- endif %}
{%- endfor %}
{{table|rst_csvtable('Command', 'Description')}}
{%- endif %}

{% if module.aliases -%}
**Aliases (Overview)**:

A list of all short hand aliases for queries (check commands)

{% set table = [] %}
{% for key,query in module.aliases|dictsort  -%}
	{% if query.info.description.startswith('Alternative name for:') -%}
		{% set command = query.info.description[22:] -%}
		{% do table.append([query.key, command|rst_link('query')]) -%}
	{%- elif query.info.description.startswith('Alias for:') -%}
		{% set command = query.info.description[11:] -%}
		{% do table.append([query.key, command|rst_link('query')]) -%}
	{%- else -%}
		{% do table.append([query.key, query.info.description|firstline]) -%}
	{%- endif %}
{%- endfor %}
{{table|rst_csvtable('Command', 'Description')}}
{%- endif %}

**Commands (Overview)**: 

**TODO:** Add a list of all external commands (this is not check commands)

{% if module.paths -%}
**Configuration (Overview)**:

{% set table = [] -%}
{% set table_adv = [] -%}
{% set table_sam = [] -%}
{% for k,path in module.paths|dictsort  -%}
	{% set pkey = path.key|rst_link('confpath') -%}
	{% for k,key in path.keys|dictsort  -%}
		{% set kkey = ("~"+path.key+"."+k)|rst_link('confkey') -%}
		{% if key.info.sample -%}
			{% do table_sam.append([pkey, kkey, key.info.title|firstline]) -%}
		{%- elif key.info.advanced -%}
			{% do table_adv.append([pkey, kkey, key.info.title|firstline]) -%}
		{%- else -%}
			{% do table.append([pkey, kkey, key.info.title|firstline]) -%}
		{%- endif %}
	{%- endfor %}
{%- endfor %}
{% if table -%}
Common Keys:

{{table|rst_csvtable('Path / Section', 'Key', 'Description')}}
{%- endif %}
{% if table_adv -%}
Advanced keys:

{{table_adv|rst_csvtable('Path / Section', 'Key', 'Default Value', 'Description')}}
{%- endif %}
{% if table_sam -%}
Sample keys:

{{table_sam|rst_csvtable('Path / Section', 'Key', 'Default Value', 'Description')}}
{%- endif %}
{%- endif %}

{% if module.queries -%}

Queries
=======
A quick reference for all avalible queries (check commands) in the {{module.key}} module.

{% for k,query in module.queries|dictsort -%}

{{(':query:`'+query.key+'`')|rst_heading}}
.. query:: {{query.key}}
    :synopsis: {{query.info.description|firstline}}

**Usage:**

{% set table = [] %}
{% for help in query.help -%}
    {% do table.append([help.key|rst_link('option'), help.arg, help.desc|firstline]) %}
{%- endfor %}
{{table|rst_csvtable('Option', 'Default Value', 'Description')}}

Arguments
*********
{% for help in query.help -%}
.. option:: {{help.key}}
    :synopsis: {{help.desc|firstline}}

{%if help.ext -%}
{{help.ext.head|block_pad(4, '| ')}}

{{help.ext.data|rst_table|block_pad(4, '  ')}}

{{help.ext.tail|block_pad(4, '| ')}}

{% else -%}
{{help.desc|block_pad(4, '| ')}}
{%- endif %}
{{'\n'}}
{%- endfor %}
{% if query.sample %}
Samples
*******
{%if module.namespace %}
.. include:: ../../samples/{{query.sample}}
{% else %}
.. include:: ../samples/{{query.sample}}
{% endif %}
{%- endif %}
{%- endfor %}
{%- endif %}

{% for pkey,path in module.paths|dictsort %}
{% set common_heading=module.paths.keys()|common_head|length %}
{{("… " + pkey[common_heading:])|replace("/", " / ")|rst_heading}}
.. confpath:: {{pkey}}
    :synopsis: {{path.info.title|firstline}}

    **{{path.info.title}}**

{{path.info.description|block_pad(4, '| ')}}

{% set table = [] -%}
{% set pkey = path.key|rst_link('confpath') -%}
{% for k,key in path.keys|dictsort  -%}
    {% set kkey = k|rst_link('confkey') -%}
    {% do table.append([kkey, key.info.default_value|extract_value, key.info.title|firstline]) -%}
{%- endfor %}
{{table|rst_csvtable('Key', 'Default Value', 'Description')}}

**Sample**::

    # {{path.info.title}}
    # {{path.info.description|firstline}}
    [{{path.key}}]
{% for kkey,key in path.keys|dictsort %}    {{kkey}}={{key.info.default_value|extract_value}}
{% endfor %}
{% for kkey,key in path.keys|dictsort %}
.. confkey:: {{kkey}}
    :synopsis: {{key.info.title}}

    **{{key.info.title}}**

{{key.info.description|block_pad(4, '| ')}}

{% if key.info.advanced %}    **Advanced** (means it is not commonly used)

{% endif %}    **Path**: {{path.key}}

    **Key**: {{kkey}}

{% if key.info.default_value %}    **Default value**: {{key.info.default_value|extract_value}}

{% endif %}{% if key.info.sample %}    **Sample key**: This key is provided as a sample to show how to configure objects

{% endif %}    **Used by**: {% for m in path.info.plugin %}{% if not loop.first %},  {% endif %}:module:`{{m}}`{% endfor %}

    **Sample**::

        # {{key.info.title}}
        # {{key.info.description|firstline}}
        [{{path.key}}]
        {{kkey}}={{key.info.default_value|extract_value}}

{% endfor %}
{% endfor %}
"""

class root_container(object):
	paths = {}
	commands = {}
	aliases = {}
	plugins = {}
	windows_modules = ['CheckSystem', 'CheckDisk', 'NSClientServer', 'DotnetPlugins']
	def __init__(self):
		self.paths = {}
		self.commands = {}
		self.aliases = {}
		self.plugins = {}

	def append_key(self, info):
		path = info.node.path
		if path in self.paths:
			self.paths[path].append_key(info)
		else:
			p = path_container()
			p.append_key(info)
			self.paths[path] = p
		
	def append_path(self, info):
		path = info.node.path
		if not path in self.paths:
			self.paths[path] = path_container(info)

	def append_command(self, info):
		name = info.name
		if not name in self.commands:
			self.commands[name] = command_container(info)

	def append_alias(self, info):
		name = info.name
		if not name in self.commands:
			self.aliases[name] = command_container(info)

	def append_plugin(self, info):
		name = info.name
		namespace = ''
		if name in self.windows_modules:
			namespace = 'windows'
		if not name in self.plugins:
			self.plugins[name] = plugin_container(info, namespace)
			
	def get_hash(self):
		ret = {}
		ret['paths'] = self.paths
		ret['commands'] = self.commands
		ret['aliases'] = self.aliases
		ret['plugins'] = self.plugins
		return ret

class path_container(object):
	keys = {}
	info = None
	def __init__(self, info = None):
		self.keys = {}
		self.info = info.info
		
	def append_key(self, info):
		self.keys[info.node.key] = info

class command_container(object):
	info = None
	def __init__(self, info = None):
		self.info = info.info

class plugin_container(object):
	info = None
	namespace = ''
	def __init__(self, info = None, namespace = ''):
		self.info = info.info
		self.namespace = namespace

def first_line(string):
    return string.strip().split('\n')[0]

def make_rst_link(name, type, title = None):
	if title:
		return ':%s:`%s<%s>`'%(type, title, name)
	return ':%s:`%s`'%(type, name)

def largest_value(a,b):
	return map(lambda n: n[0] if len(n[0])>len(n[1]) else n[1], zip(a, b))

def extract_value(value):
	if value.HasField("string_data"):
		return value.string_data
	return '??? %s ???'%value

def block_pad(string, pad, prefix = ''):
	string = string.strip()
	if not string:
		return ''
	ret = ''
	for line in string.split('\n'):
		if line != '\n':
			ret += (pad * ' ') + prefix + line + '\n'
		else:
			ret += line + '\n'
	return ret.rstrip()

def render_rst_csv_table(table, *header):
	if not table:
		return ''
	ret = """.. csv-table:: 
    :class: contentstable 
    :delim: | 
"""
	ret += "    :header: " + ", ".join(map(lambda a: '"%s"'%a, header))
	ret += "\n\n"
	for line in table:
		ret += '    ' + ' | '.join(line) + '\n'
	return ret
 

def render_rst_table(table, *args):
	if not table:
		return ''
	if args:
		table.insert(0, args)
	ret = ''
	maxcol = map(lambda a:len(a)+1, reduce(lambda a,b: largest_value(a,b), table))
	divider = ''.join(map(lambda a:''.ljust(a,'=') + ' ', maxcol)) + '\n'
	for line in table:
		c = ''.join(map(lambda a:a[1].ljust(a[0],' ') + ' ', zip(maxcol, line))) + '\n'
		if not ret:
			c = c + divider
		ret = ret + c
	return divider + ret + divider
	
def render_rst_heading(string, char='-', top=False):
	if top:
		return "\n".rjust(len(string)+1, char) + string + "\n".ljust(len(string)+1, char)
	return string + "\n".ljust(len(string)+1, char)

def getcommonletters(strlist):
	return ''.join([x[0] for x in zip(*strlist) \
					 if reduce(lambda a,b:(a == b) and a or None,x)])

def calculate_common_head(strlist):
	strlist = strlist[:]
	prev = None
	while True:
		common = getcommonletters(strlist)
		if common == prev:
			break
		strlist.append(common)
		prev = common

	return getcommonletters(strlist)

def render_template(hash, template, filename):
	data = template.render(hash).encode('utf8')
	
	path = os.path.dirname(filename)
	if not os.path.exists(path):
		os.makedirs(path)

	if os.path.exists(filename):
		m1 = hashlib.sha256()
		m1.update(data)
		sha1 = m1.digest()
		with open(filename) as f:
			m2 = hashlib.sha256()
			m2.update(f.read())
			sha2 = m2.digest()
		if sha1 == sha2:
			log_debug("no changes detected in: %s"%filename)
			return

	log_debug('Writing file: %s'%filename)
	f = open(filename,"w")
	f.write(data)
	f.close()
	
class DocumentationHelper(object):
	plugin_id = None
	plugin_alias = None
	script_alias = None
	conf = None
	registry = None
	core = None
	dir = None
	command_cache = {}
	
	def __init__(self, plugin_id, plugin_alias, script_alias):
		self.plugin_id = plugin_id
		self.plugin_alias = plugin_alias
		self.script_alias = script_alias
		self.conf = Settings.get(self.plugin_id)
		self.registry = Registry.get(self.plugin_id)
		self.core = Core.get(self.plugin_id)
		self.command_cache = {}
		
	def build_inventory_request(self,  path = '/', recursive = True, keys = False):
		message = plugin_pb2.SettingsRequestMessage()
		message.header.version = plugin_pb2.Common.VERSION_1
		payload = message.payload.add()
		payload.plugin_id = self.plugin_id
		payload.inventory.node.path = path
		payload.inventory.recursive_fetch = recursive
		payload.inventory.fetch_keys = keys
		payload.inventory.fetch_paths = not keys
		payload.inventory.fetch_samples = True
		payload.inventory.descriptions = True
		return message.SerializeToString()
	
	def build_command_request(self, type = 1):
		message = plugin_pb2.RegistryRequestMessage()
		message.header.version = plugin_pb2.Common.VERSION_1
		payload = message.payload.add()
		payload.inventory.fetch_all = True
		payload.inventory.type.append(type)
		return message.SerializeToString()

	def get_paths(self):
		(code, data) = self.conf.query(self.build_inventory_request())
		if code == 1:
			message = plugin_pb2.SettingsResponseMessage()
			message.ParseFromString(data)
			for payload in message.payload:
				if payload.inventory:
					log_debug('Found %d paths'%len(payload.inventory))
					return payload.inventory
		return []

	def get_keys(self, path):
		(code, data) = self.conf.query(self.build_inventory_request(path, False, True))
		if code == 1:
			message = plugin_pb2.SettingsResponseMessage()
			message.ParseFromString(data)
			for payload in message.payload:
				if payload.inventory:
					log_debug('Found %d keys for %s'%(len(payload.inventory), path))
					return payload.inventory
		return []

	def get_queries(self):
		(code, data) = self.registry.query(self.build_command_request(1))
		if code == 1:
			message = plugin_pb2.RegistryResponseMessage()
			message.ParseFromString(data)
			for payload in message.payload:
				if payload.inventory:
					log_debug('Found %d commands'%len(payload.inventory))
					return payload.inventory
		log_error('No commands found')
		return []

	def get_query_aliases(self):
		(code, data) = self.registry.query(self.build_command_request(5))
		if code == 1:
			message = plugin_pb2.RegistryResponseMessage()
			message.ParseFromString(data)
			for payload in message.payload:
				if payload.inventory:
					log_debug('Found %d commands'%len(payload.inventory))
					return payload.inventory
		log_error('No commands found')
		return []

	def get_plugins(self):
		(code, data) = self.registry.query(self.build_command_request(4))
		if code == 1:
			message = plugin_pb2.RegistryResponseMessage()
			message.ParseFromString(data)
			for payload in message.payload:
				if payload.inventory:
					log_debug('Found %d plugins'%len(payload.inventory))
					return payload.inventory
		log_error('No plugins')
		return []

	def get_info(self):
		root = root_container()
		for p in self.get_paths():
			root.append_path(p)
			for k in self.get_keys(p.node.path):
				root.append_key(k)
		for p in self.get_queries():
			root.append_command(p)
		for p in self.get_query_aliases():
			root.append_alias(p)
		for p in self.get_plugins():
			root.append_plugin(p)
		return root
		
	def fetch_command(self, command, cinfo):
		cinfo.help = []
		cinfo.sample = ''
		if command in self.command_cache:
			return self.command_cache[command]
		try:
			log_debug("Fetching info from: %s"%command)
			(ret, msg, perf) = self.core.simple_query(command.encode('ascii', 'ignore'), ['help-csv'])
			if not ret == 0:
				log_error("WARNING: Ignoring command: %s as it returned %d"%(command, ret))
				return None
			reader = csv.reader(StringIO.StringIO(msg), delimiter=',')
			for row in reader:
				if len(row) <= 3:
					log_error("WARNING: Ignoring invalid argument: %s"%(row))
					continue
				hash = {}
				hash['key'] = row[0]
				hash['arg'] = 'N/A' if row[1] == "false" or row[2] == 'arg' else row[2]
				hash['desc'] = row[3].replace('\\n', '\n').replace('\\t', '\t')
				hash['sample'] = ''
				
				spos = hash['desc'].find('\n\n')
				extdata = {}
				if spos != -1:
					desc = hash['desc']
					epos = desc.find('\n\n', spos+2)
					if epos != -1:
						pos = desc.find('\t', spos+2, epos)
						if pos != -1:
							extdata['head'] = desc[:spos]
							extdata['tail'] = desc[epos+2:]
							data = desc[spos+2:epos]
							rows = data.split('\n')
							tbl = []
							for r in rows:
								tbl.append(r.split('\t'))
							extdata['data'] = tbl
				hash['ext'] = extdata
				cinfo.help.append(hash)
			self.command_cache[command] = cinfo
			return cinfo
		except Exception as e:
			log_error('ERROR: failed to process %s %s'%(command, e))
			return None

	def generate_rst(self, input_dir, output_dir):
		root = self.get_info()
		i = 0
		for (module,minfo) in root.plugins.iteritems():
			out_base_path = '%s/reference/'%output_dir
			in_base_path = '%s/reference/'%input_dir
			if minfo.namespace:
				out_base_path = '%s/reference/%s/'%(output_dir, minfo.namespace)
				in_base_path = '%s/reference/%s/'%(input_dir, minfo.namespace)
			hash = root.get_hash()
			minfo.key = module
			minfo.queries = {}
			for (c,cinfo) in sorted(root.commands.iteritems()):
				if module in cinfo.info.plugin:
					more_info = self.fetch_command(c,cinfo)
					if more_info:
						cinfo = more_info
					sfile = '%s/%s_%s_samples.inc'%(in_base_path, module, c)
					if os.path.exists(sfile):
						cinfo.sample = os.path.basename(sfile)
						#all_samples.append((module, command, sfile))
					cinfo.key = c
					minfo.queries[c] = cinfo
			minfo.aliases = {}
			for (c,cinfo) in sorted(root.aliases.iteritems()):
				if module in cinfo.info.plugin:
					cinfo.key = c
					minfo.aliases[c] = cinfo
					
			minfo.paths = {}
			for (c,cinfo) in sorted(root.paths.iteritems()):
				if module in cinfo.info.plugin:
					cinfo.key = c
					minfo.paths[c] = cinfo

			hash['module'] = minfo
			i=i+1
			log_debug('Processing module: %d of %d [%s]'%(i, len(root.plugins), module))

			env = Environment(extensions=["jinja2.ext.do",])
			env.filters['firstline'] = first_line
			env.filters['rst_link'] = make_rst_link
			env.filters['rst_table'] = render_rst_table
			env.filters['rst_csvtable'] = render_rst_csv_table
			env.filters['rst_heading'] = render_rst_heading
			env.filters['extract_value'] = extract_value
			env.filters['block_pad'] = block_pad
			env.filters['common_head'] = calculate_common_head
			
			template = env.from_string(module_template)
			render_template(hash, template, '%s/%s.rst'%(out_base_path, module))

	def main(self, args):
		parser = OptionParser(prog="")
		parser.add_option("-f", "--format", help="Generate format")
		parser.add_option("-o", "--output", help="write report to FILE(s)")
		parser.add_option("-i", "--input", help="Reference folder")
		(options, args) = parser.parse_args(args=args)

		if not options.format:
			options.format = "rst"
		if options.format in ["rst"]:
			self.generate_rst(options.input, options.output)
		else:
			log("Help%s"%parser.print_help())
			log("Invalid format: %s"%options.format)
			return

def __main__(args):
	global helper
	helper.main(args);
	
def init(plugin_id, plugin_alias, script_alias):
	global helper
	helper = DocumentationHelper(plugin_id, plugin_alias, script_alias)

def shutdown():
	global helper
	helper = None
