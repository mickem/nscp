#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Assemble the "Upgrading" and "Security notices" pages from one file per note.

Both pages used to be edited by nearly every feature branch, each inserting
its entry at the top of the same section, so they conflicted on almost every
merge. Every note is now its own file:

* ``docs/upgrades/<version>/<slug>.md`` — one upgrade note (a bullet on
  ``setup/upgrading.md``), see ``docs/upgrades/README.md``;
* ``docs/security/<slug>.md`` — one security notice (a ``###`` section on
  ``security/notices.md``, plus a row of the advisories table when it carries
  an ``advisory:`` block), see ``docs/security/README.md``.

This module merges them into the two committed stub pages at build time:

* as an **mkdocs hook** (``hooks: [hooks/notes.py]`` in ``mkdocs.yml``) it
  expands the ``<!-- notes:... -->`` markers while the site is built or served;
* as a **command-line tool** it validates the notes (``--check``) or writes
  the expanded pages (``--render DIR``) for a build that does not run mkdocs
  from this repository.

Every note renders inside ``<div class="note-entry" data-modules=...
data-version=...>`` (upgrade notes also grouped in ``<div class="note-version">``
per release); ``js/notes.js`` uses those attributes to let a reader hide the
notes that do not concern the modules they run or the versions they skip.
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
SECURITY_DIR = os.path.join(DOCS_DIR, 'security')
MODULES_DIR = os.path.join(REPO_DIR, 'modules')
REFERENCE_DIR = os.path.join(DOCS_DIR, 'docs', 'reference')
PAGES_DIR = os.path.join(DOCS_DIR, 'docs')

UPGRADING_PAGE = 'setup/upgrading.md'
NOTICES_PAGE = 'security/notices.md'

MARKER_FILTER = '<!-- notes:filter -->'
MARKER_UPGRADES = '<!-- notes:upgrades -->'
MARKER_ADVISORIES = '<!-- notes:advisories -->'
MARKER_HARDENING = '<!-- notes:hardening -->'

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

# In the page filter the areas collapse into two checks: "service" (the agent
# itself — core, filter framework, packaging — which every installation runs)
# and "docs". The front matter keeps the finer tags; only the filter merges.
AREA_FILTER = {'core': 'service', 'filters': 'service', 'packaging': 'service', 'docs': 'docs'}
FILTER_AREAS = [
    ('service', 'The service itself: core, settings, command line, filter framework, packaging'),
    ('docs', 'Documentation'),
]
FILTER_AREA_NAMES = [name for name, _ in FILTER_AREAS]


def filter_tags(modules):
    """The tags an entry is filtered on: its modules, with the areas folded
    into the filter's "service" / "docs" checks (order kept, duplicates dropped)."""
    out = []
    for m in modules:
        tag = AREA_FILTER.get(m, m)
        if tag not in out:
            out.append(tag)
    return out

SECURITY_ICON = u'\U0001F512'  # 🔒

# What the reader has to do about a note. ``required``: everyone running the
# tagged modules must act; ``conditional``: only setups using the feature the
# note describes must check; ``none``: nothing to do. Rendered as a badge and
# behind the "may need action only" switch of the filter.
ACTIONS = ['required', 'conditional', 'none']
ACTION_ICON = u'\U0001F6E0\uFE0F'  # 🛠️
ACTION_BADGE = {
    'required': 'Action required',
    'conditional': 'Check your setup',
}
VERSION_RE = re.compile(r'^\d+(\.\d+)*$')

# Display groups for the module picker, in this order.
GROUP_CHECKS = 'Check modules'
GROUP_CLIENTS = 'Clients and servers'
GROUP_HELPERS = 'Helper modules'
GROUP_GENERAL = 'General'
GROUP_ORDER = [GROUP_GENERAL, GROUP_CHECKS, GROUP_CLIENTS, GROUP_HELPERS]

ADVISORY_COLUMNS = [('id', 'Advisory'), ('cve', 'CVE'), ('severity', 'Severity'),
                    ('affected', 'Affected'), ('summary', 'Summary')]


class NoteError(Exception):
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


def read_front_matter(path):
    with io.open(path, encoding='utf-8') as f:
        text = f.read()
    if not text.startswith('---\n'):
        raise NoteError('%s: missing front matter (the file must start with ---)' % path)
    try:
        _, front, body = text.split('---\n', 2)
    except ValueError:
        raise NoteError('%s: unterminated front matter' % path)
    try:
        meta = yaml.safe_load(front) or {}
    except yaml.YAMLError as e:
        raise NoteError('%s: invalid front matter: %s' % (path, e))
    if not isinstance(meta, dict):
        raise NoteError('%s: front matter must be a mapping' % path)
    body = body.strip('\n')
    if not body.strip():
        raise NoteError('%s: the note has no text' % path)
    return meta, body


def read_modules(meta, path, valid):
    modules = meta.get('modules')
    if isinstance(modules, str):
        modules = [m.strip() for m in modules.split(',') if m.strip()]
    if not modules or not isinstance(modules, list):
        raise NoteError('%s: "modules:" must list at least one module or area' % path)
    modules = [str(m) for m in modules]
    unknown = [m for m in modules if m not in valid]
    if unknown:
        raise NoteError('%s: unknown module(s) %s (expected a module directory name or one of: %s)'
                        % (path, ', '.join(unknown), ', '.join(AREA_NAMES)))
    return modules


def read_action(meta, path):
    action = meta.get('action')
    if action is None:
        raise NoteError('%s: "action:" is required (one of: %s)' % (path, ', '.join(ACTIONS)))
    if not isinstance(action, str) or action.strip() not in ACTIONS:
        raise NoteError('%s: "action:" must be one of: %s' % (path, ', '.join(ACTIONS)))
    return action.strip()


def read_string(meta, key, path, required=False):
    value = meta.get(key)
    if value is None:
        if required:
            raise NoteError('%s: "%s:" is required' % (path, key))
        return ''
    if not isinstance(value, str):
        raise NoteError('%s: "%s:" must be a string' % (path, key))
    return value.strip()


def parse_upgrade_note(path, valid):
    meta, body = read_front_matter(path)
    icon = read_string(meta, 'icon', path)
    security = meta.get('security')
    if security is None:
        security = SECURITY_ICON in icon
    if not isinstance(security, bool):
        raise NoteError('%s: "security:" must be true or false' % path)
    if '**' not in body.split('\n', 1)[0]:
        raise NoteError('%s: the first line must carry a bold **title** (see docs/upgrades/README.md)' % path)
    return {
        'path': path,
        'icon': icon,
        'modules': read_modules(meta, path, valid),
        'action': read_action(meta, path),
        'security': security,
        'body': body,
    }


def parse_security_note(path, valid):
    meta, body = read_front_matter(path)
    fixed_in = read_string(meta, 'fixed_in', path)
    if fixed_in and not VERSION_RE.match(fixed_in):
        raise NoteError('%s: "fixed_in:" must be a version number such as 0.18.1' % path)
    advisory = meta.get('advisory')
    if advisory is not None:
        if not isinstance(advisory, dict):
            raise NoteError('%s: "advisory:" must be a mapping with %s'
                            % (path, ', '.join(k for k, _ in ADVISORY_COLUMNS)))
        advisory = dict((k, read_string(advisory, k, path, required=True)) for k, _ in ADVISORY_COLUMNS)
    return {
        'path': path,
        'title': read_string(meta, 'title', path, required=True),
        'security': True,
        'fixed_in': fixed_in,
        'fixed_in_note': read_string(meta, 'fixed_in_note', path),
        'severity': read_string(meta, 'severity', path),
        'reported_by': read_string(meta, 'reported_by', path),
        'modules': read_modules(meta, path, valid),
        'action': read_action(meta, path),
        'advisory': advisory,
        'body': body,
    }


def load_upgrade_notes(upgrades_dir=UPGRADES_DIR, valid_modules=None):
    """Return ``[(version, [note, ...]), ...]`` newest version first.

    Notes within a version are ordered by file name; the migrated history uses
    a ``NNN-`` prefix to keep its original order and unnumbered files sort
    after those, alphabetically.
    """
    valid = set(known_modules() if valid_modules is None else valid_modules) | set(AREA_NAMES)
    versions = []
    for entry in os.listdir(upgrades_dir):
        vdir = os.path.join(upgrades_dir, entry)
        if not os.path.isdir(vdir):
            continue
        if not VERSION_RE.match(entry):
            raise NoteError('%s: directory name is not a version number' % vdir)
        notes = []
        for name in sorted(os.listdir(vdir)):
            if name.endswith('.md'):
                note = parse_upgrade_note(os.path.join(vdir, name), valid)
                note['version'] = entry
                notes.append(note)
        if not notes:
            raise NoteError('%s: version directory has no notes' % vdir)
        versions.append((entry, notes))
    versions.sort(key=lambda v: version_key(v[0]), reverse=True)
    return versions


def load_security_notes(security_dir=SECURITY_DIR, valid_modules=None):
    """Return the security notices, newest ``fixed_in`` first (file name order
    within a version; notices without a version last)."""
    valid = set(known_modules() if valid_modules is None else valid_modules) | set(AREA_NAMES)
    notes = []
    for name in sorted(os.listdir(security_dir)):
        if name.endswith('.md') and name != 'README.md':
            notes.append(parse_security_note(os.path.join(security_dir, name), valid))
    notes.sort(key=lambda n: (0, tuple(-p for p in version_key(n['fixed_in']))) if n['fixed_in'] else (1, ()))
    return notes


# --- rendering -------------------------------------------------------------------


def html_attr(value):
    return value.replace('&', '&amp;').replace('"', '&quot;').replace('<', '&lt;')


def entry_attrs(note, version=''):
    attrs = ' data-modules="%s" data-action="%s"' % (html_attr(' '.join(filter_tags(note['modules']))), note['action'])
    if version:
        attrs += ' data-version="%s"' % html_attr(version)
    if note.get('security'):
        attrs += ' data-security="1"'
    return attrs


def entry_open(note, version=''):
    return '<div class="note-entry" markdown="1"%s>' % entry_attrs(note, version)


def action_badge(note):
    label = ACTION_BADGE.get(note['action'])
    if not label:
        return ''
    return '<span class="note-badge note-badge--%s">%s</span>' % (note['action'], label)


def render_upgrade_note(note):
    lines = note['body'].split('\n')
    badge = action_badge(note)
    head = '- ' + (note['icon'] + ' ' if note['icon'] else '') + (badge + ' ' if badge else '') + lines[0]
    # Continuation lines are indented four spaces so nested lists, tables and
    # fenced code blocks stay inside the list item.
    rest = [('    ' + l) if l.strip() else '' for l in lines[1:]]
    return '%s\n\n%s\n\n</div>' % (entry_open(note), '\n'.join([head] + rest))


def render_upgrades(versions):
    out = []
    for version, notes in versions:
        out.append('<div class="note-version" markdown="1" data-version="%s">\n' % html_attr(version))
        out.append('## %s\n' % version)
        out.extend(render_upgrade_note(n) + '\n' for n in notes)
        out.append('</div>\n')
    return '\n'.join(out)


def render_security_note(note):
    out = [entry_open(note, version=note['fixed_in']), '', '### %s' % note['title'], '']
    meta = []
    badge = action_badge(note)
    if badge:
        meta.append(badge)
    if note['fixed_in']:
        fixed = note['fixed_in']
        if note['fixed_in_note']:
            fixed += ' (%s)' % note['fixed_in_note']
        meta.append('**Fixed in:** ' + fixed)
    if note['severity']:
        meta.append('**Severity:** ' + note['severity'])
    if note['reported_by']:
        meta.append('**Reported by:** ' + note['reported_by'])
    if meta:
        out.extend([u' · '.join(meta), ''])
    out.extend([note['body'], '', '</div>'])
    return '\n'.join(out)


def render_advisory_table(notes):
    """The summary table of published advisories, one filterable row each."""
    # md_in_html only parses the cells as Markdown when every enclosing element
    # is marked block-level and each tag sits on its own line.
    out = ['<table class="note-table" markdown="block">', '<thead><tr>%s</tr></thead>'
           % ''.join('<th>%s</th>' % label for _, label in ADVISORY_COLUMNS), '<tbody markdown="block">']
    for n in notes:
        out.append('<tr class="note-entry" markdown="block"%s>' % entry_attrs(n, n['fixed_in']))
        out.extend('<td markdown="span">%s</td>' % n['advisory'][k] for k, _ in ADVISORY_COLUMNS)
        out.append('</tr>')
    out.extend(['</tbody>', '</table>'])
    return '\n'.join(out)


def render_security(notes, advisories):
    selected = [n for n in notes if bool(n['advisory']) == advisories]
    out = []
    if advisories:
        out.append(render_advisory_table(selected) + '\n')
    out.extend(render_security_note(n) + '\n' for n in selected)
    return '\n'.join(out)


def module_group(name):
    if name in FILTER_AREA_NAMES:
        return GROUP_GENERAL
    if name.startswith('Check'):
        return GROUP_CHECKS
    if name.endswith('Client') or name.endswith('Server'):
        return GROUP_CLIENTS
    return GROUP_HELPERS


def used_modules():
    """Every module tagged on either page. Both pages show the same picker so
    a selection made on one carries over to the other unchanged."""
    used = set()
    for _, notes in load_upgrade_notes():
        for n in notes:
            used.update(filter_tags(n['modules']))
    for n in load_security_notes():
        used.update(filter_tags(n['modules']))
    return used


def render_filter(versions, security_switch):
    """The filter form. Hidden until ``js/notes.js`` enables it, so a reader
    without JavaScript simply sees the whole page."""
    used = used_modules()
    groups = {}
    for m in used:
        groups.setdefault(module_group(m), []).append(m)
    area_label = dict(FILTER_AREAS)
    options = ''.join('<option value="%s">%s</option>' % (v, v) for v in versions)

    out = ['<div class="note-filter" id="note-filter" hidden>']
    out.append('<div class="note-filter__row">')
    out.append('<label>Upgrading from <select id="note-from"><option value="">any version</option>%s</select></label>' % options)
    out.append('<label>to <select id="note-to"><option value="">latest</option>%s</select></label>' % options)
    out.append('</div>')
    out.append('<div class="note-filter__row">')
    if security_switch:
        out.append('<label><input type="checkbox" id="note-security"> %s security-relevant only</label>' % SECURITY_ICON)
    out.append('<label title="Hide entries that need nothing from you"><input type="checkbox" id="note-action"> '
               '%s may need action only</label>' % ACTION_ICON)
    out.append('</div>')
    out.append('<details class="note-filter__modules" id="note-modules">')
    out.append('<summary>Modules: <span id="note-modules-summary">all</span></summary>')
    out.append('<div class="note-filter__actions">'
               '<button type="button" data-action="all">Select all</button> '
               '<button type="button" data-action="none">Select none</button> '
               '<button type="button" data-action="reset">Reset</button></div>')
    out.append('<p class="note-filter__hint">Tick the modules your <code>nsclient.ini</code> enables. '
               'Entries that touch none of the ticked modules are hidden; <em>service</em> covers the agent '
               'itself, which every installation runs. The selection is shared between the Upgrading and '
               'Security notices pages.</p>')
    out.append('<div class="note-filter__groups">')
    for group in GROUP_ORDER:
        names = groups.get(group)
        if not names:
            continue
        if group == GROUP_GENERAL:
            names = [a for a in FILTER_AREA_NAMES if a in names]
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
    out.append('<p class="note-filter__status" id="note-status"></p>')
    out.append('</div>')
    return '\n'.join(out)


def all_versions():
    """Every release either page knows about, newest first: the version
    selects list the same choices on both pages."""
    versions = set(v for v, _ in load_upgrade_notes())
    versions.update(n['fixed_in'] for n in load_security_notes() if n['fixed_in'])
    return sorted(versions, key=version_key, reverse=True)


def expand_upgrading(markdown):
    if MARKER_UPGRADES not in markdown:
        raise NoteError('%s: marker %s not found' % (UPGRADING_PAGE, MARKER_UPGRADES))
    versions = load_upgrade_notes()
    markdown = markdown.replace(MARKER_FILTER, render_filter(all_versions(), security_switch=True))
    return markdown.replace(MARKER_UPGRADES, render_upgrades(versions))


def expand_notices(markdown):
    for marker in (MARKER_ADVISORIES, MARKER_HARDENING):
        if marker not in markdown:
            raise NoteError('%s: marker %s not found' % (NOTICES_PAGE, marker))
    notes = load_security_notes()
    markdown = markdown.replace(MARKER_FILTER, render_filter(all_versions(), security_switch=False))
    markdown = markdown.replace(MARKER_ADVISORIES, render_security(notes, advisories=True))
    return markdown.replace(MARKER_HARDENING, render_security(notes, advisories=False))


PAGES = {
    UPGRADING_PAGE: expand_upgrading,
    NOTICES_PAGE: expand_notices,
}


def render_page(src_uri):
    with io.open(os.path.join(PAGES_DIR, src_uri), encoding='utf-8') as f:
        return PAGES[src_uri](f.read())


# --- mkdocs hook -------------------------------------------------------------------


def on_config(config):
    # The rendered notes rely on <div markdown="1"> wrappers.
    if 'md_in_html' not in config['markdown_extensions']:
        config['markdown_extensions'].append('md_in_html')
    return config


def on_page_markdown(markdown, page, config, files):
    expand = PAGES.get(page.file.src_uri)
    if expand is None:
        return markdown
    try:
        return expand(markdown)
    except NoteError as e:
        try:
            from mkdocs.exceptions import PluginError
        except ImportError:  # pragma: no cover
            raise
        raise PluginError(str(e))


def on_serve(server, config, builder):
    # Rebuild `mkdocs serve` when a note changes, not just docs/docs.
    server.watch(UPGRADES_DIR)
    server.watch(SECURITY_DIR)
    return server


# --- command line ------------------------------------------------------------------


def main(argv):
    import argparse
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--check', action='store_true', help='validate the notes and exit')
    parser.add_argument('--render', metavar='DIR',
                        help='write the expanded setup/upgrading.md and security/notices.md below DIR '
                             '(pass docs/docs to expand the stub pages in place before a build)')
    args = parser.parse_args(argv)
    try:
        upgrades = load_upgrade_notes()
        security = load_security_notes()
        pages = dict((uri, render_page(uri)) for uri in PAGES)
        if args.render:
            for uri, text in pages.items():
                target = os.path.join(args.render, uri)
                dirname = os.path.dirname(target)
                if dirname and not os.path.isdir(dirname):
                    os.makedirs(dirname)
                with io.open(target, 'w', encoding='utf-8') as f:
                    f.write(text)
                print('wrote %s' % target, file=sys.stderr)
        elif not args.check:
            parser.print_help()
            return 2
    except NoteError as e:
        print('error: %s' % e, file=sys.stderr)
        return 1
    print('%d upgrade notes across %d versions, %d security notices: OK'
          % (sum(len(n) for _, n in upgrades), len(upgrades), len(security)), file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
