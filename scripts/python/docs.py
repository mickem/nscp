#!/usr/bin/env python
# -*- coding: utf-8 -*-
from NSCP import Settings, Registry, Core, log, log_debug, log_error, status
#import plugin_pb2
import registry_pb2
import settings_pb2
from optparse import OptionParser
import os
import datetime
from functools import reduce
from jinja2 import Template, Environment
import hashlib
import sys
helper = None

module_template = u"""# {{module.key}}

{{module.info.description}}

{%if module.ext_desc -%}
{{ module.ext_desc}}
{%- endif %}

## Enable module

To enable this module and and allow using the commands you need to ass `{{module.key}} = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
{{module.key}} = enabled
```

{% if module.sample %}
## Samples

_Feel free to add more samples [on this page](https://github.com/mickem/nscp/blob/master/docs/{{module.sample_source}})_

{{module.sample}}

{%- endif %}
{% if module.queries -%}

## Queries

A quick reference for all available queries (check commands) in the {{module.key}} module.

**List of commands:**

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

{% if module.aliases -%}
**List of command aliases:**

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

{% for k,query in module.queries|dictsort -%}

### {{query.key}}

{{query.info.description}}
{% if query.ext_desc %}
{{query.ext_desc}}
{%- endif %}

**Jump to section:**

{% if query.sample -%}
* [Sample Commands](#{{"samples"|md_prefix_lnk(query.key)}})
{% endif -%}
* [Command-line Arguments](#{{"options"|md_prefix_lnk(query.key)}})
{% if query.fields -%}
* [Filter keywords](#{{"filter_keys"|md_prefix_lnk(query.key)}})
{% endif %}

{% if query.sample -%}
<a id="{{"samples"|md_prefix_lnk(query.key)}}"></a>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/{{query.sample_source}})_

{{query.sample}}
{%- endif %}

{% for help in query.params -%}{%- if help.is_simple %}
<a id="{{help.name|md_prefix_lnk(query.key)}}"></a>{% endif %}{%- endfor %}
<a id="{{"options"|md_prefix_lnk(query.key)}}"></a>
#### Command-line Arguments

{% set table = [] -%}
{% for help in query.params -%}
    {%- if help.is_simple %}{% if help.content_type == 4 -%}
        {% do table.append([help.name, 'N/A', help.long_description|firstline]) %}{% else -%}
        {% do table.append([help.name, help.default_value|unexpand_path, help.long_description|firstline]) %}{%- endif %}
    {%- else %}{% if help.content_type == 4 -%}
        {% do table.append([help.name|md_prefix_lnk(query.key)|md_self_link(help.name), 'N/A', help.long_description|firstline]) %}{% else -%}
        {% do table.append([help.name|md_prefix_lnk(query.key)|md_self_link(help.name), help.default_value|unexpand_path, help.long_description|firstline]) %}{%- endif %}
    {%- endif %}
{%- endfor %}
{{table|rst_table('Option', 'Default Value', 'Description')}}

{% for help in query.params -%}{%- if not help.is_simple %}
<h5 id="{{help.name|md_prefix_lnk(query.key)}}">{{help.name}}:</h5>

{{help.long_description}}
{% if help.default_value %}
*Default Value:* `{{help.default_value|unexpand_path}}`
{%- endif %}{{'\n'}}
{%- endif %}{%- endfor %}

{% if query.fields -%}
<a id="{{"filter_keys"|md_prefix_lnk(query.key)}}"></a>
#### Filter keywords

{% set table = [] -%}
{% for help in query.own_fields -%}
    {% do table.append([help.name,help.long_description|firstline]) %}
{%- endfor %}
{{table|rst_table('Option', 'Description')}}
**Common options for all checks:**
{% set table = [] -%}
{% for help in query.generic_fields -%}
    {% do table.append([help.name,help.long_description|replace("Common option for all checks.","")|firstline]) %}
{%- endfor %}
{{table|rst_table('Option', 'Description')}}
{{'\n'}}
{%- endif %}

{%- endfor %}
{%- endif %}

{% if module.paths -%}
## Configuration

{% set has_paths = [] -%}
{% set paths = [] -%}
{% for pk,path in module.paths|dictsort  -%}
    {% set pkey = path.info.title|as_text|mkref|md_self_link(pk) -%}
    {% if pk not in has_paths -%}
        {% do has_paths.append(pk) -%}
        {% do paths.append([pkey, path.info.title|firstline]) -%}
    {%- endif %}
{%- endfor %}

{{paths|rst_table('Path / Section', 'Description')}}


{% for pkey,path in module.paths|dictsort -%}
### {% if path.info.title %}{{path.info.title}}{%else%}{{pkey}}{%endif%} <a id="{{path.key}}"></a>

{{path.info.description}}

{% if path.info.subkey %}
This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.
{% if path.sample %}

**Keys:**

{% set tbl = [] -%}
{% for k,key in path.sample.keys|dictsort  -%}
    {% do tbl.append([k, key.info.default_value|unexpand_path, key.info.title|firstline]) -%}
{%- endfor %}
{{tbl|rst_table('Key', 'Default Value', 'Description')}}

**Sample:**

```ini
# An example of a {{path.info.title}} section
[{{path.key}}/sample]
{% for kkey,key in path.sample.keys|dictsort -%}
{% if key.info.default_value %}{{kkey}}={{key.info.default_value|unexpand_path}}
{% else %}#{{kkey}}=...
{% endif %}
{%- endfor %}
```
{% endif %}

{% if path.objects %}
**Known instances:**

{% for k in path.objects|sort %}*  {{k}}
{% endfor %}


{% endif %}
{% else %}

{% set tbl = [] -%}
{% set pkey = path.key|md_self_link -%}
{% for k,key in path.keys|dictsort  -%}
    {% set kkey = key.info.title|as_text|mkref|md_self_link(k) -%}
    {% do tbl.append([kkey, key.info.default_value|unexpand_path, key.info.title|firstline]) -%}
{%- endfor %}
{{tbl|rst_table('Key', 'Default Value', 'Description')}}


```ini
# {{path.info.description|firstline}}
[{{path.key}}]
{% for kkey,key in path.keys|dictsort -%}
{% if key.info.default_value -%}
{{kkey}}={{key.info.default_value|unexpand_path}}
{% endif %}
{%- endfor %}
```

{% endif %}

{% for kkey,key in path.keys|dictsort %}

#### {{key.info.title|as_text}} <a id="{{kkey|md_conf_lnk(path.key)}}"></a>

{{key.info.description|as_text}}


{% set table = [] -%}
{% do table.append(['Path:', path.key|md_self_link(path.key)]) -%}
{% do table.append(['Key:', kkey]) -%}
{% if key.info.advanced -%}
{% do table.append(['Advanced:', 'Yes (means it is not commonly used)']) -%}
{%- endif %}
{% if key.info.default_value -%}
{% do table.append(['Default value:', '`' + key.info.default_value|unexpand_path + '`']) -%}
{% else %}
{% do table.append(['Default value:', '_N/A_']) -%}
{%- endif %}
{% if key.info.sample -%}
{% do table.append(['Sample key:', 'Yes (This section is only to show how this key is used)']) -%}
{%- endif %}
{{table|rst_table('Key', 'Description')}}

**Sample:**

```
[{{path.key}}]
# {{key.info.title}}
{{kkey}}={{key.info.default_value|unexpand_path}}
```

{% endfor %}
{% endfor %}
{%- endif %}
"""



index_template = u"""

# Modules

{% set table = [] -%}
{% for key,module in plugins|dictsort  -%}
    {% do table.append([module.namespace, (module.namespace + '/' + module.key + '.md')|md_link(module.key), module.info.description|firstline]) -%}
{%- endfor -%}
{{table|rst_table('Type', 'Module', 'Description')}}

# Queries
{% set table = [] -%}
{% for mk,module in plugins|dictsort  -%}
    {% for key,query in module.queries|dictsort  -%}
        {% set desc = query.info.description|firstline -%}
        {% if desc.startswith('Alternative name for:') -%}
            {% set desc = query.info.description[22:]|rst_link('query') -%}
        {%- elif desc.startswith('Legacy version of ') -%}
            {% set desc = '**Deprecated** please use: ' + query.info.description[18:]|rst_link('query') -%}
        {%- elif query.info.description.startswith('Alias for:') -%}
            {% set desc = query.info.description[11:]|rst_link('query') -%}
        {%- endif -%}
        {% do table.append([mk, (module.namespace + '/' + module.key + '.md#' + query.key)|md_link(query.key), desc]) -%}
    {%- endfor -%}
{%- endfor -%}
{{table|rst_table('Module', 'Command', 'Description')}}
"""


# --------------------------------------------------------------------------
# man-page (roff) templates
#
# Emitted straight to troff (no external converter) from the same inventory
# used for the Markdown above. Lines starting with a macro (.SH, .TP, ...) must
# sit at column 0, so the control blocks use whitespace-trimming ({%- -%}) to
# avoid leaking indentation. All interpolated text goes through the `roff`
# filter (escapes backslash/hyphen and protects leading dots); free-form
# descriptions go through `man_para` (escapes + splits into .PP paragraphs).
# --------------------------------------------------------------------------

# One section-1 page per module, listing its queries (check commands).
man_module_template = u""".\\" Generated by docs.py - do not edit.
.TH "NSCP-{{module.key|upper}}" "1" "{{date}}" "NSClient++{% if version %} {{version}}{% endif %}" "NSClient++ Reference"
.SH NAME
nscp\\-{{module.key|roff}} \\- {{module.info.description|firstline|roff}}
.SH DESCRIPTION
{{module.info.description|man_para}}
{%- if module.ext_desc %}
.PP
{{module.ext_desc|man_para}}
{%- endif %}
.PP
To enable this module add \\fB{{module.key|roff}} = enabled\\fR to the \\fB[/modules]\\fR section of nsclient.ini.
{%- if module.queries %}
.SH QUERIES
{%- for key, query in module.queries|dictsort %}
.SS {{query.key|roff}}
{{query.info.description|man_para}}
{%- if query.ext_desc %}
.PP
{{query.ext_desc|man_para}}
{%- endif %}
.PP
.B Synopsis:
.PP
.nf
nscp client \\-\\-query {{query.key|roff}} [options]
.fi
{%- if query.params %}
.PP
.B Options:
{%- for help in query.params %}
.TP
.B {{help.name|roff}}{% if help.content_type != 4 and help.default_value %} (default: {{help.default_value|unexpand_path|roff}}){% endif %}
{{help.long_description|firstline|roff}}
{%- endfor %}
{%- endif %}
{%- if query.own_fields %}
.PP
.B Filter keywords:
{%- for help in query.own_fields %}
.TP
.B {{help.name|roff}}
{{help.long_description|firstline|roff}}
{%- endfor %}
{%- endif %}
{%- endfor %}
{%- endif %}
.SH "SEE ALSO"
.BR nscp (1),
.BR nsclient.ini (5)
"""

# Section-5 page documenting the configuration file and every known section.
man_config_template = u""".\\" Generated by docs.py - do not edit.
.TH "NSCLIENT.INI" "5" "{{date}}" "NSClient++" "NSClient++ Configuration"
.SH NAME
nsclient.ini \\- configuration file for NSClient++
.SH DESCRIPTION
NSClient++ reads its configuration from \\fInsclient.ini\\fR, an INI\\-style file
divided into sections. Each section below lists its available keys with their
default values and meaning.
.SH SECTIONS
{%- for pkey, path in paths|dictsort %}
.SS {{path.key|roff}}
{%- if path.info.title %}
{{path.info.title|firstline|roff}}
{%- endif %}
{%- if path.info.description %}
.PP
{{path.info.description|man_para}}
{%- endif %}
{%- if path.info.subkey %}
.PP
This is a template section: create your own \\fB[{{path.key|roff}}/<name>]\\fR sections using the following keys.
{%- set keys = path.sample.keys if path.sample else {} %}
{%- else %}
{%- set keys = path.keys %}
{%- endif %}
{%- for kkey, key in keys|dictsort %}
.TP
.B {{kkey|roff}}{% if key.info.default_value %} (default: {{key.info.default_value|unexpand_path|roff}}){% endif %}
{{key.info.title|firstline|roff}}
{%- endfor %}
{%- endfor %}
.SH "SEE ALSO"
.BR nscp (1)
"""

# Section-1 umbrella page for the nscp binary, indexing the per-module pages.
man_nscp_template = u""".\\" Generated by docs.py - do not edit.
.TH "NSCP" "1" "{{date}}" "NSClient++" "NSClient++ Reference"
.SH NAME
nscp \\- NSClient++ monitoring agent and command line client
.SH SYNOPSIS
.B nscp
[\\fImode\\fR] [\\fIoptions\\fR]
.SH DESCRIPTION
NSClient++ (\\fBnscp\\fR) is a monitoring agent and client. It exposes check
commands (queries) that can be invoked locally or remotely (for example over
NRPE or NSCA). Each module provides a set of queries; the available modules are
listed below. Configuration is documented in \\fBnsclient.ini\\fR(5).
.SH MODULES
{%- for key, module in plugins|dictsort %}
.TP
.BR nscp\\-{{module.key|roff}} (1)
{{module.info.description|firstline|roff}}
{%- endfor %}
.SH "SEE ALSO"
.BR nsclient.ini (5)
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
    subkeys = {}
    commands = {}
    aliases = {}
    plugins = {}
    windows_modules = ['CheckSystem', 'CheckDisk', 'NSClientServer', 'DotnetPlugins', 'CheckEventLog',  'CheckTaskSched',  'CheckWMI']
    check_modules = ['CheckExternalScripts',  'CheckHelpers',  'CheckLogFile',  'CheckMKClient',  'CheckMKServer',  'CheckNSCP', 'CheckNet']
    client_modules = ['GraphiteClient', 'IcingaClient',  'NRDPClient',  'NRPEClient',  'NRPEServer',  'NSCAClient',  'NSCANgClient', 'NSCAServer',  'NSClientServer',  'SMTPClient',  'SyslogClient']
    generic_modules = ['CommandClient',  'DotnetPlugins',  'LUAScript',  'PythonScript',  'Scheduler',  'SimpleCache',  'SimpleFileWriter', 'WEBServer']
    ignored_modules = ['CauseCrashes', 'SamplePluginSimple']

    def __init__(self):
        self.paths = {}
        self.commands = {}
        self.aliases = {}
        self.plugins = {}
        self.subkeys = {}

    def append_key(self, info):
        path = info.node.path
        if path in self.paths:
            self.paths[path].append_key(info)
        else:
            p = path_container()
            p.append_key(info)
            self.paths[path] = p
        
    def process_paths(self):
        for k in self.subkeys.keys():
            self.paths[k].objects = self.paths[k].keys
            self.paths[k].keys = {}
            keys = list(self.paths.keys())
            for pk in keys:
                if pk in self.subkeys.keys():
                    continue
                if pk.startswith(k):
                    if pk[len(k)+1:] == "sample":
                        self.paths[k].sample = self.paths[pk]
                    if pk[len(k)+1:] == "default":
                        self.paths[k].default = self.paths[pk]
                    del self.paths[pk]

    def append_path(self, info):
        path = info.node.path
        if not path in self.paths:
            self.paths[path] = path_container(info)
        if info.info.subkey:
            if path not in self.subkeys.keys():
                self.subkeys[path] = self.paths[path]

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
        if name in self.ignored_modules:
            return
        namespace = ''
        if name in self.windows_modules and sys.platform == 'win32':
            namespace = 'windows'
        elif name in self.windows_modules:
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
                with open(spath, encoding='utf-8') as f:
                    self.plugins[name].sample = f.read()
                    self.plugins[name].sample_source = 'samples/%s_samples.md'%(name)
            spath = '%s/samples/%s_desc.md'%(folder, name)
            self.plugins[name].ext_desc = ''
            if os.path.exists(spath):
                with open(spath, encoding='utf-8') as f:
                    self.plugins[name].ext_desc = f.read().strip(" \t")
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
        self.default = None
        self.sample = None
        self.ext_desc = None
        self.ext_desc_source = None
        self.objects = {}
        
    def append_key(self, info):
        self.keys[info.node.key] = info

class command_container(object):
    info = None
    parameters = None
    def __init__(self, info = None):
        self.info = info.info
        self.parameters = info.parameters

class param_container:
    
    def __init__(self, info):
        self.name = info.name
        self.default_value = info.default_value
        self.short_description = info.short_description
        self.long_description = info.long_description
        self.required = info.required
        self.repeatable = info.repeatable
        self.content_type = info.content_type
        self.keyword = info.keyword
        self.is_simple = True
        if info.default_value or '\n' in info.long_description:
            self.is_simple = False

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
    title = title or name
    name = name.replace('(', '').replace(')', '')
    return '[%s](%s)'%(title, name)

def make_md_self_link(name, title = None):
    title = title or name
    name = name.replace('(', '').replace(')', '')
    return '[%s](#%s)'%(title, name)
def make_md_code(name):
    return '`%s`'%name
def make_md_prefix_lnk(value, prefix):
    return '%s_%s'%(prefix, value)
def make_md_conf_lnk(value, prefix):
    return '%s/%s'%(prefix, value)
def mkref(value):
    return value.lower().replace(" ", "-")

# Known path variables resolved via core.expand_path. Mirrors the fixed set in
# service/path_manager.cpp get_path_for_key — unknown keys there fall back to
# base-path, so probing arbitrary names would not yield more matches.
KNOWN_PATH_KEYS = [
    'base-path', 'exe-path', 'shared-path', 'data-path', 'appdata',
    'common-appdata', 'certificate-path', 'module-path', 'web-path',
    'scripts', 'log-path', 'cache-folder', 'crash-folder', 'ca-path',
    'temp', 'etc',
]

def _normalize_path(value):
    return value.replace('\\', '/').rstrip('/').lower()

def make_unexpand_path(path_cache):
    # path_cache is a list of (key, expanded) pre-sorted longest-first so the
    # most specific prefix wins (e.g. ${certificate-path} before ${shared-path}).
    def unexpand(value):
        if not value:
            return value
        norm = _normalize_path(value)
        for key, expanded in path_cache:
            exp_norm = _normalize_path(expanded)
            if not exp_norm:
                continue
            if norm == exp_norm:
                return '${%s}' % key
            if norm.startswith(exp_norm + '/'):
                return '${%s}%s' % (key, value[len(expanded):])
        return value
    return unexpand

def largest_value(a,b):
    return map(lambda n: n[0] if len(n[0])>len(n[1]) else n[1], zip(a, b))

def as_text(value):
    value = value.replace('\\', '\\\\')
    value = value.replace('`', '\\`')
    value = value.replace('|', '\\|')
    return value

def roff_escape(value):
    # Make an arbitrary string safe to interpolate into roff. Backslash must go
    # first (it is roff's escape char); hyphens become \- so options like
    # --warning render as real minus signs and stay copy-pasteable.
    if not value:
        return ''
    value = str(value)
    value = value.replace('\\', '\\e')
    value = value.replace('-', '\\-')
    return value

def man_paragraphs(value):
    # Turn free-form text into roff paragraphs separated by .PP. Each line is
    # roff-escaped, and a leading '.' or "'" (which roff would treat as a macro)
    # is neutralized with \&.
    if not value:
        return ''
    blocks = []
    for block in str(value).split('\n\n'):
        if not block.strip():
            continue
        lines = []
        for line in block.split('\n'):
            line = roff_escape(line.strip())
            if line[:1] in ('.', "'"):
                line = '\\&' + line
            lines.append(line)
        blocks.append('\n'.join(lines))
    return '\n.PP\n'.join(blocks)

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
        table.insert(0, list(args))
    ret = ''
    maxcol = list(map(lambda a:len(a), reduce(lambda a,b: largest_value(a,b), table)))
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
    rendered = template.render(hash)
    # Normalize line endings to Unix (LF) for consistent output across platforms
    rendered = rendered.replace('\r\n', '\n')
    data = rendered.encode('utf8')

    path = os.path.dirname(filename)
    if not os.path.exists(path):
        os.makedirs(path)

    if os.path.exists(filename):
        m1 = hashlib.sha256()
        m1.update(data)
        sha1 = m1.digest()
        with open(filename, encoding='utf-8') as f:
            m2 = hashlib.sha256()
            m2.update(f.read().encode('utf8'))
            sha2 = m2.digest()
        if sha1 == sha2:
            log_debug("no changes detected in: %s"%filename)
            return

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
        message = settings_pb2.SettingsRequestMessage()
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
        message = registry_pb2.RegistryRequestMessage()
        payload = message.payload.add()
        payload.inventory.fetch_all = True
        payload.inventory.type.append(type)
        return message.SerializeToString()

    def get_paths(self):
        (code, data) = self.conf.query(self.build_inventory_request())
        if code == 1:
            message = settings_pb2.SettingsResponseMessage()
            message.ParseFromString(data)
            for payload in message.payload:
                if payload.inventory:
                    log_debug('Found %d paths'%len(payload.inventory))
                    return payload.inventory
        return []

    def get_keys(self, path):
        (code, data) = self.conf.query(self.build_inventory_request(path, False, True))
        if code == 1:
            message = settings_pb2.SettingsResponseMessage()
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
            message = registry_pb2.RegistryResponseMessage()
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
            message = registry_pb2.RegistryResponseMessage()
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
            message = registry_pb2.RegistryResponseMessage()
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
        root.process_paths()
        for p in self.get_queries():
            root.append_command(p)
        for p in self.get_query_aliases():
            root.append_alias(p)
        return root
        

    def fetch_command(self, module, minfo, command, cinfo):
        if command in self.command_cache:
            return self.command_cache[command]
        cinfo.fields = cinfo.parameters.fields
        cinfo.generic_fields = list(filter(lambda tf: tf.long_description.endswith('Common option for all checks.'), cinfo.parameters.fields))
        cinfo.own_fields = list(filter(lambda tf: not tf.long_description.endswith('Common option for all checks.'), cinfo.parameters.fields))
        cinfo.params = []
        for p in cinfo.parameters.parameter:
            cinfo.params.append(param_container(p))
        spath = '%s/samples/%s_%s_samples.md'%(self.folder, module, command)
        cinfo.sample = ''
        if os.path.exists(spath):
            with open(spath, encoding='utf-8') as f:
                cinfo.sample = f.read()
                cinfo.sample_source = 'samples/%s_%s_samples.md'%(module, command)

        spath = '%s/samples/%s_%s_desc.md'%(self.folder, module, command)
        cinfo.ext_desc = ''
        if os.path.exists(spath):
            with open(spath, encoding='utf-8') as f:
                cinfo.ext_desc = f.read().strip(" \t")
                cinfo.ext_desc_source = 'samples/%s_%s_desc.md'%(module, command)

        self.command_cache[command] = cinfo
        return cinfo

    def build_path_cache(self):
        # Resolve every known path variable once via core.expand_path so the
        # template can rewrite absolute defaults (c:\Program Files\...) back to
        # ${var}/... form. Sorted longest-first so a more specific prefix
        # (${certificate-path}) wins over an ancestor (${shared-path}).
        cache = {}
        for k in KNOWN_PATH_KEYS:
            try:
                v = self.core.expand_path('${%s}' % k)
            except Exception as e:
                log_debug('expand_path failed for ${%s}: %s' % (k, e))
                continue
            if v and v != '${%s}' % k:
                cache[k] = v
        return sorted(cache.items(), key=lambda kv: len(kv[1]), reverse=True)

    def generate_rst(self, input_dir, output_dir):
        root = self.get_info()
        i = 0

        path_cache = self.build_path_cache()
        log_debug('Resolved %d path variables for default-value rewriting' % len(path_cache))

        env = Environment(extensions=["jinja2.ext.do",])
        env.filters['firstline'] = first_line
        env.filters['rst_link'] = make_rst_link
        env.filters['md_link'] = make_md_link
        env.filters['md_prefix_lnk'] = make_md_prefix_lnk
        env.filters['md_conf_lnk'] = make_md_conf_lnk
        env.filters['md_self_link'] = make_md_self_link
        env.filters['md_code'] = make_md_code
        env.filters['rst_table'] = render_rst_table
        env.filters['rst_heading'] = render_rst_heading
        env.filters['block_pad'] = block_pad
        env.filters['common_head'] = calculate_common_head
        env.filters['as_text'] = as_text
        env.filters['mkref'] = mkref
        env.filters['unexpand_path'] = make_unexpand_path(path_cache)
        env.filters['roff'] = roff_escape
        env.filters['man_para'] = man_paragraphs
        
        for (module,minfo) in root.plugins.items():
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

            for (c,cinfo) in sorted(root.commands.items()):
                if cinfo.info.description.startswith('Legacy version of'):
                    continue
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
            for (c,cinfo) in sorted(root.aliases.items()):
                if module in cinfo.info.plugin:
                    cinfo.key = c
                    minfo.aliases[c] = cinfo
                    
            minfo.paths = {}
            for (c,cinfo) in sorted(root.paths.items()):
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

        self.generate_man(root, env, output_dir)

    def generate_man(self, root, env, output_dir):
        # Emit roff man pages from the same inventory already collected for the
        # Markdown: one section-1 page per module (reusing the queries/paths
        # populated onto each minfo above), a section-5 page for nsclient.ini,
        # and a section-1 umbrella for nscp itself. No external converter.
        man_date = datetime.date.today().strftime('%Y-%m-%d')

        module_t = env.from_string(man_module_template)
        for (module, minfo) in root.plugins.items():
            hash = root.get_hash()
            hash['module'] = minfo
            hash['date'] = man_date
            hash['version'] = ''
            render_template(hash, module_t, '%s/man/man1/nscp-%s.1'%(output_dir, module))

        hash = root.get_hash()
        hash['date'] = man_date
        render_template(hash, env.from_string(man_config_template), '%s/man/man5/nsclient.ini.5'%output_dir)

        hash = root.get_hash()
        hash['date'] = man_date
        render_template(hash, env.from_string(man_nscp_template), '%s/man/man1/nscp.1'%output_dir)

    def main(self, args):
        parser = OptionParser(prog="")
        parser.add_option("-o", "--output", help="write report to FILE(s)")
        parser.add_option("-i", "--input", help="Reference folder")
        (options, args) = parser.parse_args(args=args)
        self.folder = options.output
        self.generate_rst(options.input, options.output)

def __main__(args):
    global helper
    helper.main(args)
    return 0
    
def init(plugin_id, plugin_alias, script_alias):
    global helper
    helper = DocumentationHelper(plugin_id, plugin_alias, script_alias)

def shutdown():
    global helper
    helper = None
