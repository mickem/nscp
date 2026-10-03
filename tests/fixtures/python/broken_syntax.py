# Fixture for tests/pythonscript-api.test.ts: a script that does not parse.
# The module must log it and load every other script regardless.
from NSCP import Registry, status

def init(plugin_id, plugin_alias, script_alias)
    Registry.get(plugin_id).simple_function('py_never', lambda a: (status.OK, 'never'), 'never registered')
