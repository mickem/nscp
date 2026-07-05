#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Documentation generator (step 2 of 2).
#
# Standalone: reads the git-committed per-module YAML produced by docs_extract.py
# (docs/reference/<Module>.yaml) and renders the Markdown reference under
# docs/docs/reference/. It needs only PyYAML + jinja2 -- no running nscp.
#
# Each YAML file carries up to two platform slices ("unix" / "windows"). The
# generator unions them:
#   * an item present on only one platform gets an "Available on ... only" note;
#   * a section whose rendered content differs between platforms is wrapped in
#     mkdocs-Material content tabs (=== "Windows" / === "Linux") so the reader can
#     see both behaviours; identical sections render once.
#
# Human-authored samples (docs/samples/*_samples.md, *_desc.md) are read here, as
# in the original docs.py -- they are platform-neutral and not part of the YAML.
from optparse import OptionParser
import os
import sys
import glob
import hashlib
from functools import reduce

import yaml
from jinja2 import Environment

helper = None

# Platform tab ordering + display labels (Windows first, matching the UI).
PLATFORM_ORDER = ['windows', 'unix']
PLATFORM_LABEL = {'windows': 'Windows', 'unix': 'Linux'}


# --- Sub-templates (markup lifted from docs.py's module_template, split so each
# renderable section can be rendered per-platform and merged). --------------------

# The query body is split into three renderable content sections (intro, args,
# filter keywords). Each is rendered per-platform and collapsed independently, so
# the Windows/Linux tabs land INSIDE a single "#### Command-line Arguments" /
# "#### Filter keywords" heading rather than duplicating every heading per tab.

# Intro prose (description + extended description). No heading of its own.
query_intro_template = u"""{{query.info.description}}
{% if query.ext_desc %}
{{query.ext_desc}}
{%- endif %}"""

# Content of the "Command-line Arguments" section (table + per-option detail),
# WITHOUT the heading/anchor -- those are emitted once by the layout template.
query_args_template = u"""{% for help in query.params -%}{%- if help.is_simple %}
<a id="{{help.name|md_prefix_lnk(query.key)}}"></a>{% endif %}{%- endfor %}
{% set table = [] -%}
{% for help in query.params -%}
    {%- if help.is_simple %}{% if help.content_type == 4 -%}
        {% do table.append([help.name, 'N/A', help.long_description|firstline]) %}{% else -%}
        {% do table.append([help.name, help.default_value, help.long_description|firstline]) %}{%- endif %}
    {%- else %}{% if help.content_type == 4 -%}
        {% do table.append([help.name|md_prefix_lnk(query.key)|md_self_link(help.name), 'N/A', help.long_description|firstline]) %}{% else -%}
        {% do table.append([help.name|md_prefix_lnk(query.key)|md_self_link(help.name), help.default_value, help.long_description|firstline]) %}{%- endif %}
    {%- endif %}
{%- endfor %}
{{table|rst_table('Option', 'Default Value', 'Description')}}

{% for help in query.params -%}{%- if not help.is_simple %}
<h5 id="{{help.name|md_prefix_lnk(query.key)}}">{{help.name}}:</h5>

{{help.long_description}}
{% if help.default_value %}
*Default Value:* `{{help.default_value}}`
{%- endif %}{{'\n'}}
{%- endif %}{%- endfor %}"""

# Content of the "Filter keywords" section, WITHOUT the heading/anchor.
query_filter_template = u"""{% set table = [] -%}
{% for help in query.own_fields -%}
    {% do table.append([help.name,help.long_description|firstline]) %}
{%- endfor %}
{{table|rst_table('Option', 'Description')}}
**Common options for all checks:**
{% set table = [] -%}
{% for help in query.generic_fields -%}
    {% do table.append([help.name,help.long_description|replace("Common option for all checks.","")|firstline]) %}
{%- endfor %}
{{table|rst_table('Option', 'Description')}}"""

# Layout scaffold: emits the single sub-headings, anchors and jump list, and drops
# the already-merged (possibly tabbed) section bodies into place. Rendered once per
# query, WITHOUT the "### <name>" heading (emitted outside).
query_layout_template = u"""{{intro}}

**Jump to section:**

{% if has_sample -%}
* [Sample Commands](#{{"samples"|md_prefix_lnk(query.key)}})
{% endif -%}
{% if has_args -%}
* [Command-line Arguments](#{{"options"|md_prefix_lnk(query.key)}})
{% endif -%}
{% if has_fields -%}
* [Filter keywords](#{{"filter_keys"|md_prefix_lnk(query.key)}})
{% endif %}
{% if has_sample %}
<a id="{{"samples"|md_prefix_lnk(query.key)}}"></a>
#### Sample Commands

{{query.sample}}
{% endif %}
{% if has_args %}
<a id="{{"options"|md_prefix_lnk(query.key)}}"></a>
#### Command-line Arguments

{{args}}
{% endif %}
{% if has_fields %}
<a id="{{"filter_keys"|md_prefix_lnk(query.key)}}"></a>
#### Filter keywords

{{filter}}
{% endif %}
"""

# Rendered once per configuration path/section (whole block, heading included).
path_block_template = u"""### {% if path.info.title %}{{path.info.title}}{%else%}{{path.key}}{%endif%} <a id="{{path.key}}"></a>

{{path.info.description}}

{% if path.info.subkey %}
This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.
{% if path.sample %}

**Keys:**

{% set tbl = [] -%}
{% for k,key in path.sample['keys']|dictsort  -%}
    {% do tbl.append([k, key.default_value, key.title|firstline]) -%}
{%- endfor %}
{{tbl|rst_table('Key', 'Default Value', 'Description')}}

**Sample:**

```ini
# An example of a {{path.info.title}} section
[{{path.key}}/sample]
{% for kkey,key in path.sample['keys']|dictsort -%}
{% if key.default_value %}{{kkey}}={{key.default_value}}
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
{% for k,key in path['keys']|dictsort  -%}
    {% set kkey = key.title|as_text|mkref|md_self_link(k) -%}
    {% do tbl.append([kkey, key.default_value, key.title|firstline]) -%}
{%- endfor %}
{{tbl|rst_table('Key', 'Default Value', 'Description')}}


```ini
# {{path.info.description|firstline}}
[{{path.key}}]
{% for kkey,key in path['keys']|dictsort -%}
{% if key.default_value -%}
{{kkey}}={{key.default_value}}
{% endif %}
{%- endfor %}
```

{% endif %}

{% for kkey,key in path['keys']|dictsort %}

#### {{key.title|as_text}} <a id="{{kkey|md_conf_lnk(path.key)}}"></a>

{{key.description|as_text}}


{% set table = [] -%}
{% do table.append(['Path:', path.key|md_self_link(path.key)]) -%}
{% do table.append(['Key:', kkey]) -%}
{% if key.advanced -%}
{% do table.append(['Advanced:', 'Yes (means it is not commonly used)']) -%}
{%- endif %}
{% if key.default_value -%}
{% do table.append(['Default value:', '`' + key.default_value + '`']) -%}
{% else %}
{% do table.append(['Default value:', '_N/A_']) -%}
{%- endif %}
{% if key.sample -%}
{% do table.append(['Sample key:', 'Yes (This section is only to show how this key is used)']) -%}
{%- endif %}
{{table|rst_table('Key', 'Description')}}

**Sample:**

```
[{{path.key}}]
# {{key.title}}
{{kkey}}={{key.default_value}}
```

{% endfor %}
"""

index_template = u"""

# Modules

{% set table = [] -%}
{% for key,module in modules|dictsort  -%}
    {% do table.append([module.namespace, (module.namespace + '/' + module.key + '.md')|md_link(module.key), module.description|firstline]) -%}
{%- endfor -%}
{{table|rst_table('Type', 'Module', 'Description')}}

# Queries
{% set table = [] -%}
{% for mk,module in modules|dictsort  -%}
    {% for key,query in module.queries|dictsort  -%}
        {% set desc = query.description|firstline -%}
        {% if desc.startswith('Alternative name for:') -%}
            {% set desc = query.description[22:]|rst_link('query') -%}
        {%- elif desc.startswith('Legacy version of ') -%}
            {% set desc = '**Deprecated** please use: ' + query.description[18:]|rst_link('query') -%}
        {%- elif query.description.startswith('Alias for:') -%}
            {% set desc = query.description[11:]|rst_link('query') -%}
        {%- endif -%}
        {% do table.append([mk, (module.namespace + '/' + module.key + '.md#' + query.key)|md_link(query.key), desc]) -%}
    {%- endfor -%}
{%- endfor -%}
{{table|rst_table('Module', 'Command', 'Description')}}
"""


# --- Filters / helpers (ported verbatim from docs.py). ---------------------------
def first_line(string):
    return (string or '').strip().split('\n')[0]


def make_rst_link(name, type, title=None):
    if title:
        return ':%s:`%s<%s>`' % (type, title, name)
    return ':%s:`%s`' % (type, name)


def make_md_link(name, title=None):
    title = title or name
    name = name.replace('(', '').replace(')', '')
    return '[%s](%s)' % (title, name)


def make_md_self_link(name, title=None):
    title = title or name
    name = name.replace('(', '').replace(')', '')
    return '[%s](#%s)' % (title, name)


def make_md_prefix_lnk(value, prefix):
    return '%s_%s' % (prefix, value)


def make_md_conf_lnk(value, prefix):
    return '%s/%s' % (prefix, value)


def mkref(value):
    return value.lower().replace(" ", "-")


def as_text(value):
    value = value.replace('\\', '\\\\')
    value = value.replace('`', '\\`')
    value = value.replace('|', '\\|')
    return value


def largest_value(a, b):
    return map(lambda n: n[0] if len(n[0]) > len(n[1]) else n[1], zip(a, b))


def render_rst_table(table, *args):
    if not table:
        return ''
    if args:
        table.insert(0, list(args))
    ret = ''
    maxcol = list(map(lambda a: len(a), reduce(lambda a, b: largest_value(a, b), table)))
    for idx, line in enumerate(table):
        ret += '|' + ''.join(map(lambda a: ' ' + a[1].ljust(a[0], ' ') + ' |', zip(maxcol, line))) + '\n'
        if idx == 0:
            ret += '|' + ''.join(map(lambda a: '-' + ''.ljust(a[0], '-') + '-|', zip(maxcol, line))) + '\n'
    return ret


def make_env():
    env = Environment(extensions=["jinja2.ext.do", ])
    env.filters['firstline'] = first_line
    env.filters['rst_link'] = make_rst_link
    env.filters['md_link'] = make_md_link
    env.filters['md_prefix_lnk'] = make_md_prefix_lnk
    env.filters['md_conf_lnk'] = make_md_conf_lnk
    env.filters['md_self_link'] = make_md_self_link
    env.filters['rst_table'] = render_rst_table
    env.filters['as_text'] = as_text
    env.filters['mkref'] = mkref
    return env


def render_template(text, filename):
    # Normalize line endings to Unix (LF); skip the write if nothing changed.
    text = text.replace('\r\n', '\n')
    data = text.encode('utf8')
    path = os.path.dirname(filename)
    if path and not os.path.exists(path):
        os.makedirs(path)
    if os.path.exists(filename):
        with open(filename, encoding='utf-8') as f:
            if hashlib.sha256(f.read().encode('utf8')).digest() == hashlib.sha256(data).digest():
                return
    with open(filename, "wb") as f:
        f.write(data)


# --- Content-tab machinery. ------------------------------------------------------
def _indent(text, n=4):
    pad = ' ' * n
    out = []
    for line in text.split('\n'):
        out.append(pad + line if line.strip() else '')
    return '\n'.join(out)


def tabify(rendered):
    # rendered: {platform: markdown}. Emit mkdocs-Material content tabs.
    blocks = []
    for p in PLATFORM_ORDER:
        if p not in rendered:
            continue
        body = _indent(rendered[p].strip('\n'))
        blocks.append('=== "%s"\n\n%s' % (PLATFORM_LABEL[p], body))
    return '\n\n'.join(blocks) + '\n'


def per_platform(present, render_one):
    # render_one(platform) -> markdown for that platform. Collapse to a single
    # rendering when every present platform produces identical output; otherwise
    # wrap the differing renderings in content tabs.
    rendered = {p: render_one(p) for p in present}
    values = list(rendered.values())
    if all(v == values[0] for v in values):
        return values[0]
    return tabify(rendered)


def availability_note(present, all_present):
    # Note shown when an item exists on fewer platforms than the module spans.
    if present == all_present or len(all_present) < 2:
        return ''
    labels = ', '.join(PLATFORM_LABEL[p] for p in PLATFORM_ORDER if p in present)
    return '*Available on %s only.*\n' % labels


# --- Platform factoring (storage-only; KEEP IN SYNC with docs_extract.py). -------
# The YAML stores shared data once under "common" plus per-platform overrides.
# expand() rebuilds each platform's full tree so the rest of the generator can keep
# consuming plain per-platform slices -- rendering (markers/tabs) is unchanged.
FACTOR_SECTIONS = ('queries', 'aliases', 'paths')


def expand_platform(data, platform):
    common = data.get('common', {})
    ov = data.get(platform, {})
    tree = {'module': data.get('module'), 'namespace': data.get('namespace')}
    info = ov.get('info', common.get('info'))
    if info is not None:
        tree['info'] = info
    for section in FACTOR_SECTIONS:
        merged = dict(common.get(section, {}))
        merged.update(ov.get(section, {}))
        if merged:
            tree[section] = merged
    return tree


def expand(data):
    return {p: expand_platform(data, p) for p in data.get('platforms', [])}


# --- Query / path preparation (compute the derived keys the templates expect). ---
def prepare_query(qdict, qname, module, sample_folder):
    q = dict(qdict)
    q['key'] = qname
    q.setdefault('info', {})
    q.setdefault('params', [])
    fields = q.get('fields', [])
    q['own_fields'] = [f for f in fields if not f.get('generic')]
    q['generic_fields'] = [f for f in fields if f.get('generic')]
    q['fields'] = fields
    q['sample'] = ''
    q['ext_desc'] = ''
    spath = '%s/samples/%s_%s_samples.md' % (sample_folder, module, qname)
    if os.path.exists(spath):
        with open(spath, encoding='utf-8') as f:
            q['sample'] = f.read()
    spath = '%s/samples/%s_%s_desc.md' % (sample_folder, module, qname)
    if os.path.exists(spath):
        with open(spath, encoding='utf-8') as f:
            q['ext_desc'] = f.read().strip(" \t")
    return q


class DocumentationGenerator(object):
    def __init__(self, yaml_dir, sample_folder):
        self.yaml_dir = yaml_dir
        self.sample_folder = sample_folder
        self.env = make_env()
        self.query_intro = self.env.from_string(query_intro_template)
        self.query_args = self.env.from_string(query_args_template)
        self.query_filter = self.env.from_string(query_filter_template)
        self.query_layout = self.env.from_string(query_layout_template)
        self.path_block = self.env.from_string(path_block_template)
        self.index = self.env.from_string(index_template)

    def load(self):
        modules = {}
        for path in sorted(glob.glob(os.path.join(self.yaml_dir, '*.yaml'))):
            with open(path, encoding='utf-8') as f:
                data = yaml.safe_load(f) or {}
            # Reconstruct full per-platform trees from the factored common/overrides.
            slices = {p: expand_platform(data, p) for p in data.get('platforms', [])
                      if p in PLATFORM_ORDER}
            if not slices:
                continue
            name = os.path.splitext(os.path.basename(path))[0]
            modules[name] = slices
        return modules

    # -- module-level helpers --
    def _namespace(self, slices):
        for p in PLATFORM_ORDER:
            if p in slices:
                return slices[p].get('namespace', 'misc')
        return 'misc'

    def _module_sample(self, module):
        sample, ext_desc = '', ''
        spath = '%s/samples/%s_samples.md' % (self.sample_folder, module)
        if os.path.exists(spath):
            with open(spath, encoding='utf-8') as f:
                sample = f.read()
        spath = '%s/samples/%s_desc.md' % (self.sample_folder, module)
        if os.path.exists(spath):
            with open(spath, encoding='utf-8') as f:
                ext_desc = f.read().strip(" \t")
        return sample, ext_desc

    # -- section renderers --
    def render_queries(self, module, slices, present):
        # Union of query names across the platforms that provide any.
        names = set()
        for p in present:
            names.update(slices[p].get('queries', {}).keys())
        if not names:
            return ''
        out = ['## Queries\n',
               'A quick reference for all available queries (check commands) in the %s module.\n' % module,
               '**List of commands:**\n',
               'A list of all available queries (check commands)\n']

        # Command list table (union; description from any present platform).
        rows = []
        for name in sorted(names):
            desc = ''
            for p in present:
                q = slices[p].get('queries', {}).get(name)
                if q:
                    desc = q.get('info', {}).get('description', '')
                    break
            if desc.startswith('Alternative name for:'):
                cell = make_rst_link(desc[22:], 'query')
            elif desc.startswith('Alias for:'):
                cell = make_rst_link(desc[11:], 'query')
            else:
                cell = first_line(desc)
            rows.append([make_md_self_link(name), cell])
        out.append(render_rst_table(rows, 'Command', 'Description'))

        # Aliases table (union).
        alias_names = set()
        for p in present:
            alias_names.update(slices[p].get('aliases', {}).keys())
        if alias_names:
            out.append('**List of command aliases:**\n')
            out.append('A list of all short hand aliases for queries (check commands)\n')
            rows = []
            for name in sorted(alias_names):
                desc = ''
                for p in present:
                    a = slices[p].get('aliases', {}).get(name)
                    if a:
                        desc = a.get('description', '')
                        break
                if desc.startswith('Alternative name for:'):
                    cell = 'Alias for: ' + make_rst_link(desc[22:], 'query')
                elif desc.startswith('Alias for:'):
                    cell = 'Alias for: ' + make_rst_link(desc[11:], 'query')
                else:
                    cell = first_line(desc)
                rows.append([name, cell])
            out.append(render_rst_table(rows, 'Command', 'Description'))

        # Per-query sections. Each content section (intro / args / filter keywords)
        # is merged across platforms independently, so the Windows/Linux tabs land
        # inside a single sub-heading instead of duplicating every heading per tab.
        for name in sorted(names):
            qpresent = [p for p in present if name in slices[p].get('queries', {})]
            out.append('### %s\n' % name)
            note = availability_note(qpresent, present)
            if note:
                out.append(note)

            prepared = {p: prepare_query(slices[p]['queries'][name], name,
                                         module, self.sample_folder)
                        for p in qpresent}
            has_sample = bool(prepared[qpresent[0]].get('sample'))
            # Select the platforms to render each section for by whether that
            # platform actually carries the section's data. This skips empty stubs
            # (e.g. a command registered on a platform but with no arguments), which
            # would otherwise produce a broken, empty content tab.
            apresent = [p for p in qpresent if prepared[p].get('params')]
            fpresent = [p for p in qpresent if prepared[p].get('fields')]

            intro = per_platform(qpresent,
                                 lambda p: self.query_intro.render(query=prepared[p]))
            args = per_platform(apresent,
                                lambda p: self.query_args.render(query=prepared[p])) \
                if apresent else ''
            filt = per_platform(fpresent,
                                lambda p: self.query_filter.render(query=prepared[p])) \
                if fpresent else ''

            out.append(self.query_layout.render(
                query=prepared[qpresent[0]],
                intro=intro.strip('\n'),
                args=args.strip('\n'),
                filter=filt.strip('\n'),
                has_sample=has_sample,
                has_args=bool(apresent),
                has_fields=bool(fpresent)))
        return '\n'.join(out)

    def render_config(self, module, slices, present):
        names = set()
        for p in present:
            names.update(slices[p].get('paths', {}).keys())
        if not names:
            return ''
        out = ['## Configuration\n']

        # Path list table (union).
        rows = []
        for pk in sorted(names):
            title = ''
            for p in present:
                path = slices[p].get('paths', {}).get(pk)
                if path:
                    title = path.get('info', {}).get('title', '')
                    break
            link = make_md_self_link(mkref(as_text(title)), pk) if title else make_md_self_link(pk)
            rows.append([link, first_line(title)])
        out.append(render_rst_table(rows, 'Path / Section', 'Description'))
        out.append('')

        for pk in sorted(names):
            ppresent = [p for p in present if pk in slices[p].get('paths', {})]
            note = availability_note(ppresent, present)
            # Subkey sections (objects + folded sample) differ rarely and have a
            # more complex shape, so fall back to whole-block content tabs there.
            # Ordinary key/value sections get key-level markers instead, so a
            # single platform-gated key (e.g. Linux-only "python lib") is flagged
            # explicitly rather than making the reader diff two tabs.
            is_subkey = any(slices[p]['paths'][pk].get('info', {}).get('subkey')
                            for p in ppresent)
            if is_subkey:
                def render_one(p, _pk=pk):
                    path = dict(slices[p]['paths'][_pk])
                    path['key'] = _pk
                    path.setdefault('keys', {})
                    path.setdefault('sample', None)
                    path.setdefault('objects', [])
                    return self.path_block.render(path=path)
                block = per_platform(ppresent, render_one)
            else:
                block = self.render_key_value_path(pk, ppresent, slices)
            if note:
                # Heading is inside the block; place the note right after it.
                lines = block.split('\n', 1)
                block = lines[0] + '\n\n' + note + ('\n' + lines[1] if len(lines) > 1 else '')
            out.append(block)
        return '\n'.join(out)

    def render_key_value_path(self, pk, ppresent, slices):
        # Render an ordinary [section]/key config block from the UNION of keys
        # across the platforms that expose this section, marking any key that is
        # not present on every one of them (or whose value differs).
        paths = {p: slices[p]['paths'][pk] for p in ppresent}
        key_names = set()
        for p in ppresent:
            key_names.update(paths[p].get('keys', {}).keys())

        def key_platforms(k):
            return [p for p in ppresent if k in paths[p].get('keys', {})]

        def marker(subset):
            if subset == ppresent or len(ppresent) < 2:
                return ''
            return ' _(%s only)_' % ', '.join(PLATFORM_LABEL[p]
                                              for p in PLATFORM_ORDER if p in subset)

        # Section heading + (possibly per-platform) description.
        title = ''
        for p in ppresent:
            title = paths[p].get('info', {}).get('title', '')
            if title:
                break
        out = ['### %s <a id="%s"></a>\n' % (title or pk, pk)]
        out.append(per_platform(ppresent, lambda p: paths[p].get('info', {}).get('description', '')))
        out.append('')

        # Key summary table (union).
        rows = []
        for k in sorted(key_names):
            kp = key_platforms(k)
            key = paths[kp[0]]['keys'][k]
            kkey = make_md_self_link(mkref(as_text(key['title'])), k)
            rows.append([kkey, key.get('default_value', ''),
                         first_line(key.get('title', '')) + marker(kp)])
        out.append(render_rst_table(rows, 'Key', 'Default Value', 'Description'))
        out.append('')

        # INI sample (union; platform-gated keys get an inline comment).
        ini = ['```ini', '# %s' % first_line(paths[ppresent[0]].get('info', {}).get('description', '')),
               '[%s]' % pk]
        for k in sorted(key_names):
            kp = key_platforms(k)
            key = paths[kp[0]]['keys'][k]
            suffix = ('  # %s only' % ', '.join(PLATFORM_LABEL[p] for p in PLATFORM_ORDER if p in kp)) \
                if kp != ppresent and len(ppresent) > 1 else ''
            if key.get('default_value'):
                ini.append('%s=%s%s' % (k, key['default_value'], suffix))
        ini.append('```')
        out.append('\n'.join(ini))
        out.append('')

        # Per-key detail blocks.
        for k in sorted(key_names):
            kp = key_platforms(k)

            def render_detail(p, _k=k):
                return self._key_detail(pk, _k, paths[p]['keys'][_k], kp, ppresent)
            out.append(per_platform(kp, render_detail))
        return '\n'.join(out)

    def _key_detail(self, pk, k, key, kp, ppresent):
        rows = [['Path:', make_md_self_link(pk, pk)], ['Key:', k]]
        if kp != ppresent and len(ppresent) > 1:
            rows.append(['Platform:', '%s only' % ', '.join(PLATFORM_LABEL[p]
                                                            for p in PLATFORM_ORDER if p in kp)])
        if key.get('advanced'):
            rows.append(['Advanced:', 'Yes (means it is not commonly used)'])
        if key.get('default_value'):
            rows.append(['Default value:', '`%s`' % key['default_value']])
        else:
            rows.append(['Default value:', '_N/A_'])
        if key.get('sample'):
            rows.append(['Sample key:', 'Yes (This section is only to show how this key is used)'])
        out = ['#### %s <a id="%s"></a>\n' % (as_text(key.get('title', '')), make_md_conf_lnk(k, pk))]
        out.append('%s\n\n' % as_text(key.get('description', '')))
        out.append(render_rst_table(rows, 'Key', 'Description'))
        out.append('\n**Sample:**\n')
        out.append('```\n[%s]\n# %s\n%s=%s\n```\n' % (pk, key.get('title', ''), k, key.get('default_value', '')))
        return '\n'.join(out)

    def render_module(self, module, slices):
        present = [p for p in PLATFORM_ORDER if p in slices]
        namespace = self._namespace(slices)
        sample, ext_desc = self._module_sample(module)

        out = ['# %s\n' % module]
        note = availability_note(present, PLATFORM_ORDER)
        if note:
            out.append(note)
        out.append(per_platform(present, lambda p: slices[p].get('info', {}).get('description', '')))
        out.append('')
        if ext_desc:
            out.append(ext_desc + '\n')

        out.append('## Enable module\n')
        out.append('To enable this module and and allow using the commands you need to ass '
                   '`%s = enabled` to the `[/modules]` section in nsclient.ini:\n' % module)
        out.append('```\n[/modules]\n%s = enabled\n```\n' % module)

        if sample:
            out.append('## Samples\n')
            out.append('_Feel free to add more samples [on this page]'
                       '(https://github.com/mickem/nscp/blob/master/docs/samples/%s_samples.md)_\n' % module)
            out.append(sample + '\n')

        out.append(self.render_queries(module, slices, present))
        out.append(self.render_config(module, slices, present))
        return namespace, '\n'.join(x for x in out if x is not None)

    def build_index_model(self, modules):
        # Flat model the index_template consumes: one entry per module with a
        # unioned queries map (name -> {key, description}).
        model = {}
        for name, slices in modules.items():
            present = [p for p in PLATFORM_ORDER if p in slices]
            namespace = self._namespace(slices)
            description = ''
            for p in present:
                description = slices[p].get('info', {}).get('description', '')
                if description:
                    break
            queries = {}
            for p in present:
                for qn, q in slices[p].get('queries', {}).items():
                    if qn not in queries:
                        queries[qn] = {'key': qn, 'description': q.get('info', {}).get('description', '')}
            model[name] = {'key': name, 'namespace': namespace,
                           'description': description, 'queries': queries}
        return model

    def generate(self, output_dir):
        modules = self.load()
        for i, (module, slices) in enumerate(sorted(modules.items()), 1):
            namespace, text = self.render_module(module, slices)
            out_path = '%s/docs/reference/%s/%s.md' % (output_dir, namespace, module)
            render_template(text, out_path)
            print('Rendered %d of %d [%s -> %s]' % (i, len(modules), module, namespace))

        index_model = self.build_index_model(modules)
        text = self.index.render(modules=index_model)
        render_template(text, '%s/docs/reference/index.md' % output_dir)
        print('Rendered %d modules into %s/docs/reference' % (len(modules), output_dir))


def main(argv):
    parser = OptionParser(prog="docs_generate")
    parser.add_option("-o", "--output", default="docs", help="Output root (Markdown -> <output>/docs/reference)")
    parser.add_option("-i", "--input", default="docs", help="Input root (YAML read from <input>/reference)")
    parser.add_option("-s", "--samples", help="Samples root (default: <output>)")
    (options, args) = parser.parse_args(args=argv)
    yaml_dir = os.path.join(options.input, 'reference')
    sample_folder = options.samples or options.output
    DocumentationGenerator(yaml_dir, sample_folder).generate(options.output)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
