#!/usr/bin/env python3
"""Unit tests for scripts/python/docs_merge.py. Run with: python build/python/test_docs_merge.py"""

import contextlib
import io
import os
import sys
import tempfile
import unittest

import yaml

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'scripts', 'python'))
import docs_merge  # noqa: E402


def query(desc, **args):
    return {'info': {'description': desc}, 'arguments': args}


# A module as docs_extract.py writes it after a Windows and a Linux run: one
# query identical on both, one that differs, one Windows-only.
COMMITTED = {
    'module': 'CheckSample',
    'namespace': 'check',
    'platforms': ['unix', 'windows'],
    'common': {
        'info': {'description': 'Sample checks'},
        'queries': {'check_shared': query('shared', a='1')},
    },
    'unix': {'queries': {'check_diff': query('linux flavour')}},
    'windows': {'queries': {'check_diff': query('windows flavour'),
                            'check_win': query('windows only')}},
}


def write(directory, name, data, header=''):
    os.makedirs(directory, exist_ok=True)
    with open(os.path.join(directory, name), 'w', encoding='utf-8', newline='\n') as f:
        f.write(header + docs_merge.dump_yaml(data))


def read(directory, name):
    with open(os.path.join(directory, name), encoding='utf-8') as f:
        return yaml.safe_load(f)


def extract_on(platform, committed, fresh_tree):
    """What docs_extract.py leaves on disk after a run on `platform`."""
    trees = docs_merge.expand(committed)
    trees[platform] = fresh_tree
    for tree in trees.values():
        tree['module'] = fresh_tree['module']
        tree['namespace'] = fresh_tree['namespace']
    return docs_merge.factor(trees)


class FactoringTest(unittest.TestCase):
    def test_round_trip(self):
        trees = docs_merge.expand(COMMITTED)
        self.assertEqual(sorted(trees), ['unix', 'windows'])
        self.assertEqual(docs_merge.factor(trees), COMMITTED)

    def test_lone_platform_has_no_common_section(self):
        tree = docs_merge.expand_platform(COMMITTED, 'unix')
        data = docs_merge.factor({'unix': tree})
        self.assertNotIn('common', data)
        self.assertEqual(data['platforms'], ['unix'])
        self.assertEqual(docs_merge.expand_platform(data, 'unix'), tree)


class MergeTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = self.tmp.name
        self.reference = os.path.join(self.root, 'reference')
        self.runs = {p: os.path.join(self.root, p) for p in ('windows', 'unix', 'darwin')}
        write(self.reference, 'CheckSample.yaml', COMMITTED)
        write(self.reference, 'common-options.yaml',
              {'options': {'filter': {'description': 'old filter', 'content_type': 'string', 'group': 'filter'}},
               'fields': {'count': 'Number of items'}},
              header='# The header docs_extract.py writes.\n# Two lines of it.\n')
        for directory in self.runs.values():
            os.makedirs(directory)

    def tearDown(self):
        self.tmp.cleanup()

    def run_merge(self, *platforms):
        args = ['--reference', self.reference]
        for p in platforms:
            args += ['--slice', '%s=%s' % (p, self.runs[p])]
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rc = docs_merge.main(args)
        self.assertEqual(rc, 0, out.getvalue())
        return out.getvalue()

    def test_each_platform_comes_from_its_own_run(self):
        # Windows changed the differing query, Linux gained a query and macOS
        # was extracted for the first time. Every other run still carries the
        # committed data for the platforms it did not run on.
        win_tree = docs_merge.expand_platform(COMMITTED, 'windows')
        win_tree['queries']['check_diff'] = query('windows flavour v2')
        write(self.runs['windows'], 'CheckSample.yaml', extract_on('windows', COMMITTED, win_tree))

        unix_tree = docs_merge.expand_platform(COMMITTED, 'unix')
        unix_tree['queries']['check_new'] = query('new on linux')
        write(self.runs['unix'], 'CheckSample.yaml', extract_on('unix', COMMITTED, unix_tree))

        mac_tree = docs_merge.expand_platform(COMMITTED, 'unix')
        mac_tree['queries']['check_diff'] = query('macos flavour')
        write(self.runs['darwin'], 'CheckSample.yaml', extract_on('darwin', COMMITTED, mac_tree))

        out = self.run_merge('windows', 'unix', 'darwin')
        self.assertIn('updated   CheckSample (windows, unix, darwin)', out)

        merged = read(self.reference, 'CheckSample.yaml')
        self.assertEqual(merged['platforms'], ['darwin', 'unix', 'windows'])
        trees = docs_merge.expand(merged)
        self.assertEqual(trees['windows'], win_tree)
        self.assertEqual(trees['unix'], unix_tree)
        self.assertEqual(trees['darwin'], mac_tree)
        # The shared query is still factored out, the rest is per platform.
        self.assertEqual(merged['common']['queries'], {'check_shared': query('shared', a='1')})
        self.assertEqual(merged['windows']['queries']['check_win'], query('windows only'))
        self.assertNotIn('check_win', merged.get('unix', {}).get('queries', {}))

    def test_sequential_and_parallel_runs_agree(self):
        # The point of the merge: three parallel runs must produce what three
        # runs on one checkout, one after the other, would have produced.
        fresh = {}
        for p in ('windows', 'unix', 'darwin'):
            tree = docs_merge.expand_platform(COMMITTED, 'windows' if p == 'windows' else 'unix')
            tree['queries']['check_diff'] = query('%s v2' % p)
            fresh[p] = tree
            write(self.runs[p], 'CheckSample.yaml', extract_on(p, COMMITTED, tree))
        self.run_merge('windows', 'unix', 'darwin')
        parallel = read(self.reference, 'CheckSample.yaml')

        sequential = COMMITTED
        for p in ('windows', 'unix', 'darwin'):
            sequential = extract_on(p, sequential, fresh[p])
        self.assertEqual(parallel, sequential)

    def test_unchanged_files_are_left_alone(self):
        for p in ('windows', 'unix'):
            write(self.runs[p], 'CheckSample.yaml', COMMITTED)
        path = os.path.join(self.reference, 'CheckSample.yaml')
        before = os.stat(path).st_mtime_ns
        os.utime(path, ns=(before - 10 ** 9, before - 10 ** 9))
        stamped = os.stat(path).st_mtime_ns
        out = self.run_merge('windows', 'unix')
        self.assertIn('unchanged CheckSample (windows, unix)', out)
        self.assertIn('0 file(s) changed', out)
        self.assertEqual(os.stat(path).st_mtime_ns, stamped)

    def test_run_that_never_saw_a_module_keeps_the_committed_slice(self):
        # macOS is built without CheckSample: its copy of the file is the
        # committed one, whose platform list does not include darwin, so the
        # merge takes nothing from it - and drops nothing either.
        write(self.runs['darwin'], 'CheckSample.yaml', COMMITTED)
        out = self.run_merge('darwin')
        self.assertIn('unchanged CheckSample (no fresh slice)', out)
        self.assertEqual(read(self.reference, 'CheckSample.yaml'), COMMITTED)

    def test_new_module_and_missing_files(self):
        # A module that only one run knows is created from that run; a run
        # that has no file for a module contributes nothing to it.
        only_mac = {'module': 'CheckDarwin', 'namespace': 'check', 'platforms': ['darwin'],
                    'darwin': {'info': {'description': 'macOS'}, 'queries': {'check_mac': query('mac')}}}
        write(self.runs['darwin'], 'CheckDarwin.yaml', only_mac)
        write(self.runs['windows'], 'CheckSample.yaml', COMMITTED)
        out = self.run_merge('windows', 'darwin')
        self.assertIn('updated   CheckDarwin (darwin)', out)
        self.assertIn('unchanged CheckSample (windows)', out)
        self.assertEqual(read(self.reference, 'CheckDarwin.yaml'), only_mac)

    def test_fresh_module_identity_wins(self):
        tree = docs_merge.expand_platform(COMMITTED, 'unix')
        tree['namespace'] = 'generic'
        moved = extract_on('unix', COMMITTED, tree)
        write(self.runs['unix'], 'CheckSample.yaml', moved)
        self.run_merge('unix')
        merged = read(self.reference, 'CheckSample.yaml')
        self.assertEqual(merged['namespace'], 'generic')
        self.assertEqual(merged['platforms'], ['unix', 'windows'])

    def test_common_options_union_keeps_header(self):
        write(self.runs['windows'], 'common-options.yaml',
              {'options': {'filter': {'description': 'new filter', 'content_type': 'string', 'group': 'filter'},
                           'top-syntax': {'description': 'top', 'content_type': 'string', 'group': 'filter'}},
               'fields': {'count': 'Number of items'}},
              header='# A copy of the header.\n')
        write(self.runs['unix'], 'common-options.yaml',
              {'options': {'filter': {'description': 'new filter', 'content_type': 'string', 'group': 'filter'}},
               'fields': {'count': 'Number of items', 'status': 'The status'}})
        out = self.run_merge('windows', 'unix')
        self.assertIn('updated   common-options.yaml', out)
        path = os.path.join(self.reference, 'common-options.yaml')
        with open(path, encoding='utf-8') as f:
            text = f.read()
        self.assertTrue(text.startswith('# The header docs_extract.py writes.\n# Two lines of it.\n'), text)
        merged = yaml.safe_load(text)
        self.assertEqual(sorted(merged['options']), ['filter', 'top-syntax'])
        self.assertEqual(merged['options']['filter']['description'], 'new filter')
        self.assertEqual(merged['fields'], {'count': 'Number of items', 'status': 'The status'})

    def test_diverging_common_description_is_reported(self):
        write(self.runs['windows'], 'common-options.yaml',
              {'options': {'filter': {'description': 'windows says', 'content_type': 'string', 'group': 'filter'}},
               'fields': {}})
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            self.run_merge('windows')
        self.assertIn('option "filter" differs on windows', err.getvalue())
        self.assertEqual(read(self.reference, 'common-options.yaml')['options']['filter']['description'],
                         'windows says')

    def test_rejects_a_platform_given_twice_and_a_bad_slice(self):
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                docs_merge.main(['--reference', self.reference,
                                 '--slice', 'unix=%s' % self.runs['unix'],
                                 '--slice', 'unix=%s' % self.runs['darwin']])
            with self.assertRaises(SystemExit):
                docs_merge.main(['--reference', self.reference, '--slice', 'unix'])
            with self.assertRaises(SystemExit):
                docs_merge.main(['--reference', self.reference,
                                 '--slice', 'unix=%s' % os.path.join(self.root, 'missing')])


if __name__ == '__main__':
    unittest.main()
