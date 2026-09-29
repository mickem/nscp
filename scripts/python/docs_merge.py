#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Documentation merger (between step 1 and step 2 of the docs pipeline).
#
# docs_extract.py runs inside nscp and rewrites only the slice of
# docs/reference/<Module>.yaml for the platform it runs on. When the platform
# builds run side by side (.github/workflows/update-docs.yml), each of them
# produces its own copy of docs/reference/ in which exactly one slice is fresh
# and every other slice is whatever was committed. This script folds those
# copies back into one tree: for every module it takes each platform's data
# from the run made on that platform, keeps what the committed file records for
# a platform no run covered, and re-factors the result into the common +
# per-platform layout docs_extract.py writes. The outcome is the same as
# running the extractions one after another on a single checkout, which is
# what a developer with all three machines would do by hand.
#
#   python3 scripts/python/docs_merge.py --reference docs/reference \
#       --slice windows=/path/to/windows-run/reference \
#       --slice unix=/path/to/linux-run/reference \
#       --slice darwin=/path/to/macos-run/reference
#
# Standalone: PyYAML only, no running nscp. Render the Markdown afterwards
# with docs_generate.py.
import argparse
import glob
import os
import sys
import yaml

COMMON_FILE = 'common-options.yaml'

# --- Platform factoring. KEEP IN SYNC with docs_extract.py (the writer) and
# docs_generate.py (the reader): the on-disk layout is theirs, this file only
# has to round-trip it. docs_extract.py cannot be imported here because it
# imports the NSCP module that only exists inside the agent.
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


# --- File handling. Same serialisation as docs_extract.py, so a merge that
# changes nothing leaves every file byte-identical. ------------------------------
def load_yaml(path):
    with open(path, encoding='utf-8') as f:
        return yaml.safe_load(f) or {}


def dump_yaml(data):
    return yaml.safe_dump(data, sort_keys=True, default_flow_style=False,
                          allow_unicode=True, width=100)


def read_header(path):
    # The leading comment block of a YAML file, newline-terminated. This is how
    # common-options.yaml keeps the explanatory header docs_extract.py writes
    # without this script having to carry a copy of the text.
    lines = []
    with open(path, encoding='utf-8') as f:
        for line in f:
            if not line.startswith('#'):
                break
            lines.append(line)
    return ''.join(lines)


def write_if_changed(path, text):
    if os.path.exists(path):
        with open(path, encoding='utf-8') as f:
            if f.read() == text:
                return False
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
    return True


def merge_module(reference_dir, module, slices):
    """Fold the per-run copies of one module file into the reference tree.

    slices is a list of (platform, directory) pairs. Returns (written, taken)
    where taken lists the platforms a fresh slice was found for, or None when
    no run and no committed file knows the module at all.
    """
    path = os.path.join(reference_dir, '%s.yaml' % module)
    trees = expand(load_yaml(path)) if os.path.exists(path) else {}
    identity = None
    taken = []
    for platform, slice_dir in slices:
        spath = os.path.join(slice_dir, '%s.yaml' % module)
        if not os.path.exists(spath):
            continue
        data = load_yaml(spath)
        if platform not in data.get('platforms', []):
            # The run on this platform never saw the module (built without it,
            # or a module that only exists elsewhere), so its copy of the file
            # holds nothing fresh for this platform. Keep what is committed,
            # exactly as docs_extract.py leaves such a file alone.
            continue
        trees[platform] = expand_platform(data, platform)
        identity = (data.get('module'), data.get('namespace'))
        taken.append(platform)
    if not trees:
        return None
    if identity is not None:
        # module/namespace are platform-invariant and statically classified in
        # docs_extract.py, so the freshly extracted values win over whatever a
        # committed file carries (see write_module_yaml there).
        for tree in trees.values():
            tree['module'], tree['namespace'] = identity
    return write_if_changed(path, dump_yaml(factor(trees))), taken


def merge_common(reference_dir, slices):
    """Union the shared option/keyword descriptions from every run.

    Mirrors write_common_yaml in docs_extract.py: a run only adds to the file,
    it never drops an entry another run contributed. A later slice wins when
    two runs describe the same option differently, and says so.
    """
    path = os.path.join(reference_dir, COMMON_FILE)
    header = ''
    options = {}
    fields = {}
    if os.path.exists(path):
        header = read_header(path)
        data = load_yaml(path)
        options = dict(data.get('options', {}))
        fields = dict(data.get('fields', {}))
    for platform, slice_dir in slices:
        spath = os.path.join(slice_dir, COMMON_FILE)
        if not os.path.exists(spath):
            continue
        if not header:
            header = read_header(spath)
        data = load_yaml(spath)
        for section, merged in (('options', options), ('fields', fields)):
            for name, value in data.get(section, {}).items():
                old = merged.get(name)
                if old is not None and old != value:
                    print('warning: %s "%s" differs on %s; taking the %s description'
                          % (section[:-1], name, platform, platform), file=sys.stderr)
                merged[name] = value
    if not options and not fields:
        return False
    return write_if_changed(path, header + dump_yaml({'options': options, 'fields': fields}))


def list_modules(directories):
    modules = set()
    for directory in directories:
        for path in glob.glob(os.path.join(directory, '*.yaml')):
            name = os.path.splitext(os.path.basename(path))[0]
            if name + '.yaml' != COMMON_FILE:
                modules.add(name)
    return sorted(modules)


def parse_slice(text):
    platform, sep, directory = text.partition('=')
    if not sep or not platform or not directory:
        raise argparse.ArgumentTypeError('expected PLATFORM=DIRECTORY, got %r' % text)
    if not os.path.isdir(directory):
        raise argparse.ArgumentTypeError('%s: not a directory' % directory)
    return platform, directory


def main(argv):
    parser = argparse.ArgumentParser(
        prog='docs_merge',
        description='Fold per-platform docs_extract.py runs into one docs/reference tree.')
    parser.add_argument('--reference', default=os.path.join('docs', 'reference'),
                        help='the committed reference folder to update (default: docs/reference)')
    parser.add_argument('--slice', dest='slices', action='append', required=True,
                        type=parse_slice, metavar='PLATFORM=DIRECTORY',
                        help='a reference folder written by docs_extract.py on PLATFORM '
                             '(windows, unix or darwin); repeat once per run')
    args = parser.parse_args(argv)

    seen = set()
    for platform, _ in args.slices:
        if platform in seen:
            parser.error('platform %s given twice' % platform)
        seen.add(platform)

    if not os.path.isdir(args.reference):
        os.makedirs(args.reference)

    changed = []
    for module in list_modules([args.reference] + [d for _, d in args.slices]):
        result = merge_module(args.reference, module, args.slices)
        if result is None:
            continue
        written, taken = result
        print('%-9s %s (%s)' % ('updated' if written else 'unchanged', module,
                                ', '.join(taken) if taken else 'no fresh slice'))
        if written:
            changed.append(module)
    if merge_common(args.reference, args.slices):
        print('%-9s %s' % ('updated', COMMON_FILE))
        changed.append(COMMON_FILE)
    print('Merged %d slice(s) into %s: %d file(s) changed'
          % (len(args.slices), args.reference, len(changed)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
