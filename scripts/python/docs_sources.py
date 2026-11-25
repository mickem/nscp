#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Validate the per-command data-source sidecars (docs/samples/*_sources.yaml).

Next to ``<Module>_<command>_desc.md`` and ``_samples.md`` a check can carry a
structured ``<Module>_<command>_sources.yaml`` stating where it reads its data
from, what privilege that needs and what it depends on. docs_generate.py renders
it as a "Data sources and privileges" section on the command's reference page
and collects every sidecar on reference/data-sources.md; this module holds the
schema, the loader and the validator both of them (and CI) use.

Schema (the top level names one or more platforms)::

    all | windows | linux | macos:        # all = the same on every platform the check runs on
      - source: /sys/class/power_supply   # required: where the data comes from (inline markdown)
        provides: charge, power source    # optional: which data / keywords come from it
        privilege: none                   # optional: none | elevated | conditional
        note: ...                         # optional; required for conditional (the condition)
        without: ...                      # optional: what happens without the privilege/dependency
        requires: [...]                   # optional: dependencies (tool, service, package, ...)

``privilege`` is about the account the *agent* runs as: ``elevated`` means
Administrator / LocalSystem on Windows and root on Linux and macOS. A missing
``privilege`` means "not determined" - never "none".

Run ``python3 scripts/python/docs_sources.py --check`` to validate every sidecar
and to flag one whose ``<Module>_<command>`` names no known command.
"""
from __future__ import print_function

import glob
import io
import json
import os
import re
import sys

import yaml

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_DIR = os.path.dirname(os.path.dirname(SCRIPT_DIR))  # <repo>/scripts/python -> <repo>
SAMPLES_DIR = os.path.join(REPO_DIR, 'docs', 'samples')
REFERENCE_DIR = os.path.join(REPO_DIR, 'docs', 'reference')
MODULES_DIR = os.path.join(REPO_DIR, 'modules')

SUFFIX = '_sources.yaml'
# Module names are CamelCase without an underscore, so the first underscore
# splits the module from the command (whose name usually has several).
FILENAME_RE = re.compile(r'^([A-Za-z0-9]+)_([A-Za-z0-9][A-Za-z0-9_\-]*)' + re.escape(SUFFIX) + '$')

# Top-level platform keys, in display order, and the docs_extract.py slice each
# one corresponds to (`all` stands for every slice the command is on).
PLATFORMS = ['all', 'windows', 'linux', 'macos']
PLATFORM_LABEL = {'all': 'All', 'windows': 'Windows', 'linux': 'Linux', 'macos': 'macOS'}
PLATFORM_SLICE = {'windows': 'windows', 'linux': 'unix', 'macos': 'darwin'}

PRIVILEGES = ['none', 'conditional', 'elevated']  # ascending: the overview ranks by it
STRING_KEYS = ['source', 'provides', 'privilege', 'note', 'without']
ENTRY_KEYS = STRING_KEYS + ['requires']
NOT_DETERMINED = 'not determined'


class SourcesError(Exception):
    def __init__(self, errors):
        self.errors = list(errors)
        Exception.__init__(self, '\n'.join(self.errors))


def sidecar_name(module, command):
    return '%s_%s%s' % (module, command, SUFFIX)


def parse_filename(name):
    """(module, command) for a sidecar file name, or None when it is not one."""
    m = FILENAME_RE.match(os.path.basename(name))
    return (m.group(1), m.group(2)) if m else None


def _is_text(value):
    return isinstance(value, str) and value.strip() != ''


def validate(data):
    """Every schema violation in a parsed sidecar, as a list of messages."""
    errors = []
    if not isinstance(data, dict) or not data:
        return ['the top level must be a mapping naming at least one of: %s' % ', '.join(PLATFORMS)]
    for platform, entries in data.items():
        if platform not in PLATFORMS:
            errors.append('unknown platform %r (expected one of: %s)' % (platform, ', '.join(PLATFORMS)))
            continue
        if not isinstance(entries, list) or not entries:
            errors.append('%s: must be a non-empty list of sources' % platform)
            continue
        for i, entry in enumerate(entries, 1):
            where = '%s[%d]' % (platform, i)
            if not isinstance(entry, dict):
                errors.append('%s: must be a mapping with at least a "source"' % where)
                continue
            for key in entry:
                if key not in ENTRY_KEYS:
                    errors.append('%s: unknown key %r (expected: %s)' % (where, key, ', '.join(ENTRY_KEYS)))
            for key in STRING_KEYS:
                if key in entry and not _is_text(entry[key]):
                    errors.append('%s: %s must be a non-empty string, not %r' % (where, key, entry[key]))
            if 'source' not in entry:
                errors.append('%s: "source" is required' % where)
            privilege = entry.get('privilege')
            if _is_text(privilege) and privilege not in PRIVILEGES:
                errors.append('%s: unknown privilege %r (expected one of: %s; leave it out when not determined)'
                              % (where, privilege, ', '.join(PRIVILEGES)))
            if privilege == 'conditional' and 'note' not in entry:
                errors.append('%s: privilege "conditional" needs a "note" stating the condition' % where)
            if 'requires' in entry:
                req = entry['requires']
                if not isinstance(req, list) or not req:
                    errors.append('%s: requires must be a non-empty list of strings' % where)
                else:
                    for item in req:
                        if not _is_text(item):
                            errors.append('%s: requires entries must be non-empty strings, not %r' % (where, item))
    return errors


def load_file(path):
    """Parse and validate one sidecar; raises SourcesError naming every problem."""
    name = os.path.basename(path)
    try:
        with io.open(path, encoding='utf-8') as f:
            data = yaml.safe_load(f)
    except yaml.YAMLError as e:
        raise SourcesError(['%s: not valid YAML: %s' % (name, ' '.join(str(e).split()))])
    errors = validate(data)
    if errors:
        raise SourcesError('%s: %s' % (name, e) for e in errors)
    return data


def load_for(sample_folder, module, command):
    """The validated sidecar of a command, or None when it has none."""
    path = os.path.join(sample_folder, 'samples', sidecar_name(module, command))
    if not os.path.exists(path):
        return None
    return load_file(path)


def entries(data):
    """(platform, entry) pairs in display order."""
    for platform in PLATFORMS:
        for entry in data.get(platform, []):
            yield platform, entry


def rank_privilege(values):
    """Summarise the privileges of several sources: the most demanding one wins,
    but a single undetermined source makes "none" undetermined (it might not be)."""
    known = [v for v in values if v in PRIVILEGES]
    top = max(known, key=PRIVILEGES.index) if known else None
    if top in ('elevated', 'conditional'):
        return top
    if len(known) != len(values):
        return None
    return top


# --- Known commands --------------------------------------------------------------


def _expand_queries(data):
    names = set()
    for section in ['common'] + list(data.get('platforms', []) or []):
        names.update(((data.get(section) or {}).get('queries') or {}).keys())
    return names


def known_commands(reference_dir=REFERENCE_DIR, modules_dir=MODULES_DIR):
    """{module: set(commands)} from the extracted reference (docs/reference/*.yaml)
    and the modules' own module.json, so a command added since the last extraction
    is known too. module.json is keyed by the module name it declares, which is
    how CheckSystemUnix documents its commands under CheckSystem."""
    known = {}
    for path in glob.glob(os.path.join(reference_dir, '*.yaml')):
        if os.path.basename(path) == 'common-options.yaml':
            continue
        with io.open(path, encoding='utf-8') as f:
            data = yaml.safe_load(f) or {}
        module = os.path.splitext(os.path.basename(path))[0]
        known.setdefault(module, set()).update(_expand_queries(data))
    for path in glob.glob(os.path.join(modules_dir, '*', 'module.json')):
        try:
            with io.open(path, encoding='utf-8') as f:
                data = json.load(f)
        except ValueError:
            continue
        module = (data.get('module') or {}).get('name') or os.path.basename(os.path.dirname(path))
        commands = data.get('commands') or {}
        known.setdefault(module, set()).update(commands.keys() if isinstance(commands, dict) else commands)
    return known


def check(samples_dir=SAMPLES_DIR, reference_dir=REFERENCE_DIR, modules_dir=MODULES_DIR):
    """(errors, count): every problem in every sidecar under samples_dir."""
    errors = []
    known = known_commands(reference_dir, modules_dir)
    paths = sorted(glob.glob(os.path.join(samples_dir, '*' + SUFFIX)))
    for path in paths:
        name = os.path.basename(path)
        parsed = parse_filename(name)
        if not parsed:
            errors.append('%s: not named <Module>_<command>%s' % (name, SUFFIX))
        else:
            module, command = parsed
            if module not in known:
                errors.append('%s: no module named %s (in docs/reference or modules/*/module.json)' % (name, module))
            elif command not in known[module]:
                errors.append('%s: %s has no command named %s' % (name, module, command))
        try:
            load_file(path)
        except SourcesError as e:
            errors.extend(e.errors)
    return errors, len(paths)


def main(argv):
    import argparse
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--check', action='store_true',
                        help='validate every sidecar and exit (the default action)')
    parser.add_argument('--samples', default=SAMPLES_DIR, help='directory holding the sidecars (docs/samples)')
    parser.add_argument('--reference', default=REFERENCE_DIR, help='extracted reference YAML (docs/reference)')
    parser.add_argument('--modules', default=MODULES_DIR, help='module sources holding module.json (modules)')
    args = parser.parse_args(argv)
    errors, count = check(args.samples, args.reference, args.modules)
    for e in errors:
        print('error: %s' % e, file=sys.stderr)
    if errors:
        print('%d data-source sidecars, %d errors' % (count, len(errors)), file=sys.stderr)
        return 1
    print('%d data-source sidecars: OK' % count, file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
