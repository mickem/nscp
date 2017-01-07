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

module_template = u"""# {{module.key}}

{{module.info.description}}

{%if module.ext_desc -%}
{{ module.ext_desc}}
{%- endif %}

{% if module.queries -%}

## List of commands

A list of all available queries (check commands)
{% set table = [] -%}
{% for key,query in module.queries|dictsort  -%}
    {% if query.info.description.startswith('Alternative name for:') -%}
        {% set command = query.info.description[22:] -%}
        {% do table.append([query.key|md_self_link, command|rst_link('query')]) -%}
    {%- elif query.info.description.startswith('Alias for:') -%}
        {% set command = query.info.description[11:] -%}
        {% do table.append([query.key|md_self_link, command|rst_link('query')]) -%}
    {%- else -%}
        {% do table.append([query.key|md_self_link, query.info.description|firstline]) -%}
    {%- endif %}
{%- endfor %}
{{table|rst_table('Command', 'Description')}}
{%- endif %}

{% if module.aliases -%}
## List of command aliases

A list of all short hand aliases for queries (check commands)

{% set table = [] -%}
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
{{table|rst_table('Command', 'Description')}}
{%- endif %}

{% if module.paths -%}
## List of Configuration

{% set table = [] -%}
{% set table_adv = [] -%}
{% set table_sam = [] -%}
{% for k,path in module.paths|dictsort  -%}
    {% set pkey = path.key|md_self_link -%}
    {% for k,key in path.keys|dictsort  -%}
        {% set kkey = k|md_prefix_lnk(path.key)|md_self_link(k) -%}
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
### Common Keys

{{table|rst_table('Path / Section', 'Key', 'Description')}}
{%- endif %}
{% if table_adv -%}
### Advanced keys

{{table_adv|rst_table('Path / Section', 'Key', 'Description')}}
{%- endif %}
{% if table_sam -%}
### Sample keys

{{table_sam|rst_table('Path / Section', 'Key', 'Description')}}
{%- endif %}
{%- endif %}

{% if module.sample %}
# Usage

_To edit the usage section please edit [this page](https://github.com/mickem/nscp-docs/blob/master/{{module.sample_source}})_

{{module.sample}}

{%- endif %}
{% if module.queries -%}

# Queries

A quick reference for all available queries (check commands) in the {{module.key}} module.

{% for k,query in module.queries|dictsort -%}

## {{query.key}}

{{query.info.description}}

{% if query.sample -%}
### Usage

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/{{query.sample_source}})_

{{query.sample}}
{%- endif %}
### Usage

{% set table = [] -%}
{% for help in query.params -%}{% if help.content_type == 4 -%}
    {% do table.append([help.name|md_prefix_lnk(query.key)|md_self_link(help.name),'N/A', help.long_description|firstline]) %}{% else -%}
    {% do table.append([help.name|md_prefix_lnk(query.key)|md_self_link(help.name),help.default_value, help.long_description|firstline]) %}{%- endif %}
{%- endfor %}
{{table|rst_table('Option', 'Default Value', 'Description')}}

{% for help in query.params -%}
<a name="{{help.name|md_prefix_lnk(query.key)}}"/>
### {{help.name}}

{% if help.default_value %}
**Deafult Value:** {{help.default_value}}
{%- endif %}

**Description:**
{%if help.ext -%}
{{help.ext.head}}

{{help.ext.data|rst_table}}

{{help.ext.tail}}

{% else -%}
{{help.long_description}}
{%- endif %}
{{'\n'}}
{%- endfor %}
{%- endfor %}
{%- endif %}

{% if module.paths -%}
# Configuration

{% for pkey,path in module.paths|dictsort -%}
<a name="{{path.key}}"/>
## {{path.info.title}}

{{path.info.description}}

```ini
# {{path.info.description|firstline}}
[{{path.key}}]
{% for kkey,key in path.keys|dictsort -%}
{% if key.info.default_value|extract_value -%}
{{kkey}}={{key.info.default_value|extract_value}}
{% endif %}
{%- endfor %}
```

{% set tbl = [] -%}
{% set pkey = path.key|md_self_link -%}
{% for k,key in path.keys|dictsort  -%}
    {% set kkey = k|md_prefix_lnk(path.key)|md_self_link(k) -%}
    {% do tbl.append([kkey, key.info.default_value|extract_value, key.info.title|firstline]) -%}
{%- endfor %}
{{tbl|rst_table('Key', 'Default Value', 'Description')}}


{% for kkey,key in path.keys|dictsort %}
<a name="{{kkey|md_prefix_lnk(path.key)}}"/>
### {{kkey}}

**{{key.info.title|as_text}}**

{%if key.ext -%}
{{key.ext.head}}

{{key.ext.data|rst_table}}

{{key.ext.tail}}

{% else -%}
{{key.info.description|as_text}}
{%- endif %}

{% set table = [] -%}
{% do table.append(['Path:', path.key|md_self_link(path.key)]) -%}
{% do table.append(['Key:', kkey]) -%}
{% if key.info.advanced -%}
{% do table.append(['Advanced:', 'Yes (means it is not commonly used)']) -%}
{%- endif %}
{% if key.info.default_value|extract_value -%}
{% do table.append(['Default value:', '`' + key.info.default_value|extract_value + '`']) -%}
{% else %}
{% do table.append(['Default value:', '_N/A_']) -%}
{%- endif %}
{% if key.info.sample -%}
{% do table.append(['Sample key:', 'Yes (This section is only to show how this key is used)']) -%}
{%- endif %}
{% do table.append(['Used by:', ', '.join(path.info.plugin|sort)]) -%}
{{table|rst_table('Key', 'Description')}}

#### Sample

```
[{{path.key}}]
# {{key.info.title}}
{{kkey}}={{key.info.default_value|extract_value}}
```

{% endfor %}
{% endfor %}
{%- endif %}
"""



index_template = u"""

# Modules

{% set table = [] -%}
{% for key,module in plugins|dictsort  -%}
    {% do table.append([module.namespace, ('reference/' + module.namespace + '/' + module.key)|md_link(module.key), module.info.description|firstline]) -%}
{%- endfor %}
{{table|rst_table('Type', 'Module', 'Description')}}

# Queries

{% set table = [] -%}
{% for mk,module in plugins|dictsort  -%}
    {% for key,query in module.queries|dictsort  -%}
        {% set desc = query.info.description|firstline %}
        {% if desc.startswith('Alternative name for:') -%}
            {% set desc = query.info.description[22:]|rst_link('query') -%}
        {%- elif desc.startswith('Legacy version of ') -%}
            {% set desc = '**Deprecated** please use: ' + query.info.description[18:]|rst_link('query') -%}
        {%- elif query.info.description.startswith('Alias for:') -%}
            {% set desc = query.info.description[11:]|rst_link('query') -%}
        {%- endif %}
        {% do table.append([mk, ('reference/' + module.namespace + '/' + module.key + '#' + query.key)|md_link(query.key), desc]) -%}
    {%- endfor %}
{%- endfor %}
{{table|rst_table('Module', 'Command', 'Description')}}
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

    def append_plugin(self, info, folder):
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
            spath = '%s/samples/%s_samples.md'%(folder, name)
            self.plugins[name].sample = ''
            if os.path.exists(spath):
                with open(spath) as f:
                    self.plugins[name].sample = unicode(f.read(), 'utf8')
                    self.plugins[name].sample_source = 'samples/%s_samples.md'%(name)
            spath = '%s/samples/%s_desc.md'%(folder, name)
            self.plugins[name].ext_desc = ''
            if os.path.exists(spath):
                with open(spath) as f:
                    self.plugins[name].ext_desc = unicode(f.read(), 'utf8')
                    self.plugins[name].ext_desc_source = 'samples/%s_desc.md'%(name)
            
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

def make_md_link(name, title = None):
    if title:
        return '[%s](%s)'%(title, name)
    return '[%s](%s)'%(name, name)

def make_md_self_link(name, title = None):
    if title:
        return '[%s](#%s)'%(title, name)
    return '[%s](#%s)'%(name, name)
def make_md_code(name):
    return '`%s`'%name
def make_md_prefix_lnk(value, prefix):
    return '%s_%s'%(prefix, value)

def largest_value(a,b):
    return map(lambda n: n[0] if len(n[0])>len(n[1]) else n[1], zip(a, b))

def extract_value(value):
    if value.HasField("string_data"):
        return value.string_data
    if value.HasField("int_data"):
        return '%d'%value.int_data
    if value.HasField("bool_data"):
        return "true" if value.bool_data else "false"
    return ''

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

def render_rst_table(table, *args):
    if not table:
        return ''
    if args:
        table.insert(0, args)
    ret = ''
    maxcol = map(lambda a:len(a), reduce(lambda a,b: largest_value(a,b), table))
    for idx, line in enumerate(table):
        ret += '|' + ''.join(map(lambda a:' ' + a[1].ljust(a[0],' ') + ' |', zip(maxcol, line))) + '\n'
        if idx == 0:
            ret += '|' + ''.join(map(lambda a:'-' + ''.ljust(a[0],'-') + '-|', zip(maxcol, line))) + '\n'
    return ret
    
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
    folder = None
    command_cache = {}
    
    def __init__(self, plugin_id, plugin_alias, script_alias):
        self.plugin_id = plugin_id
        self.plugin_alias = plugin_alias
        self.script_alias = script_alias
        self.conf = Settings.get(self.plugin_id)
        self.registry = Registry.get(self.plugin_id)
        self.core = Core.get(self.plugin_id)
        self.command_cache = {}
        self.folder = None
        
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
            root.append_plugin(p, self.folder)
        for p in self.get_paths():
            root.append_path(p)
            for k in self.get_keys(p.node.path):
                root.append_key(k)
        for p in self.get_queries():
            root.append_command(p)
        for p in self.get_query_aliases():
            root.append_alias(p)
        return root
        

    def fetch_command(self, module, minfo, command, cinfo):
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
        spath = '%s/samples/%s_%s_samples.md'%(self.folder, module, command)
        cinfo.sample = ''
        if os.path.exists(spath):
            with open(spath) as f:
                cinfo.sample = unicode(f.read(), 'utf8')
                cinfo.sample_source = 'samples/%s_%s_samples.md'%(module, command)
        self.command_cache[command] = cinfo
        return cinfo

    def generate_rst(self, input_dir, output_dir):
        root = self.get_info()
        i = 0
        
        env = Environment(extensions=["jinja2.ext.do",])
        env.filters['firstline'] = first_line
        env.filters['rst_link'] = make_rst_link
        env.filters['md_link'] = make_md_link
        env.filters['md_prefix_lnk'] = make_md_prefix_lnk
        env.filters['md_self_link'] = make_md_self_link
        env.filters['md_code'] = make_md_code
        env.filters['rst_table'] = render_rst_table
        env.filters['rst_heading'] = render_rst_heading
        env.filters['extract_value'] = extract_value
        env.filters['block_pad'] = block_pad
        env.filters['common_head'] = calculate_common_head
        env.filters['as_text'] = as_text
        
        for (module,minfo) in root.plugins.iteritems():
            out_base_path = '%s/docs/'%output_dir
            sample_base_path = '%s/docs/samples/'%output_dir
            if minfo.namespace:
                out_base_path = '%s/docs/reference/%s/'%(output_dir, minfo.namespace)
            hash = root.get_hash()
            minfo.key = module
            minfo.queries = {}
            sfile = '%s%s_samples.inc'%(sample_base_path, module)
            if os.path.exists(sfile):
                minfo.sample = os.path.basename(sfile)
            sfile = '%s%s_samples.rst'%(sample_base_path, module)
            if os.path.exists(sfile):
                minfo.sample = os.path.basename(sfile)

            for (c,cinfo) in sorted(root.commands.iteritems()):
                if module in cinfo.info.plugin:
                    more_info = self.fetch_command(module, minfo, c,cinfo)
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
            render_template(hash, template, '%s/%s.md'%(out_base_path, module))

        hash = root.get_hash()
        template = env.from_string(index_template)
        render_template(hash, template, '%s/docs/reference/index.md'%output_dir)
        
        log_debug('%s/samples/index.rst'%output_dir)
        template = env.from_string(samples_template)
        render_template(hash, template, '%s/samples/index.rst'%output_dir)

    def main(self, args):
        parser = OptionParser(prog="")
        parser.add_option("-o", "--output", help="write report to FILE(s)")
        parser.add_option("-i", "--input", help="Reference folder")
        (options, args) = parser.parse_args(args=args)
        self.folder = options.output
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
