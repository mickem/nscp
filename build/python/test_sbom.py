#!/usr/bin/env python3
"""Unit tests for sbom.py. Run with: python build/python/test_sbom.py"""

import base64
import contextlib
import hashlib
import io
import json
import os
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sbom  # noqa: E402

DIGEST_A = 'a' * 64
DIGEST_B = 'b' * 64
COMMIT = 'c' * 40

MANIFEST = f"""\
# comment line
openssl         3.5.8     {DIGEST_A}  # matches upstream
lua             5.4.8     unrecorded
tinyxml2        10.1.0    {COMMIT}
check_nsclient-x64          1.1.1   {DIGEST_B}
check_nsclient-linux-arm64  1.1.1   {DIGEST_B}
"""


def run(argv):
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        code = sbom.main(argv)
    return code, out.getvalue(), err.getvalue()


class SbomTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = self.tmp.name
        self.manifest = self.path('checksums.txt')
        self.write(self.manifest, MANIFEST)
        self.output = self.path('out.cdx.json')

    def tearDown(self):
        self.tmp.cleanup()

    def path(self, name):
        return os.path.join(self.dir, name)

    def write(self, path, text):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w', encoding='utf-8') as f:
            f.write(text)

    def generate(self, *extra):
        code, out, err = run(['--manifest', self.manifest, '--version', '1.2.3', '--platform', 'x64',
                              '--commit', COMMIT, '--output', self.output, *extra])
        self.assertEqual(code, 0, err)
        with open(self.output, encoding='utf-8') as f:
            return json.load(f), err

    def component(self, bom, ref):
        return next(c for c in bom['components'] if c['bom-ref'] == ref)

    def test_repository_manifest_is_fully_described(self):
        code, out, err = run(['--check'])
        self.assertEqual(code, 0, err)

    def test_download_carries_url_and_recorded_digest(self):
        bom, _ = self.generate('--component', 'openssl=3.5.8')
        c = self.component(bom, 'openssl@3.5.8')
        self.assertEqual(c['hashes'], [{'alg': 'SHA-256', 'content': DIGEST_A}])
        dist = c['externalReferences'][0]
        self.assertEqual(dist['type'], 'distribution')
        self.assertEqual(dist['url'], 'https://github.com/openssl/openssl/releases/download/'
                                      'openssl-3.5.8/openssl-3.5.8.tar.gz')
        self.assertEqual(dist['hashes'][0]['content'], DIGEST_A)
        props = {p['name']: p['value'] for p in c['properties']}
        self.assertEqual(props['nscp:verification'], 'sha256')
        self.assertEqual(props['nscp:verification-note'], 'matches upstream')
        self.assertEqual(bom['dependencies'], [{'ref': 'nscp', 'dependsOn': ['openssl@3.5.8']}])
        self.assertEqual(bom['specVersion'], '1.6')

    def test_git_dependency_records_commit_not_a_hash(self):
        bom, _ = self.generate('--component', 'tinyxml2=10.1.0')
        c = self.component(bom, 'tinyxml2@10.1.0')
        self.assertNotIn('hashes', c)
        props = {p['name']: p['value'] for p in c['properties']}
        self.assertEqual(props['nscp:git-commit'], COMMIT)
        self.assertEqual(props['nscp:verification'], 'git-commit')
        self.assertEqual(c['externalReferences'][0]['type'], 'vcs')

    def test_variant_selects_the_right_url(self):
        bom, _ = self.generate('--component', 'check_nsclient-x64=1.1.1')
        c = self.component(bom, 'check_nsclient-x64@1.1.1')
        self.assertTrue(c['externalReferences'][0]['url'].endswith('check_nsclient-1.1.1-windows-x64.exe'))
        key, _, variant = sbom.lookup('check_nsclient-linux-arm64')
        self.assertEqual((key, variant), ('check_nsclient-linux', 'arm64'))

    def test_unrecorded_is_listed_as_unverified(self):
        bom, err = self.generate('--component', 'lua=5.4.8')
        c = self.component(bom, 'lua@5.4.8')
        self.assertNotIn('hashes', c)
        self.assertIn({'name': 'nscp:verification', 'value': 'none'}, c['properties'])
        self.assertIn('unrecorded', err)

    def test_version_missing_from_manifest_fails(self):
        code, _, err = run(['--manifest', self.manifest, '--version', '1', '--platform', 'x64',
                            '--output', self.output, '--component', 'openssl=9.9.9'])
        self.assertEqual(code, 1)
        self.assertIn('no line', err)
        self.assertFalse(os.path.exists(self.output))

    def test_unknown_dependency_fails(self):
        self.assertRaises(sbom.SbomError, sbom.lookup, 'libfoo')

    def test_check_reports_unrecorded_and_undescribed(self):
        self.write(self.manifest, MANIFEST + f'libfoo 1.0 {DIGEST_A}\nprotobuf 1.0 {COMMIT}\n')
        code, _, err = run(['--manifest', self.manifest, '--check'])
        self.assertEqual(code, 1)
        self.assertIn('lua 5.4.8 is unrecorded', err)
        self.assertIn('libfoo has no entry', err)
        self.assertIn('protobuf 1.0 is a download, so it needs a SHA-256', err)
        self.assertIn('boost is described', err)

    def test_npm_lock_keeps_runtime_packages_only(self):
        lock = self.path('package-lock.json')
        integrity = 'sha512-' + base64.b64encode(hashlib.sha512(b'react').digest()).decode()
        self.write(lock, json.dumps({'lockfileVersion': 3, 'packages': {
            '': {'name': 'web'},
            'node_modules/react': {'version': '19.0.0', 'license': 'MIT', 'integrity': integrity,
                                   'resolved': 'https://registry.npmjs.org/react/-/react-19.0.0.tgz'},
            'node_modules/@mui/material': {'version': '9.0.0', 'license': 'MIT'},
            'node_modules/a/node_modules/react': {'version': '19.0.0', 'integrity': integrity},
            'node_modules/vite': {'version': '7.0.0', 'dev': True},
            'node_modules/fsevents': {'version': '2.3.3', 'devOptional': True},
        }}))
        bom, _ = self.generate('--npm-lock', lock)
        refs = [c['bom-ref'] for c in bom['components']]
        self.assertEqual(refs, ['pkg:npm/%40mui/material@9.0.0', 'pkg:npm/react@19.0.0'])
        react = self.component(bom, 'pkg:npm/react@19.0.0')
        self.assertEqual(react['hashes'], [{'alg': 'SHA-512', 'content': hashlib.sha512(b'react').hexdigest()}])
        mui = self.component(bom, 'pkg:npm/%40mui/material@9.0.0')
        self.assertEqual((mui['group'], mui['name']), ('@mui', 'material'))

    def make_zip(self, files):
        path = self.path('NSCP.zip')
        with zipfile.ZipFile(path, 'w') as z:
            for name, data in files.items():
                z.writestr(name, data)
        return path

    def test_zip_gets_sbom_and_checksums(self):
        zip_path = self.make_zip({'nscp.exe': b'exe', 'modules/CheckDisk.dll': b'dll'})
        self.generate('--component', 'openssl=3.5.8', '--zip', zip_path)
        # A second run (a re-run job step) replaces the entries instead of duplicating them.
        self.generate('--component', 'openssl=3.5.8', '--zip', zip_path)
        with zipfile.ZipFile(zip_path) as z:
            names = z.namelist()
            self.assertEqual(sorted(names), sorted(['nscp.exe', 'modules/CheckDisk.dll',
                                                    sbom.SBOM_ENTRY, sbom.SUMS_ENTRY]))
            with open(self.output, 'rb') as f:
                self.assertEqual(z.read(sbom.SBOM_ENTRY), f.read())
            sums = dict(reversed(line.split('  ', 1)) for line in
                        z.read(sbom.SUMS_ENTRY).decode().splitlines())
            for name in ('nscp.exe', 'modules/CheckDisk.dll', sbom.SBOM_ENTRY):
                self.assertEqual(sums[name], hashlib.sha256(z.read(name)).hexdigest())
            self.assertNotIn(sbom.SUMS_ENTRY, sums)

    def test_nuget_hash_and_presence_in_zip(self):
        root = self.path('nuget')
        digest = hashlib.sha512(b'nupkg').digest()
        self.write(os.path.join(root, 'google.protobuf', '3.36.1', 'google.protobuf.3.36.1.nupkg.sha512'),
                   base64.b64encode(digest).decode())
        with_dll = self.make_zip({'modules/dotnet/Google.Protobuf.dll': b'x'})
        bom, _ = self.generate('--nuget', 'Google.Protobuf=3.36.1', '--nuget-root', root, '--zip', with_dll)
        c = self.component(bom, 'pkg:nuget/Google.Protobuf@3.36.1')
        self.assertEqual(c['hashes'], [{'alg': 'SHA-512', 'content': digest.hex()}])
        self.assertEqual(c['licenses'], [{'license': {'id': 'BSD-3-Clause'}}])

        without_dll = self.make_zip({'nscp.exe': b'x'})
        bom, _ = self.generate('--nuget', 'Google.Protobuf=3.36.1', '--nuget-root', root, '--zip', without_dll)
        self.assertEqual(bom['components'], [])

    def test_nuget_without_recorded_hash_is_unverified(self):
        bom, err = self.generate('--nuget', 'Google.Protobuf=3.36.1', '--nuget-root', self.path('empty'))
        c = self.component(bom, 'pkg:nuget/Google.Protobuf@3.36.1')
        self.assertNotIn('hashes', c)
        self.assertIn({'name': 'nscp:verification', 'value': 'none'}, c['properties'])
        self.assertIn('without a hash', err)

    def nested_sbom(self, version='1.1.1', name='check_nsclient'):
        path = self.path('inner.cdx.json')
        root = 'path+file:///build#' + version
        self.write(path, json.dumps({
            'bomFormat': 'CycloneDX', 'specVersion': '1.5',
            'metadata': {'component': {'bom-ref': root, 'name': name, 'version': version}},
            'components': [
                {'type': 'library', 'bom-ref': 'crate#clap@4', 'name': 'clap', 'version': '4',
                 'hashes': [{'alg': 'SHA-256', 'content': DIGEST_A}]},
                {'type': 'library', 'bom-ref': 'crate#anyhow@1', 'name': 'anyhow', 'version': '1'},
            ],
            'dependencies': [
                {'ref': root, 'dependsOn': ['crate#clap@4', 'crate#anyhow@1']},
                {'ref': 'crate#clap@4', 'dependsOn': ['crate#anyhow@1', 'crate#not-listed@9']},
                {'ref': 'crate#not-listed@9', 'dependsOn': []},
            ],
        }))
        return path

    def test_nested_sbom_hangs_under_its_component(self):
        path = self.nested_sbom()
        bom, _ = self.generate('--component', 'check_nsclient-x64=1.1.1',
                               '--nested-sbom', f'check_nsclient-x64={path}')
        c = self.component(bom, 'check_nsclient-x64@1.1.1')
        self.assertEqual([n['bom-ref'] for n in c['components']],
                         ['check_nsclient-x64@1.1.1|crate#clap@4', 'check_nsclient-x64@1.1.1|crate#anyhow@1'])
        self.assertEqual(c['components'][0]['hashes'][0]['content'], DIGEST_A)
        ref = next(r for r in c['externalReferences'] if r['type'] == 'bom')
        self.assertTrue(ref['url'].endswith('check_nsclient-1.1.1-windows-x64.cdx.json'))
        with open(path, 'rb') as f:
            self.assertEqual(ref['hashes'][0]['content'], hashlib.sha256(f.read()).hexdigest())
        # Their root maps onto our component; refs outside the nested tree are dropped.
        deps = {d['ref']: d['dependsOn'] for d in bom['dependencies']}
        self.assertEqual(deps['check_nsclient-x64@1.1.1'],
                         ['check_nsclient-x64@1.1.1|crate#clap@4', 'check_nsclient-x64@1.1.1|crate#anyhow@1'])
        self.assertEqual(deps['check_nsclient-x64@1.1.1|crate#clap@4'], ['check_nsclient-x64@1.1.1|crate#anyhow@1'])
        self.assertNotIn('check_nsclient-x64@1.1.1|crate#not-listed@9', deps)
        self.assertEqual(deps['nscp'], ['check_nsclient-x64@1.1.1'])

    def test_nested_sbom_for_another_version_is_refused(self):
        path = self.nested_sbom(version='9.9.9')
        code, _, err = run(['--manifest', self.manifest, '--version', '1', '--platform', 'x64',
                            '--output', self.output, '--component', 'check_nsclient-x64=1.1.1',
                            '--nested-sbom', f'check_nsclient-x64={path}'])
        self.assertEqual(code, 1)
        self.assertIn('describes check_nsclient 9.9.9', err)

    def test_nested_sbom_needs_its_component(self):
        code, _, err = run(['--manifest', self.manifest, '--version', '1', '--platform', 'x64',
                            '--output', self.output, '--nested-sbom', f'check_nsclient-x64={self.nested_sbom()}'])
        self.assertEqual(code, 1)
        self.assertIn('no --component check_nsclient-x64', err)

    def test_checksum_list_is_not_a_component(self):
        self.write(self.manifest, MANIFEST + f'check_nsclient-sha256sums 1.1.1 {DIGEST_A}\n')
        code, _, err = run(['--manifest', self.manifest, '--version', '1', '--platform', 'x64',
                            '--output', self.output, '--component', 'check_nsclient-sha256sums=1.1.1'])
        self.assertEqual(code, 1)
        self.assertIn('checksum list, not a component', err)

    def test_python_runtime_and_root_metadata(self):
        bom, _ = self.generate('--python-runtime', '3.11.9')
        c = self.component(bom, 'cpython@3.11.9')
        self.assertEqual(c['cpe'], 'cpe:2.3:a:python:python:3.11.9:*:*:*:*:*:*:*')
        root = bom['metadata']['component']
        self.assertEqual((root['name'], root['version']), ('NSClient++', '1.2.3'))
        self.assertIn({'name': 'nscp:git-commit', 'value': COMMIT}, root['properties'])
        self.assertIn({'name': 'nscp:platform', 'value': 'x64'}, root['properties'])


if __name__ == '__main__':
    unittest.main()
