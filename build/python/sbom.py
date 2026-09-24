#!/usr/bin/env python3
"""Write a CycloneDX software bill of materials for a Windows NSClient++ build.

The build downloads every third-party dependency and checks it against the
digest or commit recorded in .github/dependency-checksums.txt before it
compiles anything. That verification happens in CI and leaves nothing behind
in what we ship. This script turns it into a record that travels with the
release: for every dependency, the version the build used, where it came from
and the SHA-256 (or git commit) it was verified against, so anyone can fetch
the same upstream file and check it themselves.

The values are read from the manifest, not recomputed from whatever the build
happened to download, so the SBOM can only ever claim what the build verified.

Given --zip, the SBOM is also added to the zip as sbom.cdx.json, together with
SHA256SUMS: the SHA-256 of every file in the zip, in the format
`sha256sum -c SHA256SUMS` reads.

Run with --check to confirm that every dependency in the manifest has the
metadata below and the other way round; CI runs that before generating.
"""

import argparse
import base64
import datetime
import hashlib
import io
import json
import os
import re
import sys
import tempfile
import uuid
import zipfile

REPO_ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
DEFAULT_MANIFEST = os.path.join(REPO_ROOT, '.github', 'dependency-checksums.txt')
MANIFEST_NAME = '.github/dependency-checksums.txt'

SBOM_ENTRY = 'sbom.cdx.json'
SUMS_ENTRY = 'SHA256SUMS'

SHA256_RE = re.compile(r'^[0-9a-f]{64}$')
COMMIT_RE = re.compile(r'^[0-9a-f]{40}$')

# Metadata for every name in the manifest. `download` entries are verified by
# the SHA-256 of the file at `url`, `git` entries by the commit the tag `ref`
# resolved to. `{version}`, `{version_}` (dots as underscores), `{version0}`
# (dots removed) and `{variant}` (the manifest name past the table key, e.g.
# `x64` in check_nsclient-x64) are substituted. The URLs must match the ones
# the composite actions under .github/actions/ fetch.
COMPONENTS = {
    'openssl': {
        'name': 'OpenSSL',
        'supplier': 'OpenSSL Foundation',
        'homepage': 'https://www.openssl.org',
        'license': 'Apache-2.0',
        'purl': 'pkg:github/openssl/openssl@openssl-{version}',
        'cpe': 'cpe:2.3:a:openssl:openssl:{version}:*:*:*:*:*:*:*',
        'kind': 'download',
        'url': 'https://github.com/openssl/openssl/releases/download/openssl-{version}/openssl-{version}.tar.gz',
    },
    'protobuf': {
        'name': 'Protocol Buffers',
        'supplier': 'Google LLC',
        'homepage': 'https://protobuf.dev',
        'license': 'BSD-3-Clause',
        'purl': 'pkg:github/protocolbuffers/protobuf@v{version}',
        'kind': 'download',
        'url': 'https://github.com/protocolbuffers/protobuf/releases/download/v{version}/protobuf-all-{version}.zip',
    },
    'cryptopp': {
        'name': 'Crypto++',
        'supplier': 'Wei Dai and the Crypto++ project',
        'homepage': 'https://cryptopp.com',
        'license': 'BSL-1.0',
        'purl': 'pkg:github/weidai11/cryptopp@CRYPTOPP_{version_}',
        'kind': 'download',
        'url': 'https://github.com/weidai11/cryptopp/releases/download/CRYPTOPP_{version_}/cryptopp{version0}.zip',
    },
    'miniz': {
        'name': 'miniz',
        'supplier': 'Rich Geldreich',
        'homepage': 'https://github.com/richgel999/miniz',
        'license': 'MIT',
        'purl': 'pkg:github/richgel999/miniz@{version}',
        'kind': 'download',
        'url': 'https://github.com/richgel999/miniz/releases/download/{version}/miniz-{version}.zip',
    },
    'boost': {
        'name': 'Boost',
        'supplier': 'Boost.org',
        'homepage': 'https://www.boost.org',
        'license': 'BSL-1.0',
        'purl': 'pkg:generic/boost@{version}',
        'cpe': 'cpe:2.3:a:boost:boost:{version}:*:*:*:*:*:*:*',
        'kind': 'download',
        'url': 'https://archives.boost.io/release/{version}/source/boost_{version_}.tar.gz',
    },
    'lua': {
        'name': 'Lua',
        'supplier': 'Lua.org, PUC-Rio',
        'homepage': 'https://www.lua.org',
        'license': 'MIT',
        'purl': 'pkg:generic/lua@{version}',
        'cpe': 'cpe:2.3:a:lua:lua:{version}:*:*:*:*:*:*:*',
        'kind': 'download',
        'url': 'https://www.lua.org/ftp/lua-{version}.tar.gz',
    },
    'googletest': {
        'name': 'GoogleTest',
        'supplier': 'Google LLC',
        'homepage': 'https://github.com/google/googletest',
        'license': 'BSD-3-Clause',
        'purl': 'pkg:github/google/googletest@release-{version}',
        'kind': 'git',
        'url': 'https://github.com/google/googletest.git',
        'ref': 'release-{version}',
        # Links the unit tests only; nothing in the zip or the MSI carries it.
        'scope': 'excluded',
    },
    'tinyxml2': {
        'name': 'TinyXML-2',
        'supplier': 'Lee Thomason',
        'homepage': 'https://github.com/leethomason/tinyxml2',
        'license': 'Zlib',
        'purl': 'pkg:github/leethomason/tinyxml2@{version}',
        'kind': 'git',
        'url': 'https://github.com/leethomason/tinyxml2.git',
        'ref': '{version}',
    },
    'mongoose': {
        'name': 'Mongoose',
        'supplier': 'Cesanta Software Ltd',
        'homepage': 'https://github.com/cesanta/mongoose',
        'license': 'GPL-2.0-only',
        'purl': 'pkg:github/cesanta/mongoose@{version}',
        'cpe': 'cpe:2.3:a:cesanta:mongoose:{version}:*:*:*:*:*:*:*',
        'kind': 'git',
        'url': 'https://github.com/cesanta/mongoose.git',
        'ref': '{version}',
    },
    'mariadb': {
        'name': 'MariaDB Connector/C',
        'supplier': 'MariaDB Corporation',
        'homepage': 'https://github.com/mariadb-corporation/mariadb-connector-c',
        'license': 'LGPL-2.1-or-later',
        'purl': 'pkg:github/mariadb-corporation/mariadb-connector-c@v{version}',
        'kind': 'git',
        'url': 'https://github.com/mariadb-corporation/mariadb-connector-c.git',
        'ref': 'v{version}',
    },
    'check_nsclient': {
        'name': 'check_nsclient',
        'type': 'application',
        'supplier': 'Michael Medin',
        'homepage': 'https://github.com/mickem/check_nsclient',
        'purl': 'pkg:github/mickem/check_nsclient@{version}',
        'kind': 'download',
        'url': 'https://github.com/mickem/check_nsclient/releases/download/{version}/check_nsclient-{version}-windows-{variant}.exe',
        # The SBOM the release publishes for that binary, nested by --nested-sbom.
        'sbom_url': 'https://github.com/mickem/check_nsclient/releases/download/{version}/check_nsclient-{version}-windows-{variant}.cdx.json',
    },
    'check_nsclient-linux': {
        'name': 'check_nsclient',
        'type': 'application',
        'supplier': 'Michael Medin',
        'homepage': 'https://github.com/mickem/check_nsclient',
        'purl': 'pkg:github/mickem/check_nsclient@{version}',
        'kind': 'download',
        'url': 'https://github.com/mickem/check_nsclient/releases/download/{version}/check_nsclient-{version}-linux-{variant}',
    },
    # Not a component: the SHA256SUMS a check_nsclient release publishes. Its
    # pinned digest is what the build checks the release's SBOMs against
    # before nesting one with --nested-sbom.
    'check_nsclient-sha256sums': {
        'name': 'check_nsclient SHA256SUMS',
        'kind': 'download',
        'checksum_list': True,
        'url': 'https://github.com/mickem/check_nsclient/releases/download/{version}/SHA256SUMS',
    },
}


# NuGet packages the .NET plugin host ships, by package id.
NUGET_PACKAGES = {
    'Google.Protobuf': {
        'supplier': 'Google LLC',
        'homepage': 'https://github.com/protocolbuffers/protobuf',
        'license': 'BSD-3-Clause',
    },
}


class SbomError(Exception):
    pass


def parse_manifest(path):
    """Return {(name, version): (value, comment)} from the checksum manifest."""
    entries = {}
    with open(path, encoding='utf-8') as f:
        for number, raw in enumerate(f, 1):
            line = raw.strip()
            if not line or line.startswith('#'):
                continue
            body, _, comment = line.partition('#')
            fields = body.split()
            if len(fields) != 3:
                raise SbomError(f'{path}:{number}: expected "<name> <version> <digest>", got {line!r}')
            name, version, value = fields
            if (name, version) in entries:
                raise SbomError(f'{path}:{number}: {name} {version} is listed twice')
            entries[(name, version)] = (value, comment.strip())
    return entries


def lookup(name):
    """Return (table key, metadata, variant) for a manifest name."""
    if name in COMPONENTS:
        return name, COMPONENTS[name], ''
    # The longest key followed by '-' wins, so check_nsclient-linux-x64 is
    # check_nsclient-linux with variant x64, not check_nsclient with linux-x64.
    for key in sorted(COMPONENTS, key=len, reverse=True):
        if name.startswith(key + '-'):
            return key, COMPONENTS[key], name[len(key) + 1:]
    raise SbomError(f'{name} has no entry in the COMPONENTS table of {os.path.basename(__file__)}; '
                    f'add one next to its line in {MANIFEST_NAME}')


def expand(template, version, variant):
    return template.format(version=version, version_=version.replace('.', '_'),
                           version0=version.replace('.', ''), variant=variant)


def check_manifest(manifest):
    """Return a list of problems between the manifest and COMPONENTS."""
    problems = []
    used = set()
    for (name, version), (value, _) in sorted(manifest.items()):
        try:
            key, meta, _ = lookup(name)
        except SbomError as e:
            problems.append(str(e))
            continue
        used.add(key)
        if value == 'unrecorded':
            problems.append(f'{name} {version} is unrecorded: the build downloads it unverified')
        elif meta['kind'] == 'download' and not SHA256_RE.match(value):
            problems.append(f'{name} {version} is a download, so it needs a SHA-256, not {value!r}')
        elif meta['kind'] == 'git' and not COMMIT_RE.match(value):
            problems.append(f'{name} {version} is cloned, so it needs a 40-hex commit id, not {value!r}')
    for key in sorted(set(COMPONENTS) - used):
        problems.append(f'{key} is described in {os.path.basename(__file__)} but has no line in {MANIFEST_NAME}')
    return problems


def prop(name, value):
    return {'name': f'nscp:{name}', 'value': value}


def manifest_component(manifest, name, version):
    """A component for a dependency the build verified against the manifest."""
    key, meta, variant = lookup(name)
    if meta.get('checksum_list'):
        raise SbomError(f'{name} is a checksum list, not a component')
    if (name, version) not in manifest:
        raise SbomError(f'{name} {version} has no line in {MANIFEST_NAME}; the build could not have verified it')
    value, comment = manifest[(name, version)]
    component = {
        'type': meta.get('type', 'library'),
        'bom-ref': f'{name}@{version}',
        'supplier': {'name': meta['supplier'], 'url': [meta['homepage']]},
        'name': meta['name'],
        'version': version,
        'scope': meta.get('scope', 'required'),
    }
    if 'license' in meta:
        component['licenses'] = [{'license': {'id': meta['license']}}]
    component['purl'] = expand(meta['purl'], version, variant)
    if 'cpe' in meta:
        component['cpe'] = expand(meta['cpe'], version, variant)
    url = expand(meta['url'], version, variant)
    properties = [prop('manifest-name', name)]
    if value == 'unrecorded':
        print(f'warning: {name} {version} is unrecorded in {MANIFEST_NAME}; '
              f'the SBOM lists it as unverified', file=sys.stderr)
        component['externalReferences'] = [
            {'type': 'distribution' if meta['kind'] == 'download' else 'vcs', 'url': url,
             'comment': 'Fetched without integrity verification: no digest is recorded for this version.'},
        ]
        properties.append(prop('verification', 'none'))
    elif meta['kind'] == 'download':
        hashes = [{'alg': 'SHA-256', 'content': value}]
        component['hashes'] = hashes
        component['externalReferences'] = [
            {'type': 'distribution', 'url': url, 'hashes': hashes,
             'comment': f'The file the build downloaded. Its SHA-256 was checked against {MANIFEST_NAME} '
                        f'before anything was built from it.'},
        ]
        properties.append(prop('verification', 'sha256'))
    else:
        ref = expand(meta['ref'], version, variant)
        component['externalReferences'] = [
            {'type': 'vcs', 'url': url,
             'comment': f'Cloned at tag {ref}. The build checked that the tag resolves to commit {value}, '
                        f'as recorded in {MANIFEST_NAME}.'},
        ]
        properties.append(prop('verification', 'git-commit'))
        properties.append(prop('git-ref', ref))
        properties.append(prop('git-commit', value))
    component['externalReferences'].append({'type': 'website', 'url': meta['homepage']})
    if comment:
        properties.append(prop('verification-note', comment))
    component['properties'] = properties
    return component


def python_component(version):
    """The embedded CPython runtime, taken from actions/setup-python."""
    return {
        'type': 'application',
        'bom-ref': f'cpython@{version}',
        'supplier': {'name': 'Python Software Foundation', 'url': ['https://www.python.org']},
        'name': 'CPython',
        'version': version,
        'scope': 'required',
        'licenses': [{'license': {'id': 'PSF-2.0'}}],
        'purl': f'pkg:generic/cpython@{version}',
        'cpe': f'cpe:2.3:a:python:python:{version}:*:*:*:*:*:*:*',
        'externalReferences': [
            {'type': 'distribution', 'url': 'https://github.com/actions/python-versions/releases',
             'comment': 'The runtime actions/setup-python installs on the build runner, bundled as the '
                        'embedded Python of the PythonScript module. No digest of it is recorded in '
                        f'{MANIFEST_NAME}.'},
            {'type': 'website', 'url': 'https://www.python.org'},
        ],
        'properties': [prop('verification', 'none')],
    }


def integrity_hash(integrity):
    """Turn an npm/NuGet `sha512-<base64>` integrity string into a CycloneDX hash."""
    algorithms = {'sha512': 'SHA-512', 'sha384': 'SHA-384', 'sha256': 'SHA-256', 'sha1': 'SHA-1'}
    # An integrity string may list several; the first is the strongest npm wrote.
    first = integrity.split()[0]
    alg, _, b64 = first.partition('-')
    if alg not in algorithms:
        raise SbomError(f'unsupported integrity algorithm {alg!r}')
    return {'alg': algorithms[alg], 'content': base64.b64decode(b64).hex()}


def npm_components(lock_path):
    """Components for the packages bundled into the web UI, from package-lock.json.

    Only packages npm does not mark dev/devOptional reach web/dist; the rest
    are build tooling.
    """
    with open(lock_path, encoding='utf-8') as f:
        lock = json.load(f)
    if lock.get('lockfileVersion', 0) < 2:
        raise SbomError(f'{lock_path}: lockfileVersion {lock.get("lockfileVersion")} has no "packages" map')
    components = {}
    for path, pkg in lock['packages'].items():
        if not path or pkg.get('dev') or pkg.get('devOptional') or pkg.get('link'):
            continue
        full_name = path.rsplit('node_modules/', 1)[-1]
        version = pkg['version']
        if full_name.startswith('@'):
            group, name = full_name.split('/', 1)
            purl = f'pkg:npm/%40{group[1:]}/{name}@{version}'
        else:
            group, name = None, full_name
            purl = f'pkg:npm/{name}@{version}'
        if purl in components:
            continue
        component = {'type': 'library', 'bom-ref': purl}
        if group:
            component['group'] = group
        component.update({'name': name, 'version': version, 'scope': 'required'})
        license_value = pkg.get('license')
        if isinstance(license_value, str) and license_value:
            component['licenses'] = [{'expression': license_value}]
        component['purl'] = purl
        references = []
        if pkg.get('integrity'):
            hashes = [integrity_hash(pkg['integrity'])]
            component['hashes'] = hashes
            if pkg.get('resolved'):
                references.append({'type': 'distribution', 'url': pkg['resolved'], 'hashes': hashes})
        if references:
            component['externalReferences'] = references
        component['properties'] = [prop('bundled-in', 'web UI'),
                                   prop('verification', 'npm-integrity' if pkg.get('integrity') else 'none')]
        components[purl] = component
    return [components[k] for k in sorted(components)]


def nuget_component(name, version, nuget_root):
    """A NuGet package, hashed with the SHA-512 NuGet recorded when it restored it."""
    meta = NUGET_PACKAGES.get(name, {})
    component = {'type': 'library', 'bom-ref': f'pkg:nuget/{name}@{version}'}
    if meta:
        component['supplier'] = {'name': meta['supplier'], 'url': [meta['homepage']]}
    component.update({'name': name, 'version': version, 'scope': 'required'})
    if meta:
        component['licenses'] = [{'license': {'id': meta['license']}}]
    component['purl'] = f'pkg:nuget/{name}@{version}'
    sha_file = os.path.join(nuget_root, name.lower(), version, f'{name.lower()}.{version}.nupkg.sha512')
    url = f'https://www.nuget.org/api/v2/package/{name}/{version}'
    reference = {'type': 'distribution', 'url': url}
    recorded = ''
    if os.path.isfile(sha_file):
        with open(sha_file, encoding='ascii') as f:
            recorded = f.read().strip()
    hashes = None
    try:
        candidate = integrity_hash('sha512-' + recorded) if recorded else None
        if candidate and len(candidate['content']) == 128:
            hashes = [candidate]
    except ValueError:  # not base64
        pass
    if hashes:
        component['hashes'] = hashes
        reference['hashes'] = hashes
        verification = 'nuget-restore'
    else:
        print(f'warning: no usable SHA-512 in {sha_file}; {name} {version} is listed without a hash',
              file=sys.stderr)
        verification = 'none'
    component['externalReferences'] = [reference]
    component['properties'] = [prop('verification', verification)]
    return component


def zip_file_names(zip_path):
    with zipfile.ZipFile(zip_path) as z:
        return [i.filename for i in z.infolist() if not i.is_dir()]


def _walk(components):
    for c in components:
        yield c
        yield from _walk(c.get('components', []))


def nest_sbom(component, path):
    """Nest the SBOM a dependency publishes for itself under its component.

    The nested SBOM's own root becomes `component`; every other bom-ref is
    prefixed with the component's so it cannot collide with ours. Returns its
    dependency graph, rewritten to the new refs, for the caller to append.
    """
    key, meta, variant = lookup(next(p['value'] for p in component['properties']
                                     if p['name'] == 'nscp:manifest-name'))
    if 'sbom_url' not in meta:
        raise SbomError(f'{component["bom-ref"]} publishes no SBOM to nest')
    with open(path, 'rb') as f:
        raw = f.read()
    inner = json.loads(raw)
    if inner.get('bomFormat') != 'CycloneDX':
        raise SbomError(f'{path} is not a CycloneDX SBOM')
    root = inner.get('metadata', {}).get('component', {})
    if root.get('name') != meta['name'] or root.get('version') != component['version']:
        raise SbomError(f'{path} describes {root.get("name")} {root.get("version")}, '
                        f'not {meta["name"]} {component["version"]}')

    parent = component['bom-ref']
    inner_root = root.get('bom-ref')

    def ref(value):
        return parent if value == inner_root else f'{parent}|{value}'

    def rewrite(c):
        c = dict(c)
        if 'bom-ref' in c:
            c['bom-ref'] = ref(c['bom-ref'])
        if 'components' in c:
            c['components'] = [rewrite(x) for x in c['components']]
        return c

    nested = [rewrite(c) for c in inner.get('components', [])]
    known = {c['bom-ref'] for c in _walk(nested) if 'bom-ref' in c} | {parent}
    dependencies = []
    for d in inner.get('dependencies', []):
        r = ref(d['ref'])
        if r in known:
            dependencies.append({'ref': r, 'dependsOn': [ref(x) for x in d.get('dependsOn', []) if ref(x) in known]})

    component['components'] = nested
    component['externalReferences'].append({
        'type': 'bom',
        'url': expand(meta['sbom_url'], component['version'], variant),
        'hashes': [{'alg': 'SHA-256', 'content': hashlib.sha256(raw).hexdigest()}],
        'comment': 'The SBOM this release of the component publishes for itself, nested here as its '
                   'components. The build checked it against the release\'s SHA256SUMS, whose digest is '
                   f'recorded in {MANIFEST_NAME}.',
    })
    component['properties'].append(prop('nested-sbom', f'{len(nested)} components'))
    return dependencies


def build_sbom(args, manifest, zip_names):
    components = []
    for spec in args.component:
        name, sep, version = spec.partition('=')
        if not sep or not name or not version:
            raise SbomError(f'--component expects NAME=VERSION, got {spec!r}')
        components.append(manifest_component(manifest, name, version))
    if args.python_runtime:
        components.append(python_component(args.python_runtime))
    for spec in args.nuget:
        name, sep, version = spec.partition('=')
        if not sep or not name or not version:
            raise SbomError(f'--nuget expects NAME=VERSION, got {spec!r}')
        dll = f'{name}.dll'.lower()
        if zip_names is not None and not any(os.path.basename(n).lower() == dll for n in zip_names):
            print(f'note: {name} is not in the zip, so it is left out', file=sys.stderr)
            continue
        components.append(nuget_component(name, version, args.nuget_root))
    if args.npm_lock:
        components.extend(npm_components(args.npm_lock))

    refs = [c['bom-ref'] for c in components]
    nested_dependencies = []
    for spec in args.nested_sbom:
        name, sep, path = spec.partition('=')
        if not sep or not name or not path:
            raise SbomError(f'--nested-sbom expects NAME=PATH, got {spec!r}')
        target = [c for c in components
                  if {'name': 'nscp:manifest-name', 'value': name} in c.get('properties', [])]
        if not target:
            raise SbomError(f'--nested-sbom {name}: no --component {name}=... to nest it under')
        nested_dependencies += nest_sbom(target[0], path)

    all_refs = [c['bom-ref'] for c in _walk(components) if 'bom-ref' in c]
    if len(set(all_refs)) != len(all_refs):
        raise SbomError('the same component was given twice')

    epoch = os.environ.get('SOURCE_DATE_EPOCH')
    when = datetime.datetime.fromtimestamp(int(epoch), datetime.timezone.utc) if epoch \
        else datetime.datetime.now(datetime.timezone.utc)

    root = {
        'type': 'application',
        'bom-ref': 'nscp',
        'supplier': {'name': 'NSClient++', 'url': ['https://nsclient.org']},
        'name': 'NSClient++',
        'version': args.version,
        'licenses': [{'expression': 'Apache-2.0 OR GPL-2.0-only'}],
        'purl': f'pkg:github/mickem/nscp@{args.version}',
        'externalReferences': [
            {'type': 'vcs', 'url': 'https://github.com/mickem/nscp'},
            {'type': 'website', 'url': 'https://nsclient.org'},
        ],
        'properties': [prop('platform', args.platform)],
    }
    if args.commit:
        root['externalReferences'][0]['comment'] = f'Built from commit {args.commit}'
        root['properties'].append(prop('git-commit', args.commit))
    run_id = os.environ.get('GITHUB_RUN_ID')
    if run_id and os.environ.get('GITHUB_REPOSITORY'):
        server = os.environ.get('GITHUB_SERVER_URL', 'https://github.com')
        root['externalReferences'].append(
            {'type': 'build-system', 'url': f'{server}/{os.environ["GITHUB_REPOSITORY"]}/actions/runs/{run_id}'})

    return {
        'bomFormat': 'CycloneDX',
        'specVersion': '1.6',
        'serialNumber': f'urn:uuid:{uuid.uuid4()}',
        'version': 1,
        'metadata': {
            'timestamp': when.strftime('%Y-%m-%dT%H:%M:%SZ'),
            'tools': {'components': [{'type': 'application', 'group': 'nscp', 'name': 'build/python/sbom.py'}]},
            'component': root,
        },
        'components': components,
        'dependencies': [{'ref': 'nscp', 'dependsOn': refs}] + nested_dependencies,
    }


def add_to_zip(zip_path, sbom_bytes):
    """Add sbom.cdx.json and SHA256SUMS to the zip, replacing earlier copies."""
    with zipfile.ZipFile(zip_path) as z:
        names = [i.filename for i in z.infolist()]
    if SBOM_ENTRY in names or SUMS_ENTRY in names:
        # zipfile cannot replace an entry in place; rewrite the zip without them.
        fd, tmp = tempfile.mkstemp(suffix='.zip', dir=os.path.dirname(os.path.abspath(zip_path)))
        os.close(fd)
        with zipfile.ZipFile(zip_path) as src, zipfile.ZipFile(tmp, 'w', zipfile.ZIP_DEFLATED) as dst:
            for info in src.infolist():
                if info.filename not in (SBOM_ENTRY, SUMS_ENTRY):
                    dst.writestr(info, src.read(info))
        os.replace(tmp, zip_path)

    sums = io.StringIO()
    with zipfile.ZipFile(zip_path) as z:
        entries = sorted((i for i in z.infolist() if not i.is_dir()), key=lambda i: i.filename)
        for info in entries:
            digest = hashlib.sha256()
            with z.open(info) as f:
                for chunk in iter(lambda: f.read(1 << 20), b''):
                    digest.update(chunk)
            sums.write(f'{digest.hexdigest()}  {info.filename}\n')
    sums.write(f'{hashlib.sha256(sbom_bytes).hexdigest()}  {SBOM_ENTRY}\n')

    with zipfile.ZipFile(zip_path, 'a', zipfile.ZIP_DEFLATED) as z:
        z.writestr(SBOM_ENTRY, sbom_bytes)
        z.writestr(SUMS_ENTRY, sums.getvalue().encode('utf-8'))


def default_nuget_root():
    return os.environ.get('NUGET_PACKAGES') or os.path.join(os.path.expanduser('~'), '.nuget', 'packages')


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--manifest', default=DEFAULT_MANIFEST, help=f'the checksum manifest (default: {MANIFEST_NAME})')
    parser.add_argument('--check', action='store_true',
                        help='only check that the manifest and the component metadata agree')
    parser.add_argument('--version', help='the NSClient++ version being built')
    parser.add_argument('--platform', help='the build platform, as in the artifact names (x64, Win32, ARM64, ...)')
    parser.add_argument('--commit', help='the git commit being built')
    parser.add_argument('--component', action='append', default=[], metavar='NAME=VERSION',
                        help='a dependency the build used, by its name in the manifest; repeat for each')
    parser.add_argument('--python-runtime', metavar='VERSION', help='the exact version of the embedded CPython')
    parser.add_argument('--nuget', action='append', default=[], metavar='NAME=VERSION',
                        help='a NuGet package shipped as <NAME>.dll; left out when --zip lacks the DLL')
    parser.add_argument('--nuget-root', default=default_nuget_root(),
                        help='the NuGet global packages folder, for the recorded package hashes')
    parser.add_argument('--nested-sbom', action='append', default=[], metavar='NAME=PATH',
                        help='the CycloneDX SBOM a --component publishes for itself, verified by the caller; '
                             'its components are nested under that component')
    parser.add_argument('--npm-lock', metavar='PATH', help='package-lock.json of the bundled web UI')
    parser.add_argument('--zip', metavar='PATH', help=f'add {SBOM_ENTRY} and {SUMS_ENTRY} to this zip')
    parser.add_argument('--output', metavar='PATH', help='where to write the SBOM')
    args = parser.parse_args(argv)
    if not args.check:
        missing = [o for o in ('version', 'platform', 'output') if not getattr(args, o)]
        if missing:
            parser.error('the following arguments are required: ' + ', '.join('--' + m for m in missing))
    return args


def main(argv=None):
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        manifest = parse_manifest(args.manifest)
        if args.check:
            problems = check_manifest(manifest)
            for problem in problems:
                print(f'error: {problem}', file=sys.stderr)
            if not problems:
                print(f'{len(manifest)} dependencies in {MANIFEST_NAME}, all recorded and described')
            return 1 if problems else 0

        zip_names = zip_file_names(args.zip) if args.zip else None
        sbom = build_sbom(args, manifest, zip_names)
        data = (json.dumps(sbom, indent=2) + '\n').encode('utf-8')
        with open(args.output, 'wb') as f:
            f.write(data)
        if args.zip:
            add_to_zip(args.zip, data)
    except SbomError as e:
        print(f'error: {e}', file=sys.stderr)
        return 1

    print(f'Wrote {args.output}: {len(sbom["components"])} components')
    npm = 0
    for c in sbom['components']:
        if c['purl'].startswith('pkg:npm/'):
            npm += 1
            continue
        verification = next((p['value'] for p in c['properties'] if p['name'] == 'nscp:verification'), '')
        nested = f'  (+{len(c["components"])} nested)' if c.get('components') else ''
        print(f'  {c["name"]:<22} {c["version"]:<10} {verification}{nested}')
    if npm:
        print(f'  plus {npm} npm packages bundled in the web UI')
    if args.zip:
        print(f'Added {SBOM_ENTRY} and {SUMS_ENTRY} to {args.zip}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
