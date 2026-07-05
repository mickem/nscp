#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Documentation extractor (step 1 of 2).
#
# Runs *inside* nscp via the PythonScript module so it can introspect the loaded
# modules over protobuf (registry + settings inventory). It serializes what it
# finds into a git-committed, per-module YAML intermediate under docs/reference/.
#
# The YAML is keyed by platform tag ("unix" / "windows"); each run rewrites ONLY
# the slice for the platform it runs on and leaves the other platform's slice
# untouched. This lets the Linux and Windows pipelines update their own data
# independently with clean, order-independent diffs. Rendering is a separate,
# platform-agnostic step (docs_generate.py) that reads these files.
#
# All path-variable resolution (unexpand of default values) happens here, so the
# generator never needs a running nscp.
from NSCP import Settings, Registry, Core, log, log_debug, log_error, status
import registry_pb2
import settings_pb2
from optparse import OptionParser
import os
import sys
import yaml

helper = None

# --- Static, platform-independent module -> namespace classification. ----------
# (docs.py picked windows vs unix at runtime from sys.platform; here the mapping
# is fixed so a module always lands in the same reference folder regardless of
# which platform's pipeline extracted it. Cross-platform "system" modules keep
# their historical windows/ home; CheckSystemUnix is the only unix-only one.)
WINDOWS_MODULES = ['CheckSystem', 'CheckDisk', 'NSClientServer', 'DotnetPlugins',
                   'CheckEventLog', 'CheckTaskSched', 'CheckWMI']
UNIX_MODULES = ['CheckSystemUnix']
CHECK_MODULES = ['CheckExternalScripts', 'CheckHelpers', 'CheckLogFile', 'CheckMKClient',
                 'CheckMKServer', 'CheckNSCP', 'CheckNet', 'CheckSecurity']
CLIENT_MODULES = ['GraphiteClient', 'IcingaClient', 'NRDPClient', 'NRPEClient', 'NRPEServer',
                  'NSCAClient', 'NSCANgClient', 'NSCAServer', 'NSClientServer', 'SMTPClient',
                  'SyslogClient']
GENERIC_MODULES = ['CommandClient', 'DotnetPlugins', 'LUAScript', 'PythonScript', 'Scheduler',
                   'SimpleCache', 'SimpleFileWriter', 'WEBServer']
IGNORED_MODULES = ['CauseCrashes', 'SamplePluginSimple']


def classify_namespace(name):
    # Order matters: the "system" modules must win over client/generic where a
    # name appears in more than one list (NSClientServer, DotnetPlugins).
    if name in UNIX_MODULES:
        return 'unix'
    if name in WINDOWS_MODULES:
        return 'windows'
    if name in CHECK_MODULES:
        return 'check'
    if name in CLIENT_MODULES:
        return 'client'
    if name in GENERIC_MODULES:
        return 'generic'
    return 'misc'


# --- Known path variables resolved via core.expand_path. Mirrors the fixed set
# in service/path_manager.cpp get_path_for_key -- unknown keys there fall back to
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
    # path_cache is a list of (key, expanded) pre-sorted longest-first so the most
    # specific prefix wins (e.g. ${certificate-path} before ${shared-path}).
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


# --- Container tree (same shape as docs.py; used only to fold subkey paths). ----
class root_container(object):
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
                    if pk[len(k) + 1:] == "sample":
                        self.paths[k].sample = self.paths[pk]
                    if pk[len(k) + 1:] == "default":
                        self.paths[k].default = self.paths[pk]
                    del self.paths[pk]

    def append_path(self, info):
        path = info.node.path
        if path not in self.paths:
            self.paths[path] = path_container(info)
        if info.info.subkey:
            if path not in self.subkeys.keys():
                self.subkeys[path] = self.paths[path]

    def append_command(self, info):
        name = info.name
        if name not in self.commands:
            self.commands[name] = command_container(info)

    def append_alias(self, info):
        name = info.name
        if name not in self.commands:
            self.aliases[name] = command_container(info)

    def append_plugin(self, info):
        name = info.name
        if name in IGNORED_MODULES:
            return
        if name not in self.plugins:
            self.plugins[name] = plugin_container(info)


class path_container(object):
    def __init__(self, info=None):
        self.keys = {}
        self.info = info.info if info else None
        self.default = None
        self.sample = None
        self.objects = {}

    def append_key(self, info):
        self.keys[info.node.key] = info


class command_container(object):
    def __init__(self, info=None):
        self.info = info.info
        self.parameters = info.parameters


class plugin_container(object):
    def __init__(self, info=None):
        self.info = info.info


# --- Serialization: container tree -> plain dicts (JSON/YAML-safe). ------------
def ser_param(p, unexpand):
    long_desc = p.long_description
    d = {
        'name': p.name,
        'default_value': unexpand(p.default_value),
        'short_description': p.short_description,
        'long_description': long_desc,
        'required': bool(p.required),
        'repeatable': bool(p.repeatable),
        'content_type': int(p.content_type),
        # Mirrors docs.py param_container: "simple" params render inline in the
        # options table, others get a dedicated detail block.
        'is_simple': not (p.default_value or '\n' in long_desc),
    }
    keywords = []
    for kw in p.keyword:
        keywords.append({
            'parameter': kw.parameter,
            'context': kw.context,
            'key': kw.key,
            'short_description': kw.short_description,
            'long_description': kw.long_description,
        })
    if keywords:
        d['keyword'] = keywords
    return d


def ser_field(f):
    return {
        'name': f.name,
        'short_description': f.short_description,
        'long_description': f.long_description,
        # docs.py splits fields into generic (common to every check) vs own.
        'generic': f.long_description.endswith('Common option for all checks.'),
    }


def ser_command(cinfo, unexpand):
    d = {'info': {'description': cinfo.info.description}}
    params = [ser_param(p, unexpand) for p in cinfo.parameters.parameter]
    if params:
        d['params'] = params
    fields = [ser_field(f) for f in cinfo.parameters.fields]
    if fields:
        d['fields'] = fields
    return d


def ser_key(kinfo, unexpand):
    return {
        'title': kinfo.info.title,
        'description': kinfo.info.description,
        'default_value': unexpand(kinfo.info.default_value),
        'advanced': bool(kinfo.info.advanced),
        'sample': bool(kinfo.info.sample),
    }


def ser_path(pc, unexpand):
    d = {
        'info': {
            'title': pc.info.title,
            'description': pc.info.description,
            'subkey': bool(pc.info.subkey),
            'advanced': bool(pc.info.advanced),
        },
    }
    keys = {}
    for k, kinfo in pc.keys.items():
        keys[k] = ser_key(kinfo, unexpand)
    if keys:
        d['keys'] = keys
    if pc.sample:
        skeys = {}
        for k, kinfo in pc.sample.keys.items():
            skeys[k] = ser_key(kinfo, unexpand)
        d['sample'] = {'keys': skeys}
    if pc.objects:
        d['objects'] = sorted(pc.objects.keys())
    return d


def serialize_module(root, module, minfo, unexpand):
    slice = {
        'module': module,
        'namespace': classify_namespace(module),
        'info': {
            'title': minfo.info.title,
            'description': minfo.info.description,
        },
    }
    queries = {}
    for (c, cinfo) in sorted(root.commands.items()):
        if cinfo.info.description.startswith('Legacy version of'):
            continue
        if module in cinfo.info.plugin:
            queries[c] = ser_command(cinfo, unexpand)
    if queries:
        slice['queries'] = queries

    aliases = {}
    for (c, cinfo) in sorted(root.aliases.items()):
        if module in cinfo.info.plugin:
            aliases[c] = {'description': cinfo.info.description}
    if aliases:
        slice['aliases'] = aliases

    paths = {}
    for (c, pc) in sorted(root.paths.items()):
        if module in pc.info.plugin:
            paths[c] = ser_path(pc, unexpand)
    if paths:
        slice['paths'] = paths
    return slice


# --- Platform factoring: dedup identical data into a shared "common" section. ---
# The on-disk file stores, per module:
#   module/namespace   -- platform-invariant, top level
#   platforms: [...]   -- which platforms have been extracted
#   common:  {info?, queries?, aliases?, paths?}  -- items present & equal on ALL
#   unix:    {...}     -- items unique to unix, or that differ across platforms
#   windows: {...}     -- likewise for windows
# expand() rebuilds each platform's full tree; factor() is its inverse. Because
# the file always carries enough to reconstruct every platform, a single-platform
# run reconstructs the OTHER platform from disk, re-compares against its own fresh
# data and re-factors -- so Linux and Windows can update independently and still
# converge. KEEP expand()/_merge_section IN SYNC with docs_generate.py.
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
    # Reconstruct {platform: full_tree} for every platform the file records.
    return {p: expand_platform(data, p) for p in data.get('platforms', [])}


def factor(trees):
    # Inverse of expand: split {platform: full_tree} into common + per-platform
    # overrides. An item is "common" only when present and equal on every platform
    # (and there is more than one -- a lone platform has nothing to agree with).
    platforms = sorted(trees)
    base = trees[platforms[0]]
    data = {'platforms': platforms, 'module': base.get('module'),
            'namespace': base.get('namespace')}
    common = {}
    ov = {p: {} for p in platforms}
    multi = len(platforms) >= 2

    infos = {p: trees[p].get('info') for p in platforms}
    if multi and all(infos[p] == infos[platforms[0]] for p in platforms):
        common['info'] = infos[platforms[0]]
    else:
        for p in platforms:
            if infos[p] is not None:
                ov[p]['info'] = infos[p]

    for section in FACTOR_SECTIONS:
        names = set()
        for p in platforms:
            names.update(trees[p].get(section, {}).keys())
        csec = {}
        osec = {p: {} for p in platforms}
        for name in sorted(names):
            holders = [p for p in platforms if name in trees[p].get(section, {})]
            vals = {p: trees[p][section][name] for p in holders}
            if multi and holders == platforms and all(vals[p] == vals[platforms[0]] for p in holders):
                csec[name] = vals[platforms[0]]
            else:
                for p in holders:
                    osec[p][name] = vals[p]
        if csec:
            common[section] = csec
        for p in platforms:
            if osec[p]:
                ov[p][section] = osec[p]

    if common:
        data['common'] = common
    for p in platforms:
        if ov[p]:
            data[p] = ov[p]
    return data


def write_module_yaml(yaml_dir, module, platform, fresh_tree):
    path = os.path.join(yaml_dir, '%s.yaml' % module)
    data = {}
    if os.path.exists(path):
        with open(path, encoding='utf-8') as f:
            data = yaml.safe_load(f) or {}
    trees = expand(data)          # reconstruct whatever platforms are on disk
    trees[platform] = fresh_tree  # replace this platform with freshly extracted data
    data = factor(trees)          # re-dedup into common + per-platform overrides
    text = yaml.safe_dump(data, sort_keys=True, default_flow_style=False,
                          allow_unicode=True, width=100)
    if os.path.exists(path):
        with open(path, encoding='utf-8') as f:
            if f.read() == text:
                log_debug('no changes detected in: %s' % path)
                return
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
    log_debug('wrote %s slice for %s -> %s' % (platform, module, path))


class DocumentationExtractor(object):
    def __init__(self, plugin_id, plugin_alias, script_alias):
        self.plugin_id = plugin_id
        self.plugin_alias = plugin_alias
        self.script_alias = script_alias
        self.conf = Settings.get(self.plugin_id)
        self.registry = Registry.get(self.plugin_id)
        self.core = Core.get(self.plugin_id)

    def build_inventory_request(self, path='/', recursive=True, keys=False):
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

    def build_command_request(self, type=1):
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
                    log_debug('Found %d paths' % len(payload.inventory))
                    return payload.inventory
        return []

    def get_keys(self, path):
        (code, data) = self.conf.query(self.build_inventory_request(path, False, True))
        if code == 1:
            message = settings_pb2.SettingsResponseMessage()
            message.ParseFromString(data)
            for payload in message.payload:
                if payload.inventory:
                    log_debug('Found %d keys for %s' % (len(payload.inventory), path))
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
                    log_debug('Found %d queries' % len(payload.inventory))
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
                    log_debug('Found %d aliases' % len(payload.inventory))
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
                    log_debug('Found %d plugins' % len(payload.inventory))
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
        root.process_paths()
        for p in self.get_queries():
            root.append_command(p)
        for p in self.get_query_aliases():
            root.append_alias(p)
        return root

    def build_path_cache(self):
        # Resolve every known path variable once via core.expand_path so default
        # values (c:\Program Files\..., /etc/...) get rewritten to ${var}/... form
        # and baked into the YAML -- the generator then needs no running nscp.
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

    def extract(self, output_dir):
        platform = 'windows' if sys.platform == 'win32' else 'unix'
        yaml_dir = os.path.join(output_dir, 'reference')
        if not os.path.exists(yaml_dir):
            os.makedirs(yaml_dir)

        path_cache = self.build_path_cache()
        log_debug('Resolved %d path variables for default-value rewriting' % len(path_cache))
        unexpand = make_unexpand_path(path_cache)

        root = self.get_info()
        i = 0
        for module, minfo in sorted(root.plugins.items()):
            slice = serialize_module(root, module, minfo, unexpand)
            write_module_yaml(yaml_dir, module, platform, slice)
            i += 1
            log_debug('Extracted module %d of %d [%s]' % (i, len(root.plugins), module))
        log('Extracted %d modules (%s slice) into %s' % (i, platform, yaml_dir))

    def main(self, args):
        parser = OptionParser(prog="")
        parser.add_option("-o", "--output", help="Output root (YAML written to <output>/reference)")
        parser.add_option("-i", "--input", help="Reference folder (unused; kept for CLI parity)")
        (options, args) = parser.parse_args(args=args)
        self.extract(options.output)


def __main__(args):
    global helper
    helper.main(args)
    return 0


def init(plugin_id, plugin_alias, script_alias):
    global helper
    helper = DocumentationExtractor(plugin_id, plugin_alias, script_alias)


def shutdown():
    global helper
    helper = None
