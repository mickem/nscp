#!/usr/bin/env python3
"""Unit tests for the data-source sidecars (docs/samples/*_sources.yaml):
the schema in scripts/python/docs_sources.py and their rendering by
scripts/python/docs_generate.py. Run with: python build/python/test_docs_sources.py"""

import contextlib
import io
import json
import os
import sys
import tempfile
import unittest

import yaml

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'scripts', 'python'))
import docs_generate  # noqa: E402
import docs_sources  # noqa: E402


def query(desc):
    return {'info': {'description': desc}}


# A module as docs_extract.py writes it: check_both on Windows and Linux,
# check_win on Windows only.
REFERENCE = {
    'module': 'CheckSample',
    'namespace': 'check',
    'platforms': ['unix', 'windows'],
    'common': {
        'info': {'description': 'Sample checks'},
        'queries': {'check_both': dict(query('Check on both'), params=[
                        {'name': 'unit', 'default_value': 'percent', 'long_description': 'Unit', 'is_simple': True}]),
                    'check_none': query('Undocumented check')},
    },
    'windows': {'queries': {'check_win': query('Windows check')}},
}

VALID = {
    'windows': [{'source': 'Windows power APIs', 'provides': 'charge, power source', 'privilege': 'none'},
                {'source': 'WMI `root\\cimv2`', 'privilege': 'conditional',
                 'note': 'Only for the `health` keyword', 'without': 'health is unknown',
                 'requires': ['WMI service']}],
    'linux': [{'source': '/sys/class/power_supply'}],
}


class Tree(object):
    """A throw-away docs root: reference/, samples/ and modules/."""

    def __init__(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = self.tmp.name
        self.reference = os.path.join(self.root, 'reference')
        self.samples = os.path.join(self.root, 'samples')
        self.modules = os.path.join(self.root, 'modules')
        for d in (self.reference, self.samples, self.modules):
            os.makedirs(d)
        with open(os.path.join(self.reference, 'CheckSample.yaml'), 'w', encoding='utf-8') as f:
            yaml.safe_dump(REFERENCE, f)

    def sidecar(self, name, data):
        path = os.path.join(self.samples, name)
        with open(path, 'w', encoding='utf-8') as f:
            if isinstance(data, str):
                f.write(data)
            else:
                yaml.safe_dump(data, f, allow_unicode=True)
        return path

    def module_json(self, directory, name, commands):
        os.makedirs(os.path.join(self.modules, directory))
        with open(os.path.join(self.modules, directory, 'module.json'), 'w', encoding='utf-8') as f:
            json.dump({'module': {'name': name}, 'commands': commands}, f)

    def check(self):
        return docs_sources.check(self.samples, self.reference, self.modules)

    def generate(self):
        out = os.path.join(self.root, 'out')
        with contextlib.redirect_stdout(io.StringIO()):
            docs_generate.DocumentationGenerator(self.reference, self.root).generate(out)
        return out

    def page(self, out, rel):
        with open(os.path.join(out, 'docs', 'reference', rel), encoding='utf-8') as f:
            return f.read()

    def close(self):
        self.tmp.cleanup()


def section(page, command):
    # The text of one command's "### <command>" section.
    start = page.index('### %s\n' % command)
    end = page.find('\n### ', start + 1)
    return page[start:end if end >= 0 else len(page)]


class ValidateTest(unittest.TestCase):
    def errors(self, data):
        return docs_sources.validate(data)

    def assertError(self, data, fragment):
        errors = self.errors(data)
        self.assertTrue(any(fragment in e for e in errors), '%r not in %r' % (fragment, errors))

    def test_valid(self):
        self.assertEqual(self.errors(VALID), [])
        self.assertEqual(self.errors({'all': [{'source': 'procfs'}]}), [])

    def test_top_level(self):
        self.assertError(None, 'top level must be a mapping')
        self.assertError({}, 'top level must be a mapping')
        self.assertError([{'source': 'x'}], 'top level must be a mapping')

    def test_unknown_platform(self):
        self.assertError({'unix': [{'source': 'x'}]}, "unknown platform 'unix'")

    def test_empty_or_wrong_list(self):
        self.assertError({'linux': []}, 'linux: must be a non-empty list')
        self.assertError({'linux': None}, 'linux: must be a non-empty list')
        self.assertError({'linux': {'source': 'x'}}, 'linux: must be a non-empty list')
        self.assertError({'linux': ['x']}, 'linux[1]: must be a mapping')

    def test_unknown_key(self):
        self.assertError({'all': [{'source': 'x', 'privileges': 'none'}]}, "unknown key 'privileges'")

    def test_source_required(self):
        self.assertError({'all': [{'provides': 'x'}]}, '"source" is required')

    def test_non_string_values(self):
        self.assertError({'all': [{'source': 42}]}, 'source must be a non-empty string')
        self.assertError({'all': [{'source': 'x', 'note': True}]}, 'note must be a non-empty string')
        self.assertError({'all': [{'source': 'x', 'privilege': None}]}, 'privilege must be a non-empty string')
        self.assertError({'all': [{'source': ' '}]}, 'source must be a non-empty string')

    def test_unknown_privilege(self):
        self.assertError({'all': [{'source': 'x', 'privilege': 'root'}]}, "unknown privilege 'root'")

    def test_conditional_needs_note(self):
        self.assertError({'all': [{'source': 'x', 'privilege': 'conditional'}]}, 'needs a "note"')
        self.assertEqual(self.errors({'all': [{'source': 'x', 'privilege': 'conditional', 'note': 'n'}]}), [])

    def test_requires(self):
        self.assertError({'all': [{'source': 'x', 'requires': []}]}, 'requires must be a non-empty list')
        self.assertError({'all': [{'source': 'x', 'requires': 'tool'}]}, 'requires must be a non-empty list')
        self.assertError({'all': [{'source': 'x', 'requires': ['ok', 3]}]}, 'requires entries must be')

    def test_yaml_spelling_traps(self):
        # `privilege: none` is the string "none"; `no`/`yes` are booleans.
        self.assertEqual(self.errors(yaml.safe_load('all:\n- source: x\n  privilege: none\n')), [])
        self.assertError(yaml.safe_load('all:\n- source: x\n  privilege: no\n'), 'privilege must be')

    def test_rank_privilege(self):
        rank = docs_sources.rank_privilege
        self.assertEqual(rank(['none', 'elevated', 'conditional']), 'elevated')
        self.assertEqual(rank(['none', None, 'conditional']), 'conditional')
        self.assertEqual(rank(['none', None]), None)
        self.assertEqual(rank(['none', 'none']), 'none')

    def test_parse_filename(self):
        self.assertEqual(docs_sources.parse_filename('CheckSystem_check_cpu_frequency_sources.yaml'),
                         ('CheckSystem', 'check_cpu_frequency'))
        self.assertIsNone(docs_sources.parse_filename('_check_cpu_sources.yaml'))
        self.assertIsNone(docs_sources.parse_filename('CheckSystem_check_cpu_sources.yml'))


class CheckTest(unittest.TestCase):
    def setUp(self):
        self.tree = Tree()

    def tearDown(self):
        self.tree.close()

    def test_ok(self):
        self.tree.sidecar('CheckSample_check_both_sources.yaml', VALID)
        self.assertEqual(self.tree.check(), ([], 1))

    def test_unknown_module_and_command(self):
        self.tree.sidecar('CheckNothing_check_both_sources.yaml', VALID)
        self.tree.sidecar('CheckSample_check_typo_sources.yaml', VALID)
        errors, count = self.tree.check()
        self.assertEqual(count, 2)
        self.assertIn('CheckNothing_check_both_sources.yaml: no module named CheckNothing', errors[0])
        self.assertIn('CheckSample_check_typo_sources.yaml: CheckSample has no command named check_typo', errors[1])

    def test_bad_filename(self):
        self.tree.sidecar('CheckSample-check_both_sources.yaml', VALID)
        errors, _ = self.tree.check()
        self.assertIn('not named <Module>_<command>_sources.yaml', errors[0])

    def test_module_json_commands_are_known(self):
        # A command not extracted yet, declared by a module whose directory
        # differs from the name it documents under (CheckSystemUnix -> CheckSystem).
        self.tree.module_json('CheckSampleUnix', 'CheckSample', {'check_new': 'New check'})
        self.tree.sidecar('CheckSample_check_new_sources.yaml', VALID)
        self.assertEqual(self.tree.check(), ([], 1))

    def test_schema_errors_are_named_per_file(self):
        self.tree.sidecar('CheckSample_check_both_sources.yaml', {'all': [{'source': 'x', 'privilege': 'root'}]})
        self.tree.sidecar('CheckSample_check_win_sources.yaml', 'all: [unclosed\n')
        errors, _ = self.tree.check()
        self.assertEqual(len(errors), 2)
        self.assertIn("CheckSample_check_both_sources.yaml: all[1]: unknown privilege 'root'", errors[0])
        self.assertIn('CheckSample_check_win_sources.yaml: not valid YAML', errors[1])

    def test_main_exit_code(self):
        self.tree.sidecar('CheckSample_check_both_sources.yaml', {'linux': []})
        argv = ['--check', '--samples', self.tree.samples, '--reference', self.tree.reference,
                '--modules', self.tree.modules]
        with contextlib.redirect_stderr(io.StringIO()) as err:
            self.assertEqual(docs_sources.main(argv), 1)
        self.assertIn('1 data-source sidecars, 1 errors', err.getvalue())


class RenderTest(unittest.TestCase):
    def setUp(self):
        self.tree = Tree()

    def tearDown(self):
        self.tree.close()

    def test_section_rendered_once_with_platform_column(self):
        self.tree.sidecar('CheckSample_check_both_sources.yaml', VALID)
        page = self.tree.page(self.tree.generate(), 'check/CheckSample.md')
        body = section(page, 'check_both')
        # Once per command, not once per platform slice.
        self.assertEqual(body.count('#### Data sources and privileges'), 1)
        self.assertNotIn('=== "', body)
        self.assertIn('* [Data sources and privileges](#check_both_sources)', body)
        self.assertIn('<a id="check_both_sources"></a>', body)
        self.assertIn('| Platform | Source', body)
        self.assertRegex(body, r'\| Windows +\| Windows power APIs +\| charge, power source +\| none ')
        # A note carries the condition and the consequence; the table points at it.
        self.assertRegex(body, r'\| conditional +\| WMI service +\| note 1 \|')
        self.assertIn('1. Only for the `health` keyword *Without it:* health is unknown', body)
        self.assertIn('[all checks by privilege](../data-sources.md)', body)
        # Before the arguments, after the samples.
        self.assertLess(body.index('#### Data sources'), body.index('#### Command-line Arguments'))

    def test_missing_privilege_is_not_determined(self):
        self.tree.sidecar('CheckSample_check_both_sources.yaml', VALID)
        body = section(self.tree.page(self.tree.generate(), 'check/CheckSample.md'), 'check_both')
        self.assertRegex(body, r'\| Linux +\| /sys/class/power_supply +\| +\| \*not determined\* ')

    def test_all_only_drops_platform_and_empty_columns(self):
        self.tree.sidecar('CheckSample_check_both_sources.yaml', {'all': [{'source': 'procfs', 'privilege': 'none'}]})
        body = section(self.tree.page(self.tree.generate(), 'check/CheckSample.md'), 'check_both')
        self.assertIn('| Source | Privilege |', body)
        for column in ('Platform', 'Provides', 'Requires', 'Notes'):
            self.assertNotIn('| %s' % column, body)

    def test_absent_sidecar_renders_nothing(self):
        self.tree.sidecar('CheckSample_check_both_sources.yaml', VALID)
        page = self.tree.page(self.tree.generate(), 'check/CheckSample.md')
        for command in ('check_none', 'check_win'):
            body = section(page, command)
            self.assertNotIn('Data sources', body)
            self.assertNotIn('_sources', body)

    def test_pipes_cannot_break_the_table(self):
        # Escaped in text; inside a code span the table already ignores it.
        self.tree.sidecar('CheckSample_check_both_sources.yaml', {'all': [{'source': 'a | b `x | y`'}]})
        body = section(self.tree.page(self.tree.generate(), 'check/CheckSample.md'), 'check_both')
        self.assertIn('a \\| b `x | y`', body)

    def test_overview_page(self):
        self.tree.sidecar('CheckSample_check_both_sources.yaml', VALID)
        self.tree.sidecar('CheckSample_check_win_sources.yaml',
                          {'all': [{'source': 'Security event log', 'privilege': 'elevated'},
                                   {'source': 'Registry', 'privilege': 'none', 'requires': ['Remote Registry']}]})
        out = self.tree.generate()
        page = self.tree.page(out, 'data-sources.md')
        self.assertTrue(page.startswith('# Data sources and privileges\n'))
        self.assertIn('the account the NSClient++ agent (the service) runs as', page)
        self.assertIn('Commands that are not listed here have not been documented yet', page)
        elevation, everything = page.split('## All documented checks')
        self.assertIn('## Checks that need elevation', elevation)
        # Elevated and conditional sources, nothing else, with the condition.
        self.assertRegex(elevation, r'\| \[check_both\]\(check/CheckSample.md#check_both_sources\) +\| CheckSample +'
                                    r'\| Windows +\| \*\*conditional\*\* +\| WMI `root\\cimv2` +\| Only for the `health` keyword')
        self.assertRegex(elevation, r'\| \[check_win\]\(check/CheckSample.md#check_win_sources\) +\| CheckSample +'
                                    r'\| All \(Windows\) +\| \*\*elevated\*\* +\| Security event log')
        self.assertNotIn('Registry', elevation)
        self.assertNotIn('/sys/class/power_supply', elevation)
        # One row per command and platform; the most demanding source wins.
        self.assertRegex(everything, r'\| Command +\| Module +\| Platform +\| Privilege +\| Dependencies')
        self.assertRegex(everything, r'\[check_both\]\S+ +\| CheckSample +\| Windows +\| conditional +\| WMI service')
        self.assertRegex(everything, r'\[check_both\]\S+ +\| CheckSample +\| Linux +\| \*not determined\* +\| —')
        self.assertRegex(everything, r'\[check_win\]\S+ +\| CheckSample +\| All \(Windows\) +\| elevated +\| Remote Registry')
        self.assertNotIn('check_none', page)
        # Linked from the reference index.
        self.assertIn('[data sources and privileges](data-sources.md)', self.tree.page(out, 'index.md'))

    def test_overview_page_without_sidecars(self):
        page = self.tree.page(self.tree.generate(), 'data-sources.md')
        self.assertIn('None of the documented checks needs an elevated agent.', page)
        self.assertIn('No command has been documented yet.', page)

    def test_invalid_sidecar_fails_the_generator(self):
        self.tree.sidecar('CheckSample_check_both_sources.yaml', {'all': [{'source': 'x', 'privilege': 'conditional'}]})
        argv = ['--input', self.tree.root, '--output', os.path.join(self.tree.root, 'out'), '--samples', self.tree.root]
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()) as err:
            self.assertEqual(docs_generate.main(argv), 1)
        self.assertIn('needs a "note"', err.getvalue())


if __name__ == '__main__':
    unittest.main()
