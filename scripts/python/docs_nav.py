#!/usr/bin/env python3
"""Keep the reference's module navigation in docs/mkdocs.yml in step with the
modules' own category (modules/<Module>/module.json "category").

The "Reference" part of the navigation lists every module under one of three
headings. Which heading used to be a hand-kept list; it now follows the
category each module declares:

=================  ==========================================
Heading            Categories
=================  ==========================================
Check commands     check, database, container
Clients & Servers  server, client
Helper modules     script, scheduling, tool
=================  ==========================================

A module is listed when the reference documents it (docs/reference/<Module>.yaml,
which also gives the folder its page lives in and the platforms it runs on;
a Windows-only module is labelled so). Within a heading the cross-platform
modules come first, then the Windows-only ones, each alphabetically.

    python3 scripts/python/docs_nav.py --check   # fail if mkdocs.yml is stale
    python3 scripts/python/docs_nav.py --write   # rewrite the three lists
"""
from __future__ import print_function

import io
import json
import os
import re
import sys

import yaml

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
MODULES_DIR = os.path.join(ROOT, 'modules')
REFERENCE_DIR = os.path.join(ROOT, 'docs', 'reference')
MKDOCS = os.path.join(ROOT, 'docs', 'mkdocs.yml')

# Keep in step with build/python/create_plugin_module.py (MODULE_CATEGORIES)
# and web/src/modules/groups.ts.
SECTIONS = [
    ('Check commands', ('check', 'database', 'container')),
    ('Clients & Servers', ('server', 'client')),
    ('Helper modules', ('script', 'scheduling', 'tool')),
]
CATEGORIES = [c for _, cats in SECTIONS for c in cats]


class NavError(Exception):
    pass


def read_modules():
    """[(module, category)] for every module with a module.json."""
    out = []
    for name in sorted(os.listdir(MODULES_DIR)):
        path = os.path.join(MODULES_DIR, name, 'module.json')
        if not os.path.isfile(path):
            continue
        with io.open(path, encoding='utf-8') as f:
            category = json.load(f).get('module', {}).get('category')
        if category not in CATEGORIES:
            raise NavError('%s: module.json declares category %r; use one of %s'
                           % (name, category, ', '.join(CATEGORIES)))
        out.append((name, category))
    return out


def entries(section_categories, modules):
    """The '- Label: path' lines of one heading, in order."""
    rows = []
    for name, category in modules:
        if category not in section_categories:
            continue
        ref = os.path.join(REFERENCE_DIR, '%s.yaml' % name)
        if not os.path.isfile(ref):
            continue  # Not documented (a sample, a crash tester, ...).
        with io.open(ref, encoding='utf-8') as f:
            data = yaml.safe_load(f) or {}
        namespace = data.get('namespace') or 'misc'
        windows_only = (data.get('platforms') or []) == ['windows']
        label = '%s (Windows)' % name if windows_only else name
        rows.append((windows_only, name.lower(), '%s: reference/%s/%s.md' % (label, namespace, name)))
    return [r[2] for r in sorted(rows)]


def render(text, modules):
    """mkdocs.yml with each heading's list replaced by the generated one."""
    for title, cats in SECTIONS:
        pattern = re.compile(r'^(?P<indent>[ ]*)- %s:[ ]*\n(?P<items>(?:(?P=indent)[ ]+- .*\n)*)' % re.escape(title),
                             re.M)
        m = pattern.search(text)
        if not m:
            raise NavError('docs/mkdocs.yml has no "- %s:" heading to fill' % title)
        indent = m.group('indent') + '  '
        items = ''.join('%s- %s\n' % (indent, e) for e in entries(cats, modules))
        text = text[:m.start('items')] + items + text[m.end('items'):]
    return text


def main(argv):
    if len(argv) != 2 or argv[1] not in ('--check', '--write'):
        print(__doc__)
        return 2
    try:
        modules = read_modules()
        with io.open(MKDOCS, encoding='utf-8') as f:
            current = f.read()
        wanted = render(current, modules)
    except NavError as e:
        print('error: %s' % e)
        return 1
    if argv[1] == '--write':
        if wanted != current:
            with io.open(MKDOCS, 'w', encoding='utf-8') as f:
                f.write(wanted)
            print('Rewrote the module navigation in docs/mkdocs.yml')
        else:
            print('docs/mkdocs.yml is up to date')
        return 0
    if wanted != current:
        print('error: the module navigation in docs/mkdocs.yml does not follow the modules\' categories; '
              'run: python3 scripts/python/docs_nav.py --write')
        return 1
    print('%d modules, navigation up to date' % len(modules))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
