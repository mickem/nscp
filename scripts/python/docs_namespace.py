#!/usr/bin/env python3
"""Which reference folder (namespace) a module's page lives in.

Kept out of docs_extract.py, which needs a running agent, so it can be
tested on its own (build/python/test_docs_namespace.py).
"""
import json
import os

# The folder follows the category the module declares in its module.json
# ("category"), the same value that groups it in the web UI and in the
# reference navigation (scripts/python/docs_nav.py). The folder is part of every
# reference page's URL, so the modules whose page already lived elsewhere keep
# it: NAMESPACE_OVERRIDES pins them, and a new module needs no entry here.
# (docs.py used to pick windows vs unix at runtime from sys.platform; the
# mapping is fixed so a module always lands in the same folder whichever
# platform's pipeline extracted it.)
CATEGORY_NAMESPACES = {
    'check': 'check',
    'database': 'check',
    'container': 'check',
    'server': 'client',
    'client': 'client',
    'script': 'generic',
    'scheduling': 'generic',
    'tool': 'generic',
}
NAMESPACE_OVERRIDES = {
    # Windows-only modules have always lived in windows/.
    'CheckActiveDirectory': 'windows',
    'CheckEventLog': 'windows',
    'CheckHyperV': 'windows',
    'CheckMSSQL': 'windows',
    'CheckTaskSched': 'windows',
    'CheckWMI': 'windows',
    'CheckWindowsApps': 'windows',
    'NSClientServer': 'windows',
    # Older than their category.
    'CheckExternalScripts': 'check',
    'WEBServer': 'generic',
}
IGNORED_MODULES = ['CauseCrashes', 'SamplePluginSimple']
# Where module.json files are looked for: the source tree's modules/. The
# extractor runs from a copy in the build tree, so it puts the folder next to
# its --output docs folder first (see docs_extract.py); this file's own place
# in the source tree is the fallback, for the tests.
MODULES_DIRS = [os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'modules'))]


def module_category(name):
    """The category in modules/<name>/module.json, or None (an out-of-tree module)."""
    for modules_dir in MODULES_DIRS:
        path = os.path.join(modules_dir, name, 'module.json')
        if not os.path.isfile(path):
            continue
        try:
            with open(path) as f:
                return json.load(f).get('module', {}).get('category')
        except (IOError, ValueError):
            return None
    return None


def classify_namespace(name):
    if name in NAMESPACE_OVERRIDES:
        return NAMESPACE_OVERRIDES[name]
    return CATEGORY_NAMESPACES.get(module_category(name), 'misc')
