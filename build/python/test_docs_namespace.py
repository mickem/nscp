#!/usr/bin/env python3
"""The reference folder each module's page lives in (docs_namespace.py).

The folder is part of every reference page's URL: a module documented today
must keep its folder, whatever its category says, and every module needs a
category that maps to one.
"""
import os
import sys
import unittest

import yaml

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'scripts', 'python'))

from docs_namespace import CATEGORY_NAMESPACES, IGNORED_MODULES, classify_namespace, module_category  # noqa: E402

REFERENCE = os.path.join(ROOT, 'docs', 'reference')


class NamespaceTest(unittest.TestCase):
    def test_a_documented_module_keeps_its_folder(self):
        moved = []
        for name in sorted(os.listdir(REFERENCE)):
            if not name.endswith('.yaml'):
                continue
            with open(os.path.join(REFERENCE, name)) as f:
                data = yaml.safe_load(f) or {}
            module = data.get('module')
            if module and classify_namespace(module) != data.get('namespace'):
                moved.append('%s: %s -> %s' % (module, data.get('namespace'), classify_namespace(module)))
        self.assertEqual([], moved)

    def test_every_module_has_a_category_with_a_folder(self):
        modules_dir = os.path.join(ROOT, 'modules')
        missing = [m for m in sorted(os.listdir(modules_dir))
                   if os.path.isfile(os.path.join(modules_dir, m, 'module.json'))
                   and m not in IGNORED_MODULES
                   and module_category(m) not in CATEGORY_NAMESPACES]
        self.assertEqual([], missing)

    def test_an_unknown_module_is_misc(self):
        self.assertEqual('misc', classify_namespace('NoSuchModule'))


if __name__ == '__main__':
    unittest.main()
