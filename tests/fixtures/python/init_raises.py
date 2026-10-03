# Fixture for tests/pythonscript-api.test.ts: init() registers one query and
# then raises. The module logs it and loads every other script regardless.
from NSCP import Registry, status


def init(plugin_id, plugin_alias, script_alias):
    Registry.get(plugin_id).simple_function('py_before_raise', lambda a: (status.OK, 'registered before the raise'),
                                            'Registered before init raised')
    raise RuntimeError('boom from init')
