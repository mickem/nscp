#!/usr/bin/env python
# -*- coding: utf-8 -*-
from NSCP import Settings, Registry, Core, log, log_debug, log_error, status
import plugin_pb2
from optparse import OptionParser
from sets import Set
import os
import traceback
#import string
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

A list of all available queries (check commands)
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
		{% do table.append([query.key, "Alias for: " + command|rst_link('query')]) -%}
	{%- elif query.info.description.startswith('Alias for:') -%}
		{% set command = query.info.description[11:] -%}
		{% do table.append([query.key, "Alias for: " + command|rst_link('query')]) -%}
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

{% if module.sample %}
Samples
-------
{%if module.namespace %}
.. include:: ../../samples/{{module.sample}}

{% else %}
.. include:: ../samples/{{module.sample}}

{% endif %}
{%- endif %}
{% if module.queries -%}

Queries
=======
A quick reference for all available queries (check commands) in the {{module.key}} module.

{% for k,query in module.queries|dictsort -%}

{{(':query:`'+query.key+'`')|rst_heading}}
.. query:: {{query.key}}
    :synopsis: {{query.info.description|firstline}}

**Usage:**

{% set table = [] %}
{% for help in query.params -%}{% if help.content_type == 4 -%}
    {% do table.append([help.name|rst_link('option'),'N/A', help.long_description|firstline]) %}{% else -%}
    {% do table.append([help.name|rst_link('option'),help.default_value, help.long_description|firstline]) %}{%- endif %}
{%- endfor %}
{{table|rst_csvtable('Option', 'Default Value', 'Description')}}

{% if query.sample %}
Samples
*******
{%if module.namespace %}
.. include:: ../../samples/{{query.sample}}

{% else %}
.. include:: ../samples/{{query.sample}}

{% endif %}
{%- endif %}

Arguments
*********
{% for help in query.params -%}
.. option:: {{help.name}}
    :synopsis: {{help.long_description|firstline}}

{%if help.ext -%}
{{help.ext.head|block_pad(4, '| ')}}

{{help.ext.data|rst_table|block_pad(4, '')}}

{{help.ext.tail|block_pad(4, '| ')}}

{% else -%}
{{help.long_description|block_pad(4, '| ')}}
{%- endif %}
{{'\n'}}
{%- endfor %}
{%- endfor %}
{%- endif %}

{% for pkey,path in module.paths|dictsort %}
{% set common_heading=module.paths.keys()|common_head|length %}
{% if common_heading != pkey|length -%}
{{("… " + pkey[common_heading:])|replace("/", " / ")|rst_heading}}
{%- else -%}
{{pkey|replace("/", "/ ")|rst_heading}}
{%- endif %}

.. confpath:: {{pkey}}
    :synopsis: {{path.info.title|firstline}}

{% if path.info.title -%}
    **{{path.info.title}}**
{%- endif %}

{{path.info.description|block_pad(4, '| ')}}

{% set tbl = [] -%}
{% set pkey = path.key|rst_link('confpath') -%}
{% for k,key in path.keys|dictsort  -%}
    {% set kkey = k|rst_link('confkey') -%}
    {% do tbl.append([kkey, key.info.default_value|extract_value, key.info.title|firstline]) -%}
{%- endfor %}
{{tbl|rst_csvtable('Key', 'Default Value', 'Description')|block_pad(4)}}

    **Sample**::

        # {{path.info.title}}
        # {{path.info.description|firstline}}
        [{{path.key}}]
{% for kkey,key in path.keys|dictsort %}        {{kkey}}={{key.info.default_value|extract_value}}
{% endfor %}
{% for kkey,key in path.keys|dictsort %}
    .. confkey:: {{kkey}}
        :synopsis: {{key.info.title|as_text}}

        **{{key.info.title|as_text}}**

{%if key.ext -%}
{{key.ext.head|block_pad(8, '| ')}}

{{key.ext.data|rst_table|block_pad(8, '  ')}}

{{key.ext.tail|block_pad(8, '| ')}}

{% else -%}
{{key.info.description|as_text|block_pad(8, '| ')}}
{%- endif %}

{% if key.info.advanced %}        **Advanced** (means it is not commonly used)

{% endif %}        **Path**: {{path.key}}

        **Key**: {{kkey}}

{% if key.info.default_value %}        **Default value**: {{key.info.default_value|extract_value}}

{% endif %}{% if key.info.sample %}        **Sample key**: This key is provided as a sample to show how to configure objects

{% endif %}        **Used by**: {% for m in path.info.plugin|sort %}{% if not loop.first %},  {% endif %}:module:`{{m}}`{% endfor %}

        **Sample**::

            [{{path.key}}]
            # {{key.info.title}}
            {{kkey}}={{key.info.default_value|extract_value}}

{% endfor %}
{% endfor %}
"""


samples_template = u""".. default-domain:: nscp

.. default-domain:: nscp

===========
All samples
===========

A collection of all sample commands

{% for mk,module in plugins|dictsort  -%}
{% set vars = {'found': False} -%}
{% for qk,query in module.queries|dictsort -%}
{% if query.sample -%}
{% if vars.update({'found': True}) -%}{%- endif %}
{%- endif %}
{%- endfor %}
{% if vars.found -%}
{{mk|rst_heading('=')}}

{% for qk,query in module.queries|dictsort -%}
{% if query.sample -%}
{{qk|rst_heading}}

{{query.info.description|firstline}}

.. include:: ../samples/{{query.sample}}

{% endif %}
{%- endfor %}
{%- endif %}
{%- endfor %}
"""

def split_argllist(name, desc):
	extdata = {}
	spos = desc.find('\n\n')
	if spos != -1:
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
	return extdata

class root_container(object):
	paths = {}
	commands = {}
	aliases = {}
	plugins = {}
	windows_modules = ['CheckSystem', 'CheckDisk', 'NSClientServer', 'DotnetPlugins', 'CheckEventLog',  'CheckTaskSched',  'CheckWMI']
	unix_modules = ['CheckSystemUnix']
	check_modules = ['CheckExternalScripts',  'CheckHelpers',  'CheckLogFile',  'CheckMKClient',  'CheckMKServer',  'CheckNSCP']
	client_modules = ['GraphiteClient',  'NRDPClient',  'NRPEClient',  'NRPEServer',  'NSCAClient',  'NSCAServer',  'NSClientServer',  'SMTPClient',  'SyslogClient']
	generic_modules = ['CommandClient',  'DotnetPlugins',  'LUAScript',  'PythonScript',  'Scheduler',  'SimpleCache',  'SimpleFileWriter', 'WEBServer']

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
		elif name in self.unix_modules:
			namespace = 'unix'
		elif name in self.check_modules:
			namespace = 'check'
		elif name in self.client_modules:
			namespace = 'client'
		elif name in self.generic_modules:
			namespace = 'generic'
		else:
			namespace = 'misc'
		
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
		extdata = split_argllist(info.node.key, info.info.description)
		if extdata:
			ninfo = {}
			ninfo['info'] = info.info
			ninfo['node'] = info.node
			ninfo['ext'] = extdata
			self.keys[info.node.key] = ninfo
		else:
			self.keys[info.node.key] = info

class command_container(object):
	info = None
	parameters = None
	def __init__(self, info = None):
		self.info = info.info
		self.parameters = info.parameters

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

def as_text(value):
	value = value.replace('\\', '\\\\')
	value = value.replace('`', '\\`')
	value = value.replace('|', '\\|')
	return value

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
			c = c + ''.join(map(lambda a:''.ljust(a,'-') + ' ', maxcol)) + '\n'
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
	f = open(filename,"wb")
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
		log_debug('Fetching queries...')
		(code, data) = self.registry.query(self.build_command_request(1))
		if code == 1:
			message = plugin_pb2.RegistryResponseMessage()
			message.ParseFromString(data)
			for payload in message.payload:
				if payload.inventory:
					log_debug('Found %d queries'%len(payload.inventory))
					return payload.inventory
		log_error('No queries found')
		return []

	def get_query_aliases(self):
		log_debug('Fetching aliases...')
		(code, data) = self.registry.query(self.build_command_request(5))
		if code == 1:
			message = plugin_pb2.RegistryResponseMessage()
			message.ParseFromString(data)
			for payload in message.payload:
				if payload.inventory:
					log_debug('Found %d aliases'%len(payload.inventory))
					return payload.inventory
		log_error('No aliases found')
		return []

	def get_plugins(self):
		log_debug('Fetching plugins...')
		(code, data) = self.registry.query(self.build_command_request(7))
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
		for p in self.get_plugins():
			root.append_plugin(p)
		for p in self.get_paths():
			root.append_path(p)
			for k in self.get_keys(p.node.path):
				root.append_key(k)
		for p in self.get_queries():
			root.append_command(p)
		for p in self.get_query_aliases():
			root.append_alias(p)
		return root
		

	def fetch_command(self, command, cinfo):
		if command in self.command_cache:
			return self.command_cache[command]
		params = []
		for p in cinfo.parameters.parameter:
			extdata = split_argllist(p.name, p.long_description)
			if extdata:
				np = {}
				np['long_description'] = p.long_description
				np['default_value'] = p.default_value
				np['name'] = p.name
				np['ext'] = extdata
				np['content_type'] = p.content_type
				params.append(np)
			else:
				params.append(p)
		cinfo.params = params
			
		self.command_cache[command] = cinfo
		return cinfo

	def generate_rst(self, input_dir, output_dir):
		root = self.get_info()
		i = 0
		
		env = Environment(extensions=["jinja2.ext.do",])
		env.filters['firstline'] = first_line
		env.filters['rst_link'] = make_rst_link
		env.filters['rst_table'] = render_rst_table
		env.filters['rst_csvtable'] = render_rst_csv_table
		env.filters['rst_heading'] = render_rst_heading
		env.filters['extract_value'] = extract_value
		env.filters['block_pad'] = block_pad
		env.filters['common_head'] = calculate_common_head
		env.filters['as_text'] = as_text
		
		for (module,minfo) in root.plugins.iteritems():
			out_base_path = '%s/reference/'%output_dir
			sample_base_path = '%s/samples/'%output_dir
			if minfo.namespace:
				out_base_path = '%s/reference/%s/'%(output_dir, minfo.namespace)
			hash = root.get_hash()
			minfo.key = module
			minfo.queries = {}
			sfile = '%s%s_samples.inc'%(sample_base_path, module)
			if os.path.exists(sfile):
				print "---->%s" %sfile
				minfo.sample = os.path.basename(sfile)
			sfile = '%s%s_samples.rst'%(sample_base_path, module)
			if os.path.exists(sfile):
				print "---->%s" %sfile
				minfo.sample = os.path.basename(sfile)

			for (c,cinfo) in sorted(root.commands.iteritems()):
				if module in cinfo.info.plugin:
					more_info = self.fetch_command(c,cinfo)
					if more_info:
						cinfo = more_info
					sfile = '%s%s_%s_samples.inc'%(sample_base_path, module, c)
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

			template = env.from_string(module_template)
			render_template(hash, template, '%s/%s.rst'%(out_base_path, module))

		log_debug('%s/samples/index.rst'%output_dir)
		hash = root.get_hash()
		template = env.from_string(samples_template)
		render_template(hash, template, '%s/samples/index.rst'%output_dir)

	def main(self, args):
		parser = OptionParser(prog="")
		parser.add_option("-o", "--output", help="write report to FILE(s)")
		parser.add_option("-i", "--input", help="Reference folder")
		(options, args) = parser.parse_args(args=args)
		self.generate_rst(options.input, options.output)

def __main__(args):
	global helper
	helper.main(args);
	return 0
	
def init(plugin_id, plugin_alias, script_alias):
	global helper
	helper = DocumentationHelper(plugin_id, plugin_alias, script_alias)

def shutdown():
	global helper
	helper = None
