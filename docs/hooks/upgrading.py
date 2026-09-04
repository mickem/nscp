#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Assemble the "Upgrading" page from one file per upgrade note.

Every upgrade note lives in its own file under ``docs/upgrades/<version>/``
(see ``docs/upgrades/README.md`` for the format), so two branches that each
add a note never touch the same file and never conflict. This module merges
them into ``docs/docs/setup/upgrading.md`` at build time:

* as an **mkdocs hook** (``hooks: [hooks/upgrading.py]`` in ``mkdocs.yml``)
  it expands the ``<!-- upgrades:filter -->`` and ``<!-- upgrades:entries -->``
  markers of the committed stub page while the site is built or served;
* as a **command-line tool** it validates the notes (``--check``) or writes
  the fully expanded page (``--render``) for a build that does not run
  mkdocs from this repository.

Each note is rendered as a bullet (``- <icon> **Title.** text``) wrapped in a
``<div class="upgrade-entry" data-modules=... data-security=...>`` and every
version in a ``<div class="upgrade-version" data-version=...>``; the page's
``js/upgrading.js`` uses those attributes to let a reader hide the notes that
do not concern the modules they run.
"""
from __future__ import print_function

import glob
import io
import os
import re
import sys

import yaml

DOCS_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # <repo>/docs
REPO_DIR = os.path.dirname(DOCS_DIR)
UPGRADES_DIR = os.path.join(DOCS_DIR, 'upgrades')
MODULES_DIR = os.path.join(REPO_DIR, 'modules')
REFERENCE_DIR = os.path.join(DOCS_DIR, 'docs', 'reference')
STUB_PAGE = os.path.join(DOCS_DIR, 'docs', 'setup', 'upgrading.md')

PAGE_SRC_URI = 'setup/upgrading.md'
MARKER_FILTER = '<!-- upgrades:filter -->'
MARKER_ENTRIES = '<!-- upgrades:entries -->'

# Tags that are not modules: cross-cutting areas a note can be filed under.
# Notes tagged with one of these are shown to everyone by default, whatever
# modules are selected, because every installation is affected.
AREAS = [
    ('core', 'Core: service, settings, command line'),
    ('filters', 'Filter framework (affects every check)'),
    ('packaging', 'Packaging, installers and dependencies'),
    ('docs', 'Documentation'),
]
AREA_NAMES = [name for name, _ in AREAS]

SECURITY_ICON = u'\U0001F512'  # 🔒

# Display groups for the module picker, in this order.
GROUP_CHECKS = 'Check modules'
GROUP_CLIENTS = 'Clients and servers'
GROUP_HELPERS = 'Helper modules'
GROUP_GENERAL = 'General'
GROUP_ORDER = [GROUP_CHECKS, GROUP_CLIENTS, GROUP_HELPERS, GROUP_GENERAL]


class UpgradeNoteError(Exception):
    """A note is malformed: the message names the file and the problem."""


# --- reading ---------------------------------------------------------------------


def known_modules():
    """Names a note's ``modules:`` may use, besides the areas above.

    Taken from the module directories of the source tree when the hook runs
    inside the repository, else from the reference pages (a docs-only checkout).
    """
    names = set()
    if os.path.isdir(MODULES_DIR):
        for entry in os.listdir(MODULES_DIR):
            if os.path.isfile(os.path.join(MODULES_DIR, entry, 'module.json')):
                names.add(entry)
    if not names:
        for path in glob.glob(os.path.join(REFERENCE_DIR, '*', '*.md')):
            names.add(os.path.splitext(os.path.basename(path))[0])
    return names


def version_key(version):
    """Sort key for ``0.18.1``-style versions (numeric, part by part)."""
    return tuple(int(p) for p in version.split('.'))


def parse_note(path):
    with io.open(path, encoding='utf-8') as f:
        text = f.read()
    if not text.startswith('---\n'):
        raise UpgradeNoteError('%s: missing front matter (the file must start with ---)' % path)
    try:
        _, front, body = text.split('---\n', 2)
    except ValueError:
        raise UpgradeNoteError('%s: unterminated front matter' % path)
    try:
        meta = yaml.safe_load(front) or {}
    except yaml.YAMLError as e:
        raise UpgradeNoteError('%s: invalid front matter: %s' % (path, e))
    if not isinstance(meta, dict):
        raise UpgradeNoteError('%s: front matter must be a mapping' % path)

    modules = meta.get('modules')
    if isinstance(modules, str):
        modules = [m.strip() for m in modules.split(',') if m.strip()]
    if not modules or not isinstance(modules, list):
        raise UpgradeNoteError('%s: "modules:" must list at least one module or area' % path)
    modules = [str(m) for m in modules]

    icon = meta.get('icon') or ''
    if not isinstance(icon, str):
        raise UpgradeNoteError('%s: "icon:" must be a string' % path)
    icon = icon.strip()

    security = meta.get('security')
    if security is None:
        security = SECURITY_ICON in icon
    if not isinstance(security, bool):
        raise UpgradeNoteError('%s: "security:" must be true or false' % path)

    body = body.strip('\n')
    if not body.strip():
        raise UpgradeNoteError('%s: the note has no text' % path)
    first = body.split('\n', 1)[0]
    if '**' not in first:
        raise UpgradeNoteError('%s: the first line must carry a bold **title** (see docs/upgrades/README.md)' % path)

    return {
        'path': path,
        'name': os.path.basename(path),
        'icon': icon,
        'modules': modules,
        'security': security,
        'body': body,
    }


def load_notes(upgrades_dir=UPGRADES_DIR, valid_modules=None):
    """Return ``[(version, [note, ...]), ...]`` newest version first.

    Notes within a version are ordered by file name; the migrated history uses
    a ``NNN-`` prefix to keep its original order and unnumbered files sort
    after those, alphabetically.
    """
    if valid_modules is None:
        valid_modules = known_modules()
    valid = set(valid_modules) | set(AREA_NAMES)
    versions = []
    for entry in os.listdir(upgrades_dir):
        vdir = os.path.join(upgrades_dir, entry)
        if not os.path.isdir(vdir):
            continue
        if not re.match(r'^\d+(\.\d+)*$', entry):
            raise UpgradeNoteError('%s: directory name is not a version number' % vdir)
        notes = []
        for name in sorted(os.listdir(vdir)):
            if not name.endswith('.md'):
                continue
            note = parse_note(os.path.join(vdir, name))
            unknown = [m for m in note['modules'] if m not in valid]
            if unknown:
                raise UpgradeNoteError('%s: unknown module(s) %s (expected a module directory name or one of: %s)'
                                       % (note['path'], ', '.join(unknown), ', '.join(AREA_NAMES)))
            note['version'] = entry
            notes.append(note)
        if not notes:
            raise UpgradeNoteError('%s: version directory has no notes' % vdir)
        versions.append((entry, notes))
    versions.sort(key=lambda v: version_key(v[0]), reverse=True)
    return versions


# --- rendering -------------------------------------------------------------------


def html_attr(value):
    return value.replace('&', '&amp;').replace('"', '&quot;').replace('<', '&lt;')


def render_note(note):
    lines = note['body'].split('\n')
    head = '- ' + (note['icon'] + ' ' if note['icon'] else '') + lines[0]
    # Continuation lines are indented four spaces so nested lists, tables and
    # fenced code blocks stay inside the list item.
    rest = [('    ' + l) if l.strip() else '' for l in lines[1:]]
    attrs = ' data-modules="%s"' % html_attr(' '.join(note['modules']))
    if note['security']:
        attrs += ' data-security="1"'
    return ('<div class="upgrade-entry" markdown="1"%s>\n\n%s\n\n</div>'
            % (attrs, '\n'.join([head] + rest)))


def render_entries(versions):
    out = []
    for version, notes in versions:
        out.append('<div class="upgrade-version" markdown="1" data-version="%s">\n' % html_attr(version))
        out.append('## %s\n' % version)
        out.extend(render_note(n) + '\n' for n in notes)
        out.append('</div>\n')
    return '\n'.join(out)


def module_group(name):
    if name in AREA_NAMES:
        return GROUP_GENERAL
    if name.startswith('Check'):
        return GROUP_CHECKS
    if name.endswith('Client') or name.endswith('Server'):
        return GROUP_CLIENTS
    return GROUP_HELPERS


def render_filter(versions):
    """The filter form. Hidden until ``js/upgrading.js`` enables it, so a
    reader without JavaScript simply sees the whole page."""
    used = set()
    for _, notes in versions:
        for n in notes:
            used.update(n['modules'])
    groups = {}
    for m in used:
        groups.setdefault(module_group(m), []).append(m)
    area_label = dict(AREAS)

    out = ['<div class="upgrade-filter" id="upgrade-filter" hidden>']
    out.append('<div class="upgrade-filter__row">')
    out.append('<label>Upgrading from <select id="upgrade-from"><option value="">any version</option>%s</select></label>'
               % ''.join('<option value="%s">%s</option>' % (v, v) for v, _ in versions))
    out.append('<label>to <select id="upgrade-to"><option value="">latest</option>%s</select></label>'
               % ''.join('<option value="%s">%s</option>' % (v, v) for v, _ in versions))
    out.append('<label><input type="checkbox" id="upgrade-security"> %s security-relevant only</label>' % SECURITY_ICON)
    out.append('</div>')
    out.append('<details class="upgrade-filter__modules" id="upgrade-modules">')
    out.append('<summary>Modules: <span id="upgrade-modules-summary">all</span></summary>')
    out.append('<div class="upgrade-filter__actions">'
               '<button type="button" data-action="all">Select all</button> '
               '<button type="button" data-action="none">Select none</button> '
               '<button type="button" data-action="reset">Reset</button></div>')
    out.append('<p class="upgrade-filter__hint">Tick the modules your <code>nsclient.ini</code> enables. '
               'Notes that touch none of the ticked modules are hidden; the <em>General</em> group covers '
               'changes that affect every installation.</p>')
    out.append('<div class="upgrade-filter__groups">')
    for group in GROUP_ORDER:
        names = groups.get(group)
        if not names:
            continue
        if group == GROUP_GENERAL:
            names = [a for a in AREA_NAMES if a in names]
        else:
            names = sorted(names)
        out.append('<fieldset><legend>%s</legend>' % group)
        for m in names:
            title = area_label.get(m, m)
            out.append('<label title="%s"><input type="checkbox" name="module" value="%s" checked> %s</label>'
                       % (html_attr(title), html_attr(m), html_attr(m)))
        out.append('</fieldset>')
    out.append('</div>')
    out.append('</details>')
    out.append('<p class="upgrade-filter__status" id="upgrade-status"></p>')
    out.append('</div>')
    return '\n'.join(out)


def expand(markdown, versions):
    if MARKER_ENTRIES not in markdown:
        raise UpgradeNoteError('%s: marker %s not found in the upgrading page' % (STUB_PAGE, MARKER_ENTRIES))
    markdown = markdown.replace(MARKER_FILTER, render_filter(versions))
    return markdown.replace(MARKER_ENTRIES, render_entries(versions))


def render_page(stub_path=STUB_PAGE, upgrades_dir=UPGRADES_DIR):
    with io.open(stub_path, encoding='utf-8') as f:
        stub = f.read()
    return expand(stub, load_notes(upgrades_dir))


# --- mkdocs hook -------------------------------------------------------------------


def on_config(config):
    # The rendered notes rely on <div markdown="1"> wrappers.
    if 'md_in_html' not in config['markdown_extensions']:
        config['markdown_extensions'].append('md_in_html')
    return config


def on_page_markdown(markdown, page, config, files):
    if page.file.src_uri != PAGE_SRC_URI:
        return markdown
    try:
        return expand(markdown, load_notes())
    except UpgradeNoteError as e:
        try:
            from mkdocs.exceptions import PluginError
        except ImportError:  # pragma: no cover
            raise
        raise PluginError(str(e))


def on_serve(server, config, builder):
    # Rebuild `mkdocs serve` when a note changes, not just docs/docs.
    server.watch(UPGRADES_DIR)
    return server


# --- command line ------------------------------------------------------------------


def main(argv):
    import argparse
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--check', action='store_true', help='validate the notes and exit')
    parser.add_argument('--render', metavar='FILE', nargs='?', const='-',
                        help='write the expanded upgrading page to FILE (default: stdout)')
    args = parser.parse_args(argv)
    try:
        versions = load_notes()
        if args.render is not None:
            page = render_page()
            if args.render == '-':
                sys.stdout.write(page)
            else:
                with io.open(args.render, 'w', encoding='utf-8') as f:
                    f.write(page)
        elif not args.check:
            parser.print_help()
            return 2
    except UpgradeNoteError as e:
        print('error: %s' % e, file=sys.stderr)
        return 1
    count = sum(len(notes) for _, notes in versions)
    print('%d upgrade notes across %d versions: OK' % (count, len(versions)), file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
